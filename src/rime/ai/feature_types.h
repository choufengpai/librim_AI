//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-06 Librim AI Team
//
#ifndef RIME_AI_FEATURE_TYPES_H_
#define RIME_AI_FEATURE_TYPES_H_

#include <string>
#include <cstdint>
#include <vector>

namespace rime {
namespace ai {

// 特征类别枚举
enum class FeatureCategory {
  kBasicInfo = 0,      // 基本情况：年龄、地区、职业等
  kPreferences = 1,    // 兴趣偏好：食物、娱乐、旅行等
  kHabits = 2,         // 生活习惯：作息、通勤、运动等
  kRecentEvents = 3,   // 近期事项：会议、旅行计划等
  kRelationships = 4   // 社交关系：人物关系描述
};

// LLM 引擎状态
enum class LLMStatus {
  kNormal,             // 正常运行
  kDegraded,           // 降级模式（推理慢/不稳定）
  kDisabled,           // 功能关闭
  kNotLoaded           // 模型未加载
};

// 推理结果
struct InferenceResult {
  bool success = false;
  std::string content;
  std::string error_message;
  int elapsed_ms = 0;
};

// 个人特征
struct PersonalFeature {
  std::string id;
  FeatureCategory category = FeatureCategory::kBasicInfo;
  std::string key;
  std::string value;
  double confidence = 0.0;
  std::string source_text;
  int64_t created_at = 0;
  int64_t updated_at = 0;
  bool enabled = true;

  PersonalFeature() = default;
  
  PersonalFeature(const std::string& id, FeatureCategory cat, 
                  const std::string& k, const std::string& v)
      : id(id), category(cat), key(k), value(v) {}
};

// 特征识别结果
struct RecognitionResult {
  bool has_feature = false;
  PersonalFeature feature;
  std::string error_message;
};

// 特征匹配结果
struct MatchResult {
  std::string intent;
  std::vector<PersonalFeature> matched_features;
  std::string error_message;
};

// 设备能力
struct DeviceCapability {
  bool has_gpu = false;
  int ram_mb = 0;
  bool is_arm64 = false;
  bool is_low_ram_device = false;
};

// LLM 配置
struct LLMConfig {
  std::string model_path;
  int threads = 4;
  int gpu_layers = 32;
  int max_tokens = 512;
  float temperature = 0.3f;
  float top_p = 0.9f;
  int timeout_ms = 10000;
  bool preload_model = true;
  int unload_after_idle_sec = 300;
};

// ============================================================
// 工具函数
// ============================================================

// 类别转字符串
inline const char* FeatureCategoryToString(FeatureCategory cat) {
  switch (cat) {
    case FeatureCategory::kBasicInfo:     return "basic_info";
    case FeatureCategory::kPreferences:   return "preferences";
    case FeatureCategory::kHabits:        return "habits";
    case FeatureCategory::kRecentEvents:  return "recent_events";
    case FeatureCategory::kRelationships: return "relationships";
    default: return "unknown";
  }
}

// 字符串转类别
inline FeatureCategory StringToFeatureCategory(const std::string& str) {
  if (str == "basic_info")      return FeatureCategory::kBasicInfo;
  if (str == "preferences")     return FeatureCategory::kPreferences;
  if (str == "habits")          return FeatureCategory::kHabits;
  if (str == "recent_events")   return FeatureCategory::kRecentEvents;
  if (str == "relationships")   return FeatureCategory::kRelationships;
  return FeatureCategory::kBasicInfo;
}

// 生成特征 ID
inline std::string GenerateFeatureId() {
  static int64_t counter = 0;
  return "feat_" + std::to_string(++counter) + "_" + 
         std::to_string(std::time(nullptr));
}

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_FEATURE_TYPES_H_