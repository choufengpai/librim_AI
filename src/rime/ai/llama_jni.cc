//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-08-20 Librim AI Team
//
// llama.cpp JNI 桥接层
// 为 Java 层 LlamaService 提供 GGUF 模型加载与推理能力
// 桥接模式参考 llama.cpp 官方 llama-android 示例
//
// 集成说明:
// - 仅在 __ANDROID__ 下编译(桌面构建为空编译单元)
// - llama.cpp 源码位于仓库根目录 llama.cpp/(vendored),静态链接进 librime
// - ChatML 模板兼容 Qwen2/Qwen2.5/Qwen3/Qwen3.5 系列
//

#ifdef __ANDROID__

#include <jni.h>
#include <android/log.h>

#include "llama.h"
// 注: 新版 llama.cpp 采样 API 已并入 llama.h(无独立 llama-sampling.h)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define TAG "LlamaJNI"
#define UNUSED(x) (void)(x)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

namespace {

std::mutex g_llama_mutex;
llama_model* g_model = nullptr;
llama_context* g_ctx = nullptr;
std::string g_model_path;
std::atomic<bool> g_cancel{false};
bool g_backend_initialized = false;

// Qwen 系列 ChatML 对话模板
// Qwen2/Qwen2.5/Qwen3/Qwen3.5 均使用 <|im_start|>...<|im_end|> 格式
// no_think: Qwen3 起的软开关,user 消息末尾附加 " /no_think" 关闭思考模式(低延迟场景)
std::string BuildChatMLPrompt(const std::string& system_prompt,
                              const std::string& user_prompt,
                              bool no_think) {
  std::string prompt;
  prompt.reserve(system_prompt.size() + user_prompt.size() + 96);
  if (!system_prompt.empty()) {
    prompt += "<|im_start|>system\n";
    prompt += system_prompt;
    prompt += "<|im_end|>\n";
  }
  prompt += "<|im_start|>user\n";
  prompt += user_prompt;
  if (no_think) {
    prompt += " /no_think";
  }
  prompt += "<|im_end|>\n<|im_start|>assistant\n";
  if (no_think) {
    // Qwen3 系列 /no_think 软开关对部分模型版本不生效(实测输出仍以 <think> 开头);
    // 预置已闭合的空思考块,等效官方 chat template 的 enable_thinking=False,
    // 模型会直接输出答案而不进入思考
    prompt += "<think>\n\n</think>\n\n";
  }
  return prompt;
}

void ThrowJavaRuntimeException(JNIEnv* env, const std::string& message) {
  if (env->ExceptionCheck()) {
    return;
  }
  jclass ex_class = env->FindClass("java/lang/RuntimeException");
  if (ex_class) {
    env->ThrowNew(ex_class, message.c_str());
    env->DeleteLocalRef(ex_class);
  }
}

// TokenCallback.onToken(Ljava/lang/String;)V
void CallOnToken(JNIEnv* env, jobject callback, const std::string& piece) {
  jclass cb_class = env->GetObjectClass(callback);
  if (!cb_class) return;
  jmethodID on_token = env->GetMethodID(cb_class, "onToken", "(Ljava/lang/String;)V");
  if (on_token) {
    jstring j_piece = env->NewStringUTF(piece.c_str());
    env->CallVoidMethod(callback, on_token, j_piece);
    env->DeleteLocalRef(j_piece);
  }
  env->DeleteLocalRef(cb_class);
}

}  // namespace

extern "C" {

// ============================================================
// 模型管理
// ============================================================

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeLoadModel(JNIEnv* env, jobject thiz,
                                                      jstring j_path,
                                                      jint context_length,
                                                      jint threads,
                                                      jint gpu_layers) {
  UNUSED(thiz);
  std::lock_guard<std::mutex> lock(g_llama_mutex);

  const char* path = env->GetStringUTFChars(j_path, nullptr);
  std::string model_path(path ? path : "");
  env->ReleaseStringUTFChars(j_path, path);

  if (model_path.empty()) {
    LOGE("nativeLoadModel: empty model path");
    return JNI_FALSE;
  }

  // 已加载同一模型则直接复用
  if (g_model && g_ctx && model_path == g_model_path) {
    LOGI("nativeLoadModel: model already loaded: %s", model_path.c_str());
    return JNI_TRUE;
  }

  // 卸载旧模型
  if (g_ctx) {
    llama_free(g_ctx);
    g_ctx = nullptr;
  }
  if (g_model) {
    llama_model_free(g_model);
    g_model = nullptr;
  }
  g_model_path.clear();

  if (!g_backend_initialized) {
    llama_backend_init();
    g_backend_initialized = true;
  }

  auto t0 = std::chrono::steady_clock::now();

  llama_model_params mparams = llama_model_default_params();
  mparams.n_gpu_layers = gpu_layers;  // 0 = 纯 CPU

  g_model = llama_model_load_from_file(model_path.c_str(), mparams);
  if (!g_model) {
    LOGE("nativeLoadModel: failed to load model: %s", model_path.c_str());
    return JNI_FALSE;
  }

  llama_context_params cparams = llama_context_default_params();
  cparams.n_ctx = context_length > 0 ? context_length : 2048;
  cparams.n_threads = threads > 0 ? threads : 4;
  // prefill(prompt 处理,决定首 token 延迟)用满全部核心;
  // decode 阶段保持性能核线程数,避免小核拖慢逐 token 生成
  const int hw_threads = (int)std::thread::hardware_concurrency();
  cparams.n_threads_batch = hw_threads > 0 ? std::min(hw_threads, 8) : cparams.n_threads;
  // KV cache 量化 F16 -> Q8_0: 缓存读写减半,显著降低纯 CPU 推理的内存带宽压力
  // (与 llama.cpp 官方 llama-android 示例一致,需 Flash Attention,默认 AUTO 已启用)
  cparams.type_k = GGML_TYPE_Q8_0;
  cparams.type_v = GGML_TYPE_Q8_0;

  g_ctx = llama_init_from_model(g_model, cparams);
  if (!g_ctx) {
    LOGE("nativeLoadModel: failed to create context");
    llama_model_free(g_model);
    g_model = nullptr;
    return JNI_FALSE;
  }

  g_model_path = model_path;
  auto t1 = std::chrono::steady_clock::now();
  auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  LOGI("nativeLoadModel: loaded %s in %lld ms (ctx=%d, threads=%d/%d, gpu_layers=%d, kv=Q8_0)",
       model_path.c_str(), (long long)load_ms, cparams.n_ctx,
       cparams.n_threads, cparams.n_threads_batch, gpu_layers);

  // 预热: 加载后立即跑一次微型推理,把 mmap 页与计算图预热好,
  // 避免用户第一次请求承担冷启动成本
  auto w0 = std::chrono::steady_clock::now();
  const llama_vocab* vocab = llama_model_get_vocab(g_model);
  const char* warm_text = "\xe4\xbd\xa0\xe5\xa5\xbd";  // "你好"
  llama_token warm_tokens[8];
  int n_warm = llama_tokenize(vocab, warm_text, (int)strlen(warm_text),
                              warm_tokens, 8, true, false);
  if (n_warm > 0) {
    llama_batch warm_batch = llama_batch_get_one(warm_tokens, n_warm);
    if (llama_decode(g_ctx, warm_batch) == 0) {
      llama_sampler* warm_smpl =
          llama_sampler_chain_init(llama_sampler_chain_default_params());
      llama_sampler_chain_add(warm_smpl, llama_sampler_init_greedy());
      llama_token warm_next = llama_sampler_sample(warm_smpl, g_ctx, -1);
      llama_batch gen_batch = llama_batch_get_one(&warm_next, 1);
      llama_decode(g_ctx, gen_batch);  // prefill + decode 全链路预热
      llama_sampler_free(warm_smpl);
    }
    // 预热产生的 KV 立即清空,不影响真实对话
    llama_memory_seq_rm(llama_get_memory(g_ctx), 0, 0, -1);
  }
  auto w1 = std::chrono::steady_clock::now();
  LOGI("nativeLoadModel: warmup done in %lld ms",
       (long long)std::chrono::duration_cast<std::chrono::milliseconds>(w1 - w0).count());
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeIsLoaded(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  std::lock_guard<std::mutex> lock(g_llama_mutex);
  return (g_model && g_ctx) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeFree(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  std::lock_guard<std::mutex> lock(g_llama_mutex);
  if (g_ctx) {
    llama_free(g_ctx);
    g_ctx = nullptr;
  }
  if (g_model) {
    llama_model_free(g_model);
    g_model = nullptr;
  }
  g_model_path.clear();
  LOGI("nativeFree: model released");
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeGetModelInfo(JNIEnv* env, jobject thiz) {
  UNUSED(thiz);
  std::lock_guard<std::mutex> lock(g_llama_mutex);
  if (!g_model) {
    return env->NewStringUTF("{}");
  }
  char desc[256];
  llama_model_desc(g_model, desc, sizeof(desc));
  // 长度单位为 MiB
  long long size_mib = (long long)(llama_model_size(g_model) / (1024.0 * 1024.0));
  int n_ctx = llama_n_ctx(g_ctx);
  std::string info = "{\"model_desc\": \"" + std::string(desc) +
                     "\", \"model_size_mib\": " + std::to_string(size_mib) +
                     ", \"n_ctx\": " + std::to_string(n_ctx) +
                     ", \"n_vocab\": " + std::to_string(llama_vocab_n_tokens(llama_model_get_vocab(g_model))) + "}";
  return env->NewStringUTF(info.c_str());
}

// ============================================================
// 推理
// ============================================================

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeGenerate(JNIEnv* env, jobject thiz,
                                                     jstring j_prompt,
                                                     jstring j_system_prompt,
                                                     jint max_tokens,
                                                     jfloat temperature,
                                                     jfloat top_p,
                                                     jboolean no_think,
                                                     jobject callback) {
  UNUSED(thiz);
  std::lock_guard<std::mutex> lock(g_llama_mutex);

  if (!g_model || !g_ctx) {
    ThrowJavaRuntimeException(env, "Model not loaded");
    return nullptr;
  }

  const char* prompt_str = env->GetStringUTFChars(j_prompt, nullptr);
  std::string prompt(prompt_str ? prompt_str : "");
  env->ReleaseStringUTFChars(j_prompt, prompt_str);

  const char* system_str =
      j_system_prompt ? env->GetStringUTFChars(j_system_prompt, nullptr) : nullptr;
  std::string system_prompt(system_str ? system_str : "");
  if (j_system_prompt) {
    env->ReleaseStringUTFChars(j_system_prompt, system_str);
  }

  // 拼装 ChatML 模板(Qwen 系列)
  std::string full_prompt = BuildChatMLPrompt(system_prompt, prompt, no_think == JNI_TRUE);

  // tokenize(不添加 BOS,解析特殊 token 如 <|im_start|>)
  const llama_vocab* vocab = llama_model_get_vocab(g_model);
  const int n_prompt = -llama_tokenize(vocab, full_prompt.c_str(), (int32_t)full_prompt.size(),
                                       nullptr, 0, false, true);
  if (n_prompt < 0) {
    ThrowJavaRuntimeException(env, "Failed to tokenize prompt");
    return nullptr;
  }
  std::vector<llama_token> tokens(n_prompt);
  if (llama_tokenize(vocab, full_prompt.c_str(), (int32_t)full_prompt.size(),
                     tokens.data(), (int32_t)tokens.size(), false, true) < 0) {
    ThrowJavaRuntimeException(env, "Failed to tokenize prompt");
    return nullptr;
  }

  // 清空 KV cache(单轮对话)
  llama_memory_seq_rm(llama_get_memory(g_ctx), 0, 0, -1);

  // 采样链: top_p -> temp -> dist
  auto sparams = llama_sampler_chain_default_params();
  llama_sampler* smpl = llama_sampler_chain_init(sparams);
  if (top_p < 1.0f) {
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(top_p, 1));
  }
  llama_sampler_chain_add(smpl, llama_sampler_init_temp(temperature));
  llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

  g_cancel = false;
  std::string output;
  output.reserve(1024);

  // 首批: 整个 prompt
  llama_batch batch = llama_batch_get_one(tokens.data(), (int32_t)tokens.size());
  char piece[128];
  int generated = 0;
  const int token_limit = max_tokens > 0 ? max_tokens : 512;

  while (generated < token_limit && !g_cancel.load()) {
    // 检查 Kotlin 侧异常(如协程取消)
    if (env->ExceptionCheck()) {
      break;
    }

    if (llama_decode(g_ctx, batch)) {
      LOGE("nativeGenerate: llama_decode failed");
      llama_sampler_free(smpl);
      ThrowJavaRuntimeException(env, "llama_decode failed");
      return nullptr;
    }

    // 采样下一个 token
    llama_token new_token = llama_sampler_sample(smpl, g_ctx, -1);

    // 终止符(<|im_end|> / <|endoftext|>)
    if (llama_vocab_is_eog(vocab, new_token)) {
      break;
    }

    // token -> 文本片段(含特殊 token 的渲染)
    int n = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, true);
    if (n > 0) {
      output.append(piece, n);
      if (callback) {
        CallOnToken(env, callback, std::string(piece, n));
      }
    }

    generated++;

    // 下一批: 单 token
    batch = llama_batch_get_one(&new_token, 1);
  }

  llama_sampler_free(smpl);

  // 清空 KV cache,为下一轮对话释放显存/内存
  llama_memory_seq_rm(llama_get_memory(g_ctx), 0, 0, -1);

  // 剥离 Qwen3/3.5 思考块 <think>...</think>(no_think 未生效时的兑底,
  // 保证返回纯答案文本)
  size_t think_end = output.find("</think>");
  if (think_end != std::string::npos) {
    output = output.substr(think_end + 8);
    size_t first_non_ws = output.find_first_not_of(" \n\r\t");
    output = (first_non_ws == std::string::npos) ? "" : output.substr(first_non_ws);
  } else if (output.rfind("<think>", 0) == 0) {
    // think 块未闭合(被 token 上限截断):整体丢弃,避免把思考片段当结果返回
    output.clear();
  }

  if (g_cancel.load()) {
    LOGI("nativeGenerate: cancelled after %d tokens", generated);
  } else {
    LOGI("nativeGenerate: generated %d tokens", generated);
  }

  return env->NewStringUTF(output.c_str());
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LlamaService_nativeCancel(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  g_cancel = true;
  LOGI("nativeCancel: cancellation requested");
}

}  // extern "C"

#endif  // __ANDROID__
