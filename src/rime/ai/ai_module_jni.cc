//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-08-20 Librim AI Team
//
// RimeAi JNI 实现
// 实现 android/ai 的 RimeAi.kt 声明的全部 external 方法,
// 将调用映射到 C++ 侧 ProfileManager / SceneDetector / LLMEngine。
//
// 推理链路(真实):
//   aiRecognizeFeature/aiMatchFeatures/aiGenerateContent
//     -> ProfileManager -> PromptManager -> LLMEngine::Infer
//     -> [Android] LLMJNIBridge -> Java LlamaService -> llama.cpp (GGUF)
//     -> LLMNative.onInferenceResult 回调 -> 返回调用方
//
// 仅在 __ANDROID__ 下编译(桌面构建为空编译单元)。
//

#ifdef __ANDROID__

#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <sstream>
#include <string>
#include <vector>

#include "feature_types.h"
#include "llm_engine.h"
#include "llm_jni.h"
#include "profile_manager.h"
#include "scene_detector.h"
#include "feature_storage.h"

#define TAG "RimeAiJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define UNUSED(x) (void)(x)

namespace {

using rime::ai::DeviceCapability;
using rime::ai::FeatureCategory;
using rime::ai::FeatureStorage;
using rime::ai::LLMConfig;
using rime::ai::LLMEngine;
using rime::ai::LLMJNIBridge;
using rime::ai::LLMStatus;
using rime::ai::MatchResult;
using rime::ai::PersonalFeature;
using rime::ai::ProfileConfig;
using rime::ai::ProfileManager;
using rime::ai::RecognitionResult;
using rime::ai::SceneDetector;
using rime::ai::SceneInfo;
using rime::ai::SceneType;
using rime::ai::StorageConfig;

bool g_ai_initialized = false;

// ============================================================
// 轻量 JSON 工具(无第三方依赖)
// ============================================================

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((unsigned char)c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// 定位 "key": 之后值的起始位置;找不到返回 npos
size_t FindJsonValue(const std::string& json, const std::string& key) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  while (pos != std::string::npos) {
    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return std::string::npos;
    size_t v = colon + 1;
    while (v < json.size() && (json[v] == ' ' || json[v] == '\t')) v++;
    return v;
  }
  return std::string::npos;
}

std::string ExtractString(const std::string& json, const std::string& key,
                          const std::string& default_val = "") {
  size_t v = FindJsonValue(json, key);
  if (v == std::string::npos || v >= json.size() || json[v] != '"') {
    return default_val;
  }
  v++;
  std::string out;
  while (v < json.size() && json[v] != '"') {
    if (json[v] == '\\' && v + 1 < json.size()) {
      v++;
      switch (json[v]) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        default: out += json[v]; break;
      }
    } else {
      out += json[v];
    }
    v++;
  }
  return out;
}

double ExtractDouble(const std::string& json, const std::string& key, double default_val) {
  size_t v = FindJsonValue(json, key);
  if (v == std::string::npos || v >= json.size()) return default_val;
  char* end = nullptr;
  double d = strtod(json.c_str() + v, &end);
  return (end && end != json.c_str() + v) ? d : default_val;
}

int ExtractInt(const std::string& json, const std::string& key, int default_val) {
  return (int)ExtractDouble(json, key, default_val);
}

bool ExtractBool(const std::string& json, const std::string& key, bool default_val) {
  size_t v = FindJsonValue(json, key);
  if (v == std::string::npos || v >= json.size()) return default_val;
  if (json.compare(v, 4, "true") == 0) return true;
  if (json.compare(v, 5, "false") == 0) return false;
  return default_val;
}

// 拆分 JSON 数组中的顶层对象 [{...},{...}]
std::vector<std::string> SplitJsonObjects(const std::string& json) {
  std::vector<std::string> objects;
  int depth = 0;
  size_t start = 0;
  bool in_string = false;
  for (size_t i = 0; i < json.size(); i++) {
    char c = json[i];
    if (in_string) {
      if (c == '\\') {
        i++;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '{') {
      if (depth == 0) start = i;
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) {
        objects.push_back(json.substr(start, i - start + 1));
      }
    }
  }
  return objects;
}

// PersonalFeature -> 紧凑 JSON(与 RimeAi.kt FeatureInfo 字段对应)
std::string FeatureToJson(const PersonalFeature& f) {
  std::ostringstream oss;
  oss << "{\"id\":\"" << JsonEscape(f.id)
      << "\",\"category\":" << static_cast<int>(f.category)
      << ",\"key\":\"" << JsonEscape(f.key)
      << "\",\"value\":\"" << JsonEscape(f.value)
      << "\",\"confidence\":" << f.confidence
      << ",\"enabled\":" << (f.enabled ? "true" : "false") << "}";
  return oss.str();
}

// JSON -> PersonalFeature
PersonalFeature FeatureFromJson(const std::string& json) {
  PersonalFeature f;
  f.id = ExtractString(json, "id");
  f.category = static_cast<FeatureCategory>(ExtractInt(json, "category", 0));
  f.key = ExtractString(json, "key");
  f.value = ExtractString(json, "value");
  f.confidence = ExtractDouble(json, "confidence", 0.0);
  f.enabled = ExtractBool(json, "enabled", true);
  return f;
}

// JNI 字符串辅助
std::string JniToString(JNIEnv* env, jstring j_str) {
  if (!j_str) return "";
  const char* chars = env->GetStringUTFChars(j_str, nullptr);
  std::string s(chars ? chars : "");
  env->ReleaseStringUTFChars(j_str, chars);
  return s;
}

// LLMStatus -> RimeAi.kt AIState 枚举值
// (DISABLED=0, INITIALIZING=1, READY=2, INFERRING=3, ERROR=4)
jint AiStateFromLLMStatus(LLMStatus status) {
  switch (status) {
    case LLMStatus::kNormal:
    case LLMStatus::kDegraded:
      return 2;  // READY
    case LLMStatus::kNotLoaded:
      return 1;  // INITIALIZING(等待模型加载)
    case LLMStatus::kDisabled:
    default:
      return 0;  // DISABLED
  }
}

}  // namespace

extern "C" {

// ============================================================
// 初始化与关闭
// ============================================================

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiInit(JNIEnv* env, jobject thiz, jstring config_json) {
  UNUSED(thiz);
  std::string config = JniToString(env, config_json);

  // LLM 配置(model_path 指向 GGUF 文件)
  LLMConfig llm_config;
  llm_config.model_path = ExtractString(config, "model_path",
      "/data/data/com.osfans.trime/files/models/qwen3.5-2b-instruct-q4_k_m.gguf");
  llm_config.max_tokens = ExtractInt(config, "max_tokens", 512);
  llm_config.temperature = (float)ExtractDouble(config, "temperature", 0.3);
  llm_config.timeout_ms = (int64_t)ExtractDouble(config, "timeout_ms", 10000);
  llm_config.preload_model = ExtractBool(config, "preload_model", true);
  llm_config.threads = ExtractInt(config, "threads", 4);

  // 特征存储配置
  ProfileConfig profile_config;
  profile_config.min_text_length = ExtractInt(config, "min_text_length", 20);
  profile_config.min_confidence = ExtractDouble(config, "min_confidence", 0.6);

  StorageConfig storage_config;
  storage_config.db_path = ExtractString(config, "db_path",
      "/data/data/com.osfans.trime/files/features.db");

  // 初始化各子模块
  if (!ProfileManager::Instance().Initialize(profile_config, storage_config)) {
    LOGE("aiInit: ProfileManager initialization failed");
    return JNI_FALSE;
  }
  if (!SceneDetector::Instance().Initialize()) {
    LOGE("aiInit: SceneDetector initialization failed");
    return JNI_FALSE;
  }
  if (!LLMEngine::Instance().Initialize(llm_config)) {
    LOGE("aiInit: LLMEngine initialization failed");
    return JNI_FALSE;
  }

  g_ai_initialized = true;
  LOGI("aiInit: success (model=%s, db=%s)",
       llm_config.model_path.c_str(), storage_config.db_path.c_str());
  return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiShutdown(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  ProfileManager::Instance().Shutdown();
  LLMEngine::Instance().Shutdown();
  g_ai_initialized = false;
  LOGI("aiShutdown: done");
}

// ============================================================
// 状态查询
// ============================================================

JNIEXPORT jint JNICALL
Java_com_osfans_trime_core_RimeAi_aiGetState(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  return AiStateFromLLMStatus(LLMEngine::Instance().GetStatus());
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiIsReady(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  return LLMEngine::Instance().IsReady() ? JNI_TRUE : JNI_FALSE;
}

// ============================================================
// LLM 推理
// ============================================================

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiInfer(JNIEnv* env, jobject thiz, jstring prompt) {
  UNUSED(thiz);
  std::string cpp_prompt = JniToString(env, prompt);
  auto result = LLMEngine::Instance().Infer(cpp_prompt);

  std::ostringstream oss;
  oss << "{\"content\":\"" << JsonEscape(result.content)
      << "\",\"success\":" << (result.success ? "true" : "false")
      << ",\"elapsed_ms\":" << result.elapsed_ms;
  if (!result.error_message.empty()) {
    oss << ",\"error\":\"" << JsonEscape(result.error_message) << "\"";
  }
  oss << "}";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jlong JNICALL
Java_com_osfans_trime_core_RimeAi_aiInferAsync(JNIEnv* env, jobject thiz,
                                                jstring prompt, jobject callback) {
  UNUSED(thiz);
  std::string cpp_prompt = JniToString(env, prompt);

  // 保存回调的全局引用(回调发生在 LLMEngine 工作线程,
  // 经 LLMJNIBridge.GetJNIEnvForCallback 附加到 JVM)
  jobject global_callback = env->NewGlobalRef(callback);

  LLMEngine::Instance().InferAsync(
      cpp_prompt, [global_callback](const rime::ai::InferenceResult& result) {
        JNIEnv* cb_env = LLMJNIBridge::Instance().GetJNIEnvForCallback();
        if (!cb_env) {
          LOGE("aiInferAsync callback: no JNIEnv");
          return;
        }

        jclass callback_class = cb_env->GetObjectClass(global_callback);
        jmethodID on_result = cb_env->GetMethodID(
            callback_class, "onResult", "(Ljava/lang/String;JZLjava/lang/String;)V");
        if (!on_result) {
          LOGE("aiInferAsync callback: onResult not found");
          cb_env->DeleteLocalRef(callback_class);
          cb_env->DeleteGlobalRef(global_callback);
          return;
        }
        jstring content = cb_env->NewStringUTF(result.content.c_str());
        jstring error = result.error_message.empty()
                            ? nullptr
                            : cb_env->NewStringUTF(result.error_message.c_str());
        cb_env->CallVoidMethod(global_callback, on_result, content,
                               (jlong)result.elapsed_ms,
                               result.success ? JNI_TRUE : JNI_FALSE, error);
        cb_env->DeleteLocalRef(content);
        if (error) cb_env->DeleteLocalRef(error);
        cb_env->DeleteLocalRef(callback_class);
        cb_env->DeleteGlobalRef(global_callback);
      });

  return 0;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiCancel(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  LLMEngine::Instance().Cancel();
}

// ============================================================
// 模型管理
// ============================================================

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiLoadModel(JNIEnv* env, jobject thiz, jstring model_path) {
  UNUSED(thiz);
  std::string path = JniToString(env, model_path);

  LLMConfig config = LLMEngine::Instance().GetConfig();
  config.model_path = path;
  LLMEngine::Instance().SetConfig(config);

  return LLMEngine::Instance().LoadModel() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiUnloadModel(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  LLMEngine::Instance().UnloadModel();
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiIsModelLoaded(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  return LLMEngine::Instance().IsModelLoaded() ? JNI_TRUE : JNI_FALSE;
}

// ============================================================
// 特征识别 / 匹配 / 内容生成
// ============================================================

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiRecognizeFeature(JNIEnv* env, jobject thiz, jstring text) {
  UNUSED(thiz);
  std::string cpp_text = JniToString(env, text);

  RecognitionResult result = ProfileManager::Instance().RecognizeFeature(cpp_text);

  std::ostringstream oss;
  oss << "{\"hasFeature\":" << (result.has_feature ? "true" : "false")
      << ",\"featureId\":\"" << JsonEscape(result.feature.id) << "\""
      << ",\"category\":" << static_cast<int>(result.feature.category)
      << ",\"key\":\"" << JsonEscape(result.feature.key) << "\""
      << ",\"value\":\"" << JsonEscape(result.feature.value) << "\""
      << ",\"confidence\":" << result.feature.confidence
      << ",\"shouldPrompt\":" << (result.should_prompt_user ? "true" : "false")
      << ",\"suggestionText\":\"" << JsonEscape(result.suggestion_text) << "\"";
  if (!result.error_message.empty()) {
    oss << ",\"error\":\"" << JsonEscape(result.error_message) << "\"";
  }
  oss << "}";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiConfirmFeature(JNIEnv* env, jobject thiz, jstring feature_json) {
  UNUSED(thiz);
  std::string json = JniToString(env, feature_json);
  PersonalFeature feature = FeatureFromJson(json);
  return ProfileManager::Instance().ConfirmFeature(feature) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiIgnoreFeature(JNIEnv* env, jobject thiz, jstring feature_id) {
  UNUSED(thiz);
  std::string id = JniToString(env, feature_id);
  ProfileManager::Instance().IgnoreFeature(id);
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiMatchFeatures(JNIEnv* env, jobject thiz,
                                                   jstring input, jstring app_package) {
  UNUSED(thiz);
  std::string cpp_input = JniToString(env, input);
  std::string cpp_package = JniToString(env, app_package);

  MatchResult result = ProfileManager::Instance().MatchFeatures(cpp_input, cpp_package);

  std::ostringstream oss;
  oss << "{\"intent\":\"" << JsonEscape(result.intent) << "\""
      << ",\"matchedFeatures\":[";
  for (size_t i = 0; i < result.matched_features.size(); i++) {
    if (i > 0) oss << ",";
    oss << FeatureToJson(result.matched_features[i]);
  }
  oss << "]"
      << ",\"confidence\":" << result.overall_confidence << "}";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiGenerateContent(JNIEnv* env, jobject thiz,
                                                     jstring input, jstring features_json) {
  UNUSED(thiz);
  std::string cpp_input = JniToString(env, input);
  std::string cpp_features = JniToString(env, features_json);

  // 解析特征数组
  std::vector<PersonalFeature> features;
  for (const auto& obj : SplitJsonObjects(cpp_features)) {
    features.push_back(FeatureFromJson(obj));
  }

  // GenerateEnhancedContent 为异步接口,此处同步等待结果
  std::promise<std::string> promise;
  std::future<std::string> future = promise.get_future();
  ProfileManager::Instance().GenerateEnhancedContent(
      cpp_input, features,
      [&promise](const std::string& content) { promise.set_value(content); });

  std::string content;
  if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
    content = future.get();
  } else {
    content = "";
    LOGE("aiGenerateContent: timeout");
  }
  return env->NewStringUTF(content.c_str());
}

// ============================================================
// 特征管理
// ============================================================

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiGetAllFeatures(JNIEnv* env, jobject thiz) {
  UNUSED(thiz);
  auto features = ProfileManager::Instance().GetAllFeatures();
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < features.size(); i++) {
    if (i > 0) oss << ",";
    oss << FeatureToJson(features[i]);
  }
  oss << "]";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiGetFeaturesByCategory(JNIEnv* env, jobject thiz, jint category) {
  UNUSED(thiz);
  auto features = ProfileManager::Instance().GetFeaturesByCategory(
      static_cast<FeatureCategory>(category));
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < features.size(); i++) {
    if (i > 0) oss << ",";
    oss << FeatureToJson(features[i]);
  }
  oss << "]";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiSearchFeatures(JNIEnv* env, jobject thiz, jstring keyword) {
  UNUSED(thiz);
  std::string kw = JniToString(env, keyword);
  auto features = ProfileManager::Instance().SearchFeatures(kw);
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < features.size(); i++) {
    if (i > 0) oss << ",";
    oss << FeatureToJson(features[i]);
  }
  oss << "]";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiDeleteFeature(JNIEnv* env, jobject thiz, jstring feature_id) {
  UNUSED(thiz);
  std::string id = JniToString(env, feature_id);
  return ProfileManager::Instance().DeleteFeature(id) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiSetFeatureEnabled(JNIEnv* env, jobject thiz,
                                                       jstring feature_id, jboolean enabled) {
  UNUSED(thiz);
  std::string id = JniToString(env, feature_id);
  return ProfileManager::Instance().SetFeatureEnabled(id, enabled == JNI_TRUE) ? JNI_TRUE
                                                                               : JNI_FALSE;
}

// ============================================================
// 隐私模式
// ============================================================

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiSetPrivacyMode(JNIEnv* env, jobject thiz, jboolean enabled) {
  UNUSED(thiz);
  ProfileManager::Instance().SetPrivacyMode(enabled == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiIsPrivacyMode(JNIEnv* env, jobject thiz) {
  UNUSED(env);
  UNUSED(thiz);
  return ProfileManager::Instance().IsPrivacyMode() ? JNI_TRUE : JNI_FALSE;
}

// ============================================================
// 场景检测
// ============================================================

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_core_RimeAi_aiDetectScene(JNIEnv* env, jobject thiz, jstring app_package) {
  UNUSED(thiz);
  std::string package = JniToString(env, app_package);
  SceneInfo scene = SceneDetector::Instance().DetectScene(package);

  std::ostringstream oss;
  oss << "{\"type\":" << static_cast<int>(scene.type)
      << ",\"appPackage\":\"" << JsonEscape(scene.app_package) << "\""
      << ",\"appName\":\"" << JsonEscape(scene.app_name) << "\""
      << ",\"shouldEnhance\":" << (scene.should_enhance ? "true" : "false") << "}";
  return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiIsAiChatApp(JNIEnv* env, jobject thiz, jstring app_package) {
  UNUSED(thiz);
  std::string package = JniToString(env, app_package);
  return SceneDetector::Instance().IsAIChatApp(package) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_core_RimeAi_aiShouldEnhance(JNIEnv* env, jobject thiz, jstring app_package) {
  UNUSED(thiz);
  std::string package = JniToString(env, app_package);
  return SceneDetector::Instance().ShouldEnhance(package) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiRegisterAiApp(JNIEnv* env, jobject thiz,
                                                  jstring package_name, jstring display_name) {
  UNUSED(thiz);
  rime::ai::AppRegistry app;
  app.package_name = JniToString(env, package_name);
  app.display_name = JniToString(env, display_name);
  app.scene_type = SceneType::kCustom;
  app.is_ai_app = true;
  SceneDetector::Instance().RegisterApp(app);
}

JNIEXPORT void JNICALL
Java_com_osfans_trime_core_RimeAi_aiUnregisterAiApp(JNIEnv* env, jobject thiz, jstring package_name) {
  UNUSED(thiz);
  std::string package = JniToString(env, package_name);
  SceneDetector::Instance().UnregisterApp(package);
}

JNIEXPORT jobjectArray JNICALL
Java_com_osfans_trime_core_RimeAi_aiGetRegisteredAiApps(JNIEnv* env, jobject thiz) {
  UNUSED(thiz);
  auto apps = SceneDetector::Instance().GetRegisteredApps();
  jobjectArray result = env->NewObjectArray(
      (jsize)apps.size(), env->FindClass("java/lang/String"), nullptr);
  for (size_t i = 0; i < apps.size(); i++) {
    env->SetObjectArrayElement(result, (jsize)i, env->NewStringUTF(apps[i].package_name.c_str()));
  }
  return result;
}

}  // extern "C"

#endif  // __ANDROID__
