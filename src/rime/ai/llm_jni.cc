//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// LLM JNI 桥接层实现
//

#include "llm_jni.h"
#include "llm_engine.h"
#include <mutex>
#include <unordered_map>

#ifdef __ANDROID__
#include <android/log.h>

#define LOG_TAG "LibrimAI_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGE(...)
#define LOGW(...)
#endif

namespace rime {
namespace ai {

// ============================================================
// LLMJNIBridge 实现
// ============================================================

LLMJNIBridge& LLMJNIBridge::Instance() {
  static LLMJNIBridge instance;
  return instance;
}

#ifdef __ANDROID__

JNIEnv* LLMJNIBridge::GetJNIEnv() {
  if (!java_vm_) {
    LOGE("JavaVM not initialized");
    return nullptr;
  }
  
  JNIEnv* env = nullptr;
  jint result = java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  
  if (result == JNI_EDETACHED) {
    // 当前线程未附加到 JVM，需要附加
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = const_cast<char*>("LibrimAI_JNI_Thread");
    args.group = nullptr;
    
    if (java_vm_->AttachCurrentThread(&env, &args) != JNI_OK) {
      LOGE("Failed to attach thread to JVM");
      return nullptr;
    }
  } else if (result != JNI_OK) {
    LOGE("Failed to get JNIEnv");
    return nullptr;
  }
  
  return env;
}

bool LLMJNIBridge::Initialize(JavaVM* vm) {
  if (java_vm_) {
    LOGI("LLMJNIBridge already initialized");
    return true;
  }
  
  java_vm_ = vm;
  
  JNIEnv* env = GetJNIEnv();
  if (!env) {
    LOGE("Failed to get JNIEnv");
    return false;
  }
  
  // 缓存 MLCService 类和方法
  jclass service_class = env->FindClass("com/osfans/trime/ai/MLCService");
  if (service_class) {
    mlc_service_class_ = reinterpret_cast<jclass>(env->NewGlobalRef(service_class));
    
    method_inference_ = env->GetMethodID(mlc_service_class_, "inference",
                                          "(Ljava/lang/String;J)V");
    method_is_ready_ = env->GetMethodID(mlc_service_class_, "isModelReady", "()Z");
    method_load_model_ = env->GetMethodID(mlc_service_class_, "loadModel",
                                           "(Ljava/lang/String;)Z");
    method_unload_model_ = env->GetMethodID(mlc_service_class_, "unloadModel", "()V");
    
    env->DeleteLocalRef(service_class);
  }
  
  LOGI("LLMJNIBridge initialized successfully");
  return true;
}

void LLMJNIBridge::SetMLCService(jobject service) {
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  
  if (mlc_service_) {
    env->DeleteGlobalRef(mlc_service_);
  }
  
  mlc_service_ = env->NewGlobalRef(service);
  LOGI("MLCService instance set");
}

void LLMJNIBridge::RequestInference(const std::string& prompt,
                                     std::function<void(const InferenceResult&)> callback) {
  JNIEnv* env = GetJNIEnv();
  if (!env || !mlc_service_ || !method_inference_) {
    LOGE("Cannot request inference: JNI not properly initialized");
    InferenceResult result;
    result.success = false;
    result.error_message = "JNI not initialized";
    if (callback) callback(result);
    return;
  }
  
  int64_t request_id = ++request_counter_;
  
  // 保存回调
  {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_[request_id] = callback;
  }
  
  // 调用 Java 方法
  jstring j_prompt = env->NewStringUTF(prompt.c_str());
  env->CallVoidMethod(mlc_service_, method_inference_, j_prompt, (jlong)request_id);
  env->DeleteLocalRef(j_prompt);
  
  LOGI("Inference request sent, id=%lld", (long long)request_id);
}

void LLMJNIBridge::OnInferenceResult(int64_t request_id,
                                      const std::string& content,
                                      int64_t elapsed_ms,
                                      bool success,
                                      const std::string& error) {
  std::function<void(const InferenceResult&)> callback;
  
  {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto it = callbacks_.find(request_id);
    if (it != callbacks_.end()) {
      callback = it->second;
      callbacks_.erase(it);
    }
  }
  
  if (callback) {
    InferenceResult result;
    result.content = content;
    result.elapsed_ms = elapsed_ms;
    result.success = success;
    result.error_message = error;
    callback(result);
  }
  
  LOGI("Inference result received, id=%lld, success=%d", (long long)request_id, success);
}

bool LLMJNIBridge::IsModelReady() {
  JNIEnv* env = GetJNIEnv();
  if (!env || !mlc_service_ || !method_is_ready_) {
    return false;
  }
  
  return env->CallBooleanMethod(mlc_service_, method_is_ready_);
}

bool LLMJNIBridge::LoadModel(const std::string& model_path) {
  JNIEnv* env = GetJNIEnv();
  if (!env || !mlc_service_ || !method_load_model_) {
    LOGE("Cannot load model: JNI not properly initialized");
    return false;
  }
  
  jstring j_path = env->NewStringUTF(model_path.c_str());
  bool result = env->CallBooleanMethod(mlc_service_, method_load_model_, j_path);
  env->DeleteLocalRef(j_path);
  
  return result;
}

void LLMJNIBridge::UnloadModel() {
  JNIEnv* env = GetJNIEnv();
  if (!env || !mlc_service_ || !method_unload_model_) {
    return;
  }
  
  env->CallVoidMethod(mlc_service_, method_unload_model_);
}

DeviceCapability LLMJNIBridge::GetDeviceCapability() {
  // TODO: 通过 JNI 调用 Android API 获取设备信息
  DeviceCapability cap;
  cap.is_arm64 = true;  // 大多数 Android 设备
  cap.ram_mb = 4096;
  cap.has_gpu = true;
  cap.is_low_ram_device = false;
  return cap;
}

void LLMJNIBridge::RegisterNativeMethods(JNIEnv* env) {
  // 注册由 JNI_OnLoad 或 Java 层调用
  LOGI("Native methods registered");
}

#endif  // __ANDROID__

// ============================================================
// JNI 导出函数实现
// ============================================================

#ifdef __ANDROID__

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_initEngine(JNIEnv* env, jobject thiz, jobject config) {
  // 解析配置
  LLMConfig llm_config;
  
  jclass config_class = env->GetObjectClass(config);
  
  jfieldID fid_model_path = env->GetFieldID(config_class, "modelPath", "Ljava/lang/String;");
  jfieldID fid_timeout = env->GetFieldID(config_class, "timeoutMs", "J");
  jfieldID fid_preload = env->GetFieldID(config_class, "preloadModel", "Z");
  jfieldID fid_max_tokens = env->GetFieldID(config_class, "maxTokens", "I");
  
  if (fid_model_path) {
    jstring j_path = (jstring)env->GetObjectField(config, fid_model_path);
    if (j_path) {
      const char* path = env->GetStringUTFChars(j_path, nullptr);
      llm_config.model_path = path;
      env->ReleaseStringUTFChars(j_path, path);
      env->DeleteLocalRef(j_path);
    }
  }
  
  if (fid_timeout) {
    llm_config.timeout_ms = env->GetLongField(config, fid_timeout);
  }
  
  if (fid_preload) {
    llm_config.preload_model = env->GetBooleanField(config, fid_preload);
  }
  
  if (fid_max_tokens) {
    llm_config.max_tokens = env->GetIntField(config, fid_max_tokens);
  }
  
  env->DeleteLocalRef(config_class);
  
  // 初始化引擎
  return LLMEngine::Instance().Initialize(llm_config);
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_shutdownEngine(JNIEnv* env, jobject thiz) {
  LLMEngine::Instance().Shutdown();
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_infer(JNIEnv* env, jobject thiz, jstring prompt) {
  const char* prompt_str = env->GetStringUTFChars(prompt, nullptr);
  std::string cpp_prompt(prompt_str);
  env->ReleaseStringUTFChars(prompt, prompt_str);
  
  InferenceResult result = LLMEngine::Instance().Infer(cpp_prompt);
  
  // 构建结果 JSON
  std::string result_json = "{\"content\":\"" + result.content + 
                            "\",\"success\":" + (result.success ? "true" : "false") +
                            ",\"elapsed_ms\":" + std::to_string(result.elapsed_ms);
  if (!result.error_message.empty()) {
    result_json += ",\"error\":\"" + result.error_message + "\"";
  }
  result_json += "}";
  
  return env->NewStringUTF(result_json.c_str());
}

JNIEXPORT jlong JNICALL
Java_com_osfans_trime_ai_LLMNative_inferAsync(JNIEnv* env, jobject thiz,
                                               jstring prompt, jobject callback) {
  const char* prompt_str = env->GetStringUTFChars(prompt, nullptr);
  std::string cpp_prompt(prompt_str);
  env->ReleaseStringUTFChars(prompt, prompt_str);
  
  // 保存 Java 回调对象的全局引用
  jobject global_callback = env->NewGlobalRef(callback);
  
  // 创建 C++ 回调
  auto cpp_callback = [env, global_callback](const InferenceResult& result) {
    jclass callback_class = env->GetObjectClass(global_callback);
    jmethodID on_result = env->GetMethodID(callback_class, "onResult",
                                            "(Ljava/lang/String;JZLjava/lang/String;)V");
    
    jstring content = env->NewStringUTF(result.content.c_str());
    jstring error = result.error_message.empty() ? nullptr : 
                    env->NewStringUTF(result.error_message.c_str());
    
    env->CallVoidMethod(global_callback, on_result, content,
                        (jlong)result.elapsed_ms, result.success, error);
    
    env->DeleteLocalRef(content);
    if (error) env->DeleteLocalRef(error);
    env->DeleteLocalRef(callback_class);
    env->DeleteGlobalRef(global_callback);
  };
  
  // 使用 LLMJNIBridge 请求推理
  int64_t request_id = LLMJNIBridge::Instance().request_counter_;
  LLMJNIBridge::Instance().RequestInference(cpp_prompt, cpp_callback);
  
  return (jlong)request_id;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_cancel(JNIEnv* env, jobject thiz) {
  LLMEngine::Instance().Cancel();
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_loadModel(JNIEnv* env, jobject thiz, jstring model_path) {
  const char* path = env->GetStringUTFChars(model_path, nullptr);
  std::string cpp_path(path);
  env->ReleaseStringUTFChars(model_path, path);
  
  LLMConfig config;
  config.model_path = cpp_path;
  LLMEngine::Instance().SetConfig(config);
  
  return LLMEngine::Instance().LoadModel();
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_unloadModel(JNIEnv* env, jobject thiz) {
  LLMEngine::Instance().UnloadModel();
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_isModelLoaded(JNIEnv* env, jobject thiz) {
  return LLMEngine::Instance().IsModelLoaded();
}

JNIEXPORT jint JNICALL
Java_com_osfans_trime_ai_LLMNative_getStatus(JNIEnv* env, jobject thiz) {
  return static_cast<jint>(LLMEngine::Instance().GetStatus());
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_isReady(JNIEnv* env, jobject thiz) {
  return LLMEngine::Instance().IsReady();
}

JNIEXPORT jobject JNICALL
Java_com_osfans_trime_ai_LLMNative_detectCapability(JNIEnv* env, jobject thiz) {
  DeviceCapability cap = LLMEngine::DetectCapability();
  
  jclass cap_class = env->FindClass("com/osfans/trime/ai/DeviceCapability");
  jmethodID constructor = env->GetMethodID(cap_class, "<init>", "(ZIZ)V");
  
  jobject result = env->NewObject(cap_class, constructor,
                                   cap.has_gpu, cap.ram_mb, cap.is_low_ram_device);
  
  env->DeleteLocalRef(cap_class);
  return result;
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getRecognitionPrompt(JNIEnv* env, jobject thiz, jstring input) {
  const char* input_str = env->GetStringUTFChars(input, nullptr);
  std::string cpp_input(input_str);
  env->ReleaseStringUTFChars(input, input_str);
  
  std::string prompt = PromptManager::Instance().GetRecognitionPrompt(cpp_input);
  return env->NewStringUTF(prompt.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getMatchPrompt(JNIEnv* env, jobject thiz,
                                                   jstring input, jstring features) {
  const char* input_str = env->GetStringUTFChars(input, nullptr);
  const char* features_str = env->GetStringUTFChars(features, nullptr);
  
  std::string cpp_input(input_str);
  std::string cpp_features(features_str);
  
  env->ReleaseStringUTFChars(input, input_str);
  env->ReleaseStringUTFChars(features, features_str);
  
  std::string prompt = PromptManager::Instance().GetMatchPrompt(cpp_input, cpp_features);
  return env->NewStringUTF(prompt.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getGenerationPrompt(JNIEnv* env, jobject thiz,
                                                        jstring input, jstring features) {
  const char* input_str = env->GetStringUTFChars(input, nullptr);
  const char* features_str = env->GetStringUTFChars(features, nullptr);
  
  std::string cpp_input(input_str);
  std::string cpp_features(features_str);
  
  env->ReleaseStringUTFChars(input, input_str);
  env->ReleaseStringUTFChars(features, features_str);
  
  std::string prompt = PromptManager::Instance().GetGenerationPrompt(cpp_input, cpp_features);
  return env->NewStringUTF(prompt.c_str());
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_onInferenceResult(JNIEnv* env, jobject thiz,
                                                      jlong request_id,
                                                      jstring content,
                                                      jlong elapsed_ms,
                                                      jboolean success,
                                                      jstring error) {
  const char* content_str = env->GetStringUTFChars(content, nullptr);
  const char* error_str = error ? env->GetStringUTFChars(error, nullptr) : "";
  
  LLMJNIBridge::Instance().OnInferenceResult(
      request_id,
      std::string(content_str),
      elapsed_ms,
      success == JNI_TRUE,
      std::string(error_str)
  );
  
  env->ReleaseStringUTFChars(content, content_str);
  if (error) env->ReleaseStringUTFChars(error, error_str);
}

#endif  // __ANDROID__

}  // namespace ai
}  // namespace rime