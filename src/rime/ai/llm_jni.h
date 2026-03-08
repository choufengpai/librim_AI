//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// LLM JNI 桥接层
// 提供 C++ 与 Java (MLC-LLM mlc4j) 之间的通信接口
//

#ifndef RIME_AI_LLM_JNI_H_
#define RIME_AI_LLM_JNI_H_

#include "feature_types.h"
#include <functional>
#include <string>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace rime {
namespace ai {

// ============================================================
// LLMJNI bridge - JNI 桥接器
// ============================================================

class LLMJNIBridge {
public:
  static LLMJNIBridge& Instance();
  
#ifdef __ANDROID__
  // 初始化 JNI 环境
  // @param vm Java 虚拟机指针
  // @return true 成功
  bool Initialize(JavaVM* vm);
  
  // 设置 MLC-LLM 服务实例
  // @param service MLCService Java 对象的全局引用
  void SetMLCService(jobject service);
  
  // 调用 Java 层推理
  // @param prompt 输入提示词
  // @param callback 结果回调（从 Java 层回调）
  void RequestInference(const std::string& prompt,
                        std::function<void(const InferenceResult&)> callback);
  
  // 从 Java 层接收推理结果
  // @param request_id 请求 ID
  // @param content 结果内容
  // @param elapsed_ms 耗时（毫秒）
  // @param success 是否成功
  // @param error 错误信息（如果有）
  void OnInferenceResult(int64_t request_id,
                         const std::string& content,
                         int64_t elapsed_ms,
                         bool success,
                         const std::string& error);
  
  // 检查模型是否就绪
  bool IsModelReady();
  
  // 加载模型
  // @param model_path 模型路径
  // @return true 成功
  bool LoadModel(const std::string& model_path);
  
  // 卸载模型
  void UnloadModel();
  
  // 获取设备能力
  DeviceCapability GetDeviceCapability();
  
  // JNI 回调注册
  void RegisterNativeMethods(JNIEnv* env);
#endif

private:
  LLMJNIBridge() = default;
  ~LLMJNIBridge() = default;
  
  // 禁止拷贝
  LLMJNIBridge(const LLMJNIBridge&) = delete;
  LLMJNIBridge& operator=(const LLMJNIBridge&) = delete;
  
#ifdef __ANDROID__
  // 获取当前 JNI 环境
  JNIEnv* GetJNIEnv();
  
  // Java 虚拟机指针
  JavaVM* java_vm_ = nullptr;
  
  // MLCService Java 对象
  jobject mlc_service_ = nullptr;
  
  // 推理回调映射
  std::unordered_map<int64_t, std::function<void(const InferenceResult&)>> callbacks_;
  std::mutex callbacks_mutex_;
  
  // 请求 ID 计数器
  std::atomic<int64_t> request_counter_{0};
  
  // 类引用
  jclass mlc_service_class_ = nullptr;
  jmethodID method_inference_ = nullptr;
  jmethodID method_is_ready_ = nullptr;
  jmethodID method_load_model_ = nullptr;
  jmethodID method_unload_model_ = nullptr;
#endif
};

// ============================================================
// C 风格 JNI 导出函数（供 Java 层调用）
// ============================================================

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __ANDROID__

// 初始化 LLM 引擎
JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_initEngine(JNIEnv* env, jobject thiz, jobject config);

// 关闭 LLM 引擎
JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_shutdownEngine(JNIEnv* env, jobject thiz);

// 同步推理
JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_infer(JNIEnv* env, jobject thiz, jstring prompt);

// 异步推理
JNIEXPORT jlong JNICALL
Java_com_osfans_trime_ai_LLMNative_inferAsync(JNIEnv* env, jobject thiz, 
                                               jstring prompt, 
                                               jobject callback);

// 取消推理
JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_cancel(JNIEnv* env, jobject thiz);

// 模型管理
JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_loadModel(JNIEnv* env, jobject thiz, jstring model_path);

JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_unloadModel(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_isModelLoaded(JNIEnv* env, jobject thiz);

// 状态查询
JNIEXPORT jint JNICALL
Java_com_osfans_trime_ai_LLMNative_getStatus(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_com_osfans_trime_ai_LLMNative_isReady(JNIEnv* env, jobject thiz);

// 设备能力
JNIEXPORT jobject JNICALL
Java_com_osfans_trime_ai_LLMNative_detectCapability(JNIEnv* env, jobject thiz);

// Prompt 管理
JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getRecognitionPrompt(JNIEnv* env, jobject thiz, jstring input);

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getMatchPrompt(JNIEnv* env, jobject thiz, 
                                                   jstring input, 
                                                   jstring features);

JNIEXPORT jstring JNICALL
Java_com_osfans_trime_ai_LLMNative_getGenerationPrompt(JNIEnv* env, jobject thiz,
                                                        jstring input,
                                                        jstring features);

// 推理结果回调（从 Java 层调用）
JNIEXPORT void JNICALL
Java_com_osfans_trime_ai_LLMNative_onInferenceResult(JNIEnv* env, jobject thiz,
                                                      jlong request_id,
                                                      jstring content,
                                                      jlong elapsed_ms,
                                                      jboolean success,
                                                      jstring error);

#endif  // __ANDROID__

#ifdef __cplusplus
}
#endif

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_LLM_JNI_H_