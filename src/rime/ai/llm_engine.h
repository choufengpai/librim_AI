//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-06 Librim AI Team
//
#ifndef RIME_AI_LLM_ENGINE_H_
#define RIME_AI_LLM_ENGINE_H_

#include "feature_types.h"
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

namespace rime {
namespace ai {

// 推理回调类型
using InferenceCallback = std::function<void(const InferenceResult&)>;

// 推理请求
struct InferenceRequest {
  std::string prompt;
  InferenceCallback callback;
  int64_t request_id;
  
  InferenceRequest() : request_id(0) {}
  InferenceRequest(const std::string& p, InferenceCallback cb, int64_t id)
      : prompt(p), callback(cb), request_id(id) {}
};

// LLM 引擎类
// 单例模式，管理模型加载、推理请求队列
class LLMEngine {
public:
  // 获取单例实例
  static LLMEngine& Instance();
  
  // 初始化引擎
  // @param config LLM 配置
  // @return true 成功，false 失败
  bool Initialize(const LLMConfig& config);
  
  // 关闭引擎
  void Shutdown();
  
  // 同步推理（阻塞调用）
  // @param prompt 输入提示词
  // @return 推理结果
  InferenceResult Infer(const std::string& prompt);
  
  // 异步推理（非阻塞）
  // @param prompt 输入提示词
  // @param callback 结果回调
  void InferAsync(const std::string& prompt, InferenceCallback callback);
  
  // 取消当前推理
  void Cancel();
  
  // 取消所有等待中的请求
  void CancelAll();
  
  // 模型管理
  bool LoadModel();
  void UnloadModel();
  bool IsModelLoaded() const;
  
  // 状态查询
  LLMStatus GetStatus() const;
  bool IsReady() const;
  
  // 配置
  void SetConfig(const LLMConfig& config);
  const LLMConfig& GetConfig() const;
  
  // 检测设备能力
  static DeviceCapability DetectCapability();
  
  // 空闲超时自动卸载
  void SetIdleTimeout(int seconds);
  
private:
  LLMEngine();
  ~LLMEngine();
  
  // 禁止拷贝
  LLMEngine(const LLMEngine&) = delete;
  LLMEngine& operator=(const LLMEngine&) = delete;
  
  // 工作线程
  void WorkerThread();
  
  // 处理单个推理请求
  InferenceResult ProcessRequest(const std::string& prompt);
  
  // 检查是否应该取消
  bool ShouldCancel() const;
  
  // 更新状态
  void UpdateStatus(LLMStatus status);
  
  // 空闲检查线程
  void IdleCheckThread();
  
  // 实现类（Pimpl 模式，隔离 MLC-LLM 依赖）
  struct Impl;
  std::unique_ptr<Impl> impl_;
  
  // 配置
  LLMConfig config_;
  
  // 状态
  std::atomic<LLMStatus> status_{LLMStatus::kNotLoaded};
  std::atomic<bool> cancel_flag_{false};
  std::atomic<bool> running_{false};
  
  // 请求队列
  std::queue<InferenceRequest> request_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  
  // 工作线程
  std::unique_ptr<std::thread> worker_thread_;
  
  // 空闲检查
  std::atomic<int64_t> last_activity_time_{0};
  std::unique_ptr<std::thread> idle_thread_;
  int idle_timeout_sec_ = 300;
  
  // 请求 ID 计数器
  std::atomic<int64_t> request_counter_{0};
  
  // 连续失败计数（用于降级）
  int consecutive_failures_{0};
  static constexpr int kMaxConsecutiveFailures = 5;
};

// ============================================================
// Prompt 模板管理
// ============================================================

class PromptManager {
public:
  static PromptManager& Instance();
  
  // 获取特征识别 Prompt
  std::string GetRecognitionPrompt(const std::string& user_input);
  
  // 获取特征匹配 Prompt
  std::string GetMatchPrompt(const std::string& user_input,
                             const std::string& personal_features);
  
  // 获取内容生成 Prompt
  std::string GetGenerationPrompt(const std::string& original_input,
                                  const std::string& selected_features);
  
  // 自定义 Prompt 模板
  void SetRecognitionTemplate(const std::string& tmpl);
  void SetMatchTemplate(const std::string& tmpl);
  void SetGenerationTemplate(const std::string& tmpl);

private:
  PromptManager() = default;
  
  std::string recognition_template_;
  std::string match_template_;
  std::string generation_template_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_LLM_ENGINE_H_