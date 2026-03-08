//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 特征管理器
// 协调特征识别、存储、查询的顶层管理器
//

#ifndef RIME_AI_PROFILE_MANAGER_H_
#define RIME_AI_PROFILE_MANAGER_H_

#include "feature_types.h"
#include "feature_storage.h"
#include "llm_engine.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rime {
namespace ai {

// ============================================================
// 配置结构
// ============================================================

struct ProfileConfig {
  // 特征识别配置
  int min_text_length = 20;           // 最小文本长度（触发识别）
  int debounce_ms = 500;              // 防抖时间（毫秒）
  double min_confidence = 0.6;        // 最小置信度阈值
  
  // 排除规则
  bool exclude_passwords = true;      // 排除密码字段
  bool exclude_credit_cards = true;   // 排除信用卡号
  bool exclude_ids = true;            // 排除身份证号
  
  // 存储配置
  int max_features = 1000;            // 最大特征数量
  int expire_days = 90;               // 过期天数
  
  // UI 配置
  int ui_cooldown_ms = 30000;         // UI 提示冷却时间
  int max_suggestions_per_day = 10;   // 每日最大建议次数
};

// ============================================================
// 识别结果
// ============================================================

struct RecognitionResult {
  bool has_feature = false;
  PersonalFeature feature;
  std::string suggestion_text;        // 给用户的建议文本
  bool should_prompt_user = false;    // 是否应该提示用户
};

// ============================================================
// 匹配结果
// ============================================================

struct MatchResult {
  std::string intent;                 // 识别的意图
  std::vector<PersonalFeature> matched_features;
  std::vector<std::pair<std::string, double>> feature_relevance; // 特征ID -> 相关度
  double overall_confidence = 0.0;
};

// ============================================================
// 回调类型
// ============================================================

using RecognitionCallback = std::function<void(const RecognitionResult&)>;
using MatchCallback = std::function<void(const MatchResult&)>;
using GenerateCallback = std::function<void(const std::string&)>;

// ============================================================
// ProfileManager - 特征管理器
// ============================================================

class ProfileManager {
public:
  // 获取单例实例
  static ProfileManager& Instance();
  
  // 初始化
  bool Initialize(const ProfileConfig& config, const StorageConfig& storage_config);
  
  // 关闭
  void Shutdown();
  
  // ============================================================
  // 特征识别
  // ============================================================
  
  // 同步识别（阻塞）
  RecognitionResult RecognizeFeature(const std::string& text);
  
  // 异步识别（非阻塞）
  void RecognizeFeatureAsync(const std::string& text, RecognitionCallback callback);
  
  // 确认保存特征
  bool ConfirmFeature(const PersonalFeature& feature);
  
  // 忽略特征（不保存）
  void IgnoreFeature(const std::string& feature_id);
  
  // ============================================================
  // 特征匹配（智能增强）
  // ============================================================
  
  // 同步匹配
  MatchResult MatchFeatures(const std::string& input, const std::string& app_package);
  
  // 异步匹配
  void MatchFeaturesAsync(const std::string& input, 
                          const std::string& app_package,
                          MatchCallback callback);
  
  // ============================================================
  // 内容生成
  // ============================================================
  
  // 生成增强内容
  void GenerateEnhancedContent(const std::string& original_input,
                               const std::vector<PersonalFeature>& selected_features,
                               GenerateCallback callback);
  
  // ============================================================
  // 特征管理
  // ============================================================
  
  // 获取所有特征
  std::vector<PersonalFeature> GetAllFeatures();
  
  // 按类别获取特征
  std::vector<PersonalFeature> GetFeaturesByCategory(FeatureCategory category);
  
  // 搜索特征
  std::vector<PersonalFeature> SearchFeatures(const std::string& keyword);
  
  // 删除特征
  bool DeleteFeature(const std::string& id);
  
  // 更新特征
  bool UpdateFeature(const PersonalFeature& feature);
  
  // 启用/禁用特征
  bool SetFeatureEnabled(const std::string& id, bool enabled);
  
  // ============================================================
  // 统计与维护
  // ============================================================
  
  // 获取特征统计
  struct FeatureStats {
    int total_count = 0;
    int enabled_count = 0;
    std::vector<std::pair<FeatureCategory, int>> count_by_category;
    int64_t last_recognition_time = 0;
    int recognitions_today = 0;
  };
  
  FeatureStats GetStats();
  
  // 清理过期特征
  int CleanupExpiredFeatures();
  
  // 导出/导入
  std::string ExportFeatures();
  bool ImportFeatures(const std::string& json);
  
  // ============================================================
  // 隐私模式控制
  // ============================================================
  
  void SetPrivacyMode(bool enabled);
  bool IsPrivacyMode() const;
  
  // ============================================================
  // 排除规则检查
  // ============================================================
  
  bool ShouldExclude(const std::string& text);
  
private:
  ProfileManager();
  ~ProfileManager();
  
  // 禁止拷贝
  ProfileManager(const ProfileManager&) = delete;
  ProfileManager& operator=(const ProfileManager&) = delete;
  
  // 内部方法
  bool IsPassword(const std::string& text);
  bool IsCreditCard(const std::string& text);
  bool IsIdNumber(const std::string& text);
  
  // 解析 LLM 返回结果
  bool ParseRecognitionResult(const std::string& llm_response, RecognitionResult* out);
  bool ParseMatchResult(const std::string& llm_response, MatchResult* out);
  
  // 防抖处理
  bool ShouldProcess(const std::string& text);
  void UpdateLastProcessTime();
  
  // 检查 UI 冷却
  bool IsUICooldownActive();
  void UpdateUICooldown();
  
  // 生成特征 ID
  std::string GenerateFeatureId();
  
  // 成员变量
  ProfileConfig config_;
  bool initialized_ = false;
  bool privacy_mode_ = false;
  
  std::mutex mutex_;
  
  // 防抖
  std::string last_processed_text_;
  int64_t last_process_time_ = 0;
  
  // UI 冷却
  int64_t last_ui_prompt_time_ = 0;
  int prompts_today_ = 0;
  int64_t day_start_time_ = 0;
  
  // 统计
  int64_t total_recognitions_ = 0;
  int64_t successful_recognitions_ = 0;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_PROFILE_MANAGER_H_