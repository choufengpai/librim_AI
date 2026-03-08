//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 特征管理器实现
//

#include "profile_manager.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <random>
#include <regex>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "LibrimAI_Profile"
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
// ProfileManager 单例实现
// ============================================================

ProfileManager& ProfileManager::Instance() {
  static ProfileManager instance;
  return instance;
}

ProfileManager::ProfileManager() = default;

ProfileManager::~ProfileManager() {
  Shutdown();
}

// ============================================================
// 初始化与关闭
// ============================================================

bool ProfileManager::Initialize(const ProfileConfig& config, 
                                 const StorageConfig& storage_config) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (initialized_) {
    LOGI("ProfileManager already initialized");
    return true;
  }
  
  config_ = config;
  
  // 初始化存储层
  if (!FeatureStorage::Instance().Open(storage_config)) {
    LOGE("Failed to open feature storage");
    return false;
  }
  
  initialized_ = true;
  LOGI("ProfileManager initialized successfully");
  return true;
}

void ProfileManager::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!initialized_) return;
  
  FeatureStorage::Instance().Close();
  initialized_ = false;
  LOGI("ProfileManager shutdown");
}

// ============================================================
// 特征识别
// ============================================================

RecognitionResult ProfileManager::RecognizeFeature(const std::string& text) {
  RecognitionResult result;
  
  if (!initialized_ || privacy_mode_) {
    return result;
  }
  
  // 检查文本长度
  if (static_cast<int>(text.length()) < config_.min_text_length) {
    return result;
  }
  
  // 排除规则检查
  if (ShouldExclude(text)) {
    LOGI("Text excluded by rules");
    return result;
  }
  
  // 防抖检查
  if (!ShouldProcess(text)) {
    return result;
  }
  
  // 获取 Prompt
  std::string prompt = PromptManager::Instance().GetRecognitionPrompt(text);
  
  // 调用 LLM 推理
  InferenceResult llm_result = LLMEngine::Instance().Infer(prompt);
  
  if (!llm_result.success) {
    LOGE("LLM inference failed: %s", llm_result.error_message.c_str());
    return result;
  }
  
  // 解析结果
  if (!ParseRecognitionResult(llm_result.content, &result)) {
    LOGW("Failed to parse LLM response");
    return result;
  }
  
  // 更新统计
  total_recognitions_++;
  if (result.has_feature) {
    successful_recognitions_++;
    
    // 检查是否应该提示用户
    if (result.feature.confidence >= config_.min_confidence && 
        !IsUICooldownActive() &&
        prompts_today_ < config_.max_suggestions_per_day) {
      result.should_prompt_user = true;
      
      // 生成建议文本
      std::ostringstream oss;
      oss << "检测到个人特征：\n"
          << result.feature.key << ": " << result.feature.value << "\n"
          << "是否保存以便智能输入增强？";
      result.suggestion_text = oss.str();
    }
  }
  
  UpdateLastProcessTime();
  return result;
}

void ProfileManager::RecognizeFeatureAsync(const std::string& text, 
                                            RecognitionCallback callback) {
  if (!initialized_ || privacy_mode_) {
    if (callback) {
      RecognitionResult result;
      callback(result);
    }
    return;
  }
  
  // 在工作线程中处理
  std::thread([this, text, callback]() {
    RecognitionResult result = RecognizeFeature(text);
    if (callback) {
      callback(result);
    }
  }).detach();
}

bool ProfileManager::ConfirmFeature(const PersonalFeature& feature) {
  if (!initialized_) return false;
  
  // 设置时间戳
  PersonalFeature saved_feature = feature;
  if (saved_feature.id.empty()) {
    saved_feature.id = GenerateFeatureId();
  }
  saved_feature.created_at = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  saved_feature.updated_at = saved_feature.created_at;
  saved_feature.enabled = true;
  
  // 保存到存储
  bool success = FeatureStorage::Instance().SaveFeature(saved_feature);
  
  if (success) {
    UpdateUICooldown();
    prompts_today_++;
    LOGI("Feature saved: %s", saved_feature.id.c_str());
  }
  
  return success;
}

void ProfileManager::IgnoreFeature(const std::string& feature_id) {
  UpdateUICooldown();
  prompts_today_++;
  LOGI("Feature ignored: %s", feature_id.c_str());
}

// ============================================================
// 特征匹配
// ============================================================

MatchResult ProfileManager::MatchFeatures(const std::string& input, 
                                           const std::string& app_package) {
  MatchResult result;
  
  if (!initialized_) return result;
  
  // 获取所有启用的特征
  FeatureQueryOptions options;
  options.enabled_only = true;
  auto features = FeatureStorage::Instance().QueryFeatures(options);
  
  if (features.empty()) {
    return result;
  }
  
  // 构建特征摘要
  std::ostringstream features_json;
  features_json << "[";
  for (size_t i = 0; i < features.size(); ++i) {
    if (i > 0) features_json << ",";
    features_json << "{\"id\":\"" << features[i].id 
                  << "\",\"key\":\"" << features[i].key 
                  << "\",\"value\":\"" << features[i].value << "\"}";
  }
  features_json << "]";
  
  // 获取匹配 Prompt
  std::string prompt = PromptManager::Instance().GetMatchPrompt(input, features_json.str());
  
  // 调用 LLM
  InferenceResult llm_result = LLMEngine::Instance().Infer(prompt);
  
  if (!llm_result.success) {
    LOGE("LLM match inference failed: %s", llm_result.error_message.c_str());
    return result;
  }
  
  // 解析结果
  if (ParseMatchResult(llm_result.content, &result)) {
    // 填充完整特征信息
    std::unordered_map<std::string, PersonalFeature> feature_map;
    for (const auto& f : features) {
      feature_map[f.id] = f;
    }
    
    for (const auto& pair : result.feature_relevance) {
      auto it = feature_map.find(pair.first);
      if (it != feature_map.end()) {
        result.matched_features.push_back(it->second);
      }
    }
  }
  
  return result;
}

void ProfileManager::MatchFeaturesAsync(const std::string& input,
                                         const std::string& app_package,
                                         MatchCallback callback) {
  if (!initialized_) {
    if (callback) {
      MatchResult result;
      callback(result);
    }
    return;
  }
  
  std::thread([this, input, app_package, callback]() {
    MatchResult result = MatchFeatures(input, app_package);
    if (callback) {
      callback(result);
    }
  }).detach();
}

// ============================================================
// 内容生成
// ============================================================

void ProfileManager::GenerateEnhancedContent(const std::string& original_input,
                                              const std::vector<PersonalFeature>& selected_features,
                                              GenerateCallback callback) {
  if (!initialized_ || !callback) return;
  
  // 构建特征文本
  std::ostringstream features_text;
  for (size_t i = 0; i < selected_features.size(); ++i) {
    if (i > 0) features_text << "\n";
    features_text << selected_features[i].key << ": " << selected_features[i].value;
  }
  
  // 获取生成 Prompt
  std::string prompt = PromptManager::Instance().GetGenerationPrompt(
      original_input, features_text.str());
  
  // 异步调用 LLM
  LLMEngine::Instance().InferAsync(prompt, [callback](const InferenceResult& result) {
    if (result.success) {
      // 解析生成结果
      // TODO: 解析 JSON 获取 enhanced_content
      callback(result.content);
    } else {
      callback("");
    }
  });
}

// ============================================================
// 特征管理
// ============================================================

std::vector<PersonalFeature> ProfileManager::GetAllFeatures() {
  if (!initialized_) return {};
  
  FeatureQueryOptions options;
  return FeatureStorage::Instance().QueryFeatures(options);
}

std::vector<PersonalFeature> ProfileManager::GetFeaturesByCategory(FeatureCategory category) {
  if (!initialized_) return {};
  
  return FeatureStorage::Instance().GetFeaturesByCategory(category);
}

std::vector<PersonalFeature> ProfileManager::SearchFeatures(const std::string& keyword) {
  if (!initialized_) return {};
  
  return FeatureStorage::Instance().SearchFeatures(keyword);
}

bool ProfileManager::DeleteFeature(const std::string& id) {
  if (!initialized_) return false;
  
  return FeatureStorage::Instance().DeleteFeature(id);
}

bool ProfileManager::UpdateFeature(const PersonalFeature& feature) {
  if (!initialized_) return false;
  
  PersonalFeature updated = feature;
  updated.updated_at = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  return FeatureStorage::Instance().SaveFeature(updated);
}

bool ProfileManager::SetFeatureEnabled(const std::string& id, bool enabled) {
  if (!initialized_) return false;
  
  PersonalFeature feature;
  if (!FeatureStorage::Instance().GetFeature(id, &feature)) {
    return false;
  }
  
  feature.enabled = enabled;
  return UpdateFeature(feature);
}

// ============================================================
// 统计与维护
// ============================================================

ProfileManager::FeatureStats ProfileManager::GetStats() {
  FeatureStats stats;
  
  if (!initialized_) return stats;
  
  stats.total_count = FeatureStorage::Instance().GetFeatureCount();
  stats.count_by_category = FeatureStorage::Instance().GetFeatureCountByCategory();
  stats.last_recognition_time = last_process_time_;
  stats.recognitions_today = prompts_today_;
  
  // 计算启用数量
  for (const auto& pair : stats.count_by_category) {
    stats.enabled_count += pair.second;
  }
  
  return stats;
}

int ProfileManager::CleanupExpiredFeatures() {
  if (!initialized_) return 0;
  
  return FeatureStorage::Instance().CleanupExpiredFeatures();
}

std::string ProfileManager::ExportFeatures() {
  if (!initialized_) return "{}";
  
  return FeatureStorage::Instance().ExportToJson();
}

bool ProfileManager::ImportFeatures(const std::string& json) {
  if (!initialized_) return false;
  
  return FeatureStorage::Instance().ImportFromJson(json);
}

// ============================================================
// 隐私模式
// ============================================================

void ProfileManager::SetPrivacyMode(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  privacy_mode_ = enabled;
  LOGI("Privacy mode: %s", enabled ? "enabled" : "disabled");
}

bool ProfileManager::IsPrivacyMode() const {
  return privacy_mode_;
}

// ============================================================
// 排除规则
// ============================================================

bool ProfileManager::ShouldExclude(const std::string& text) {
  if (config_.exclude_passwords && IsPassword(text)) {
    return true;
  }
  if (config_.exclude_credit_cards && IsCreditCard(text)) {
    return true;
  }
  if (config_.exclude_ids && IsIdNumber(text)) {
    return true;
  }
  return false;
}

bool ProfileManager::IsPassword(const std::string& text) {
  // 简单启发式：检查是否像密码
  // 包含大小写、数字、特殊字符的组合
  bool has_upper = false, has_lower = false, has_digit = false, has_special = false;
  
  for (char c : text) {
    if (std::isupper(c)) has_upper = true;
    else if (std::islower(c)) has_lower = true;
    else if (std::isdigit(c)) has_digit = true;
    else has_special = true;
  }
  
  // 如果包含3种以上字符类型且长度在8-32之间，可能是密码
  int types = (has_upper ? 1 : 0) + (has_lower ? 1 : 0) + 
              (has_digit ? 1 : 0) + (has_special ? 1 : 0);
  
  return types >= 3 && text.length() >= 8 && text.length() <= 32 && 
         text.find(' ') == std::string::npos;
}

bool ProfileManager::IsCreditCard(const std::string& text) {
  // 移除空格和横线
  std::string cleaned;
  for (char c : text) {
    if (std::isdigit(c)) cleaned += c;
  }
  
  // 检查是否是13-19位数字（信用卡号长度）
  if (cleaned.length() < 13 || cleaned.length() > 19) {
    return false;
  }
  
  // Luhn 算法校验
  int sum = 0;
  bool alternate = false;
  for (int i = cleaned.length() - 1; i >= 0; --i) {
    int digit = cleaned[i] - '0';
    if (alternate) {
      digit *= 2;
      if (digit > 9) digit -= 9;
    }
    sum += digit;
    alternate = !alternate;
  }
  
  return (sum % 10) == 0;
}

bool ProfileManager::IsIdNumber(const std::string& text) {
  // 中国身份证号：18位数字（最后一位可能是X）
  static std::regex cn_id_pattern(R"(\d{17}[\dXx])");
  
  // 简单检查
  std::string cleaned;
  for (char c : text) {
    if (std::isdigit(c) || c == 'X' || c == 'x') {
      cleaned += c;
    }
  }
  
  return std::regex_match(cleaned, cn_id_pattern);
}

// ============================================================
// 内部方法
// ============================================================

bool ProfileManager::ParseRecognitionResult(const std::string& llm_response, 
                                              RecognitionResult* out) {
  if (!out) return false;
  
  // 简单 JSON 解析（实际项目应使用 JSON 库）
  // 查找 has_feature
  if (llm_response.find("\"has_feature\": true") != std::string::npos ||
      llm_response.find("\"has_feature\":true") != std::string::npos) {
    out->has_feature = true;
    
    // 提取 feature 信息
    auto extract_string = [&llm_response](const std::string& key) -> std::string {
      std::string search = "\"" + key + "\":";
      size_t pos = llm_response.find(search);
      if (pos == std::string::npos) return "";
      
      pos = llm_response.find("\"", pos + search.length());
      if (pos == std::string::npos) return "";
      
      size_t end = llm_response.find("\"", pos + 1);
      if (end == std::string::npos) return "";
      
      return llm_response.substr(pos + 1, end - pos - 1);
    };
    
    auto extract_number = [&llm_response](const std::string& key) -> double {
      std::string search = "\"" + key + "\":";
      size_t pos = llm_response.find(search);
      if (pos == std::string::npos) return 0.0;
      
      pos += search.length();
      while (pos < llm_response.length() && (llm_response[pos] == ' ' || llm_response[pos] == '\t')) {
        pos++;
      }
      
      size_t end = pos;
      while (end < llm_response.length() && (std::isdigit(llm_response[end]) || llm_response[end] == '.')) {
        end++;
      }
      
      return std::stod(llm_response.substr(pos, end - pos));
    };
    
    out->feature.category = FeatureCategory::kPreferences; // 默认
    out->feature.key = extract_string("key");
    out->feature.value = extract_string("value");
    out->feature.confidence = extract_number("confidence");
    
    // 解析类别
    std::string category = extract_string("category");
    if (category == "basic_info" || category == "基本情况") {
      out->feature.category = FeatureCategory::kBasicInfo;
    } else if (category == "preferences" || category == "兴趣偏好") {
      out->feature.category = FeatureCategory::kPreferences;
    } else if (category == "habits" || category == "生活习惯") {
      out->feature.category = FeatureCategory::kHabits;
    } else if (category == "recent_events" || category == "近期事项") {
      out->feature.category = FeatureCategory::kRecentEvents;
    } else if (category == "relationships" || category == "社交关系") {
      out->feature.category = FeatureCategory::kRelationships;
    }
  }
  
  return true;
}

bool ProfileManager::ParseMatchResult(const std::string& llm_response, 
                                       MatchResult* out) {
  if (!out) return false;
  
  // 简单解析
  // TODO: 使用 JSON 库完善解析
  
  // 提取 intent
  auto intent_pos = llm_response.find("\"intent\":");
  if (intent_pos != std::string::npos) {
    size_t start = llm_response.find("\"", intent_pos + 9);
    size_t end = llm_response.find("\"", start + 1);
    if (start != std::string::npos && end != std::string::npos) {
      out->intent = llm_response.substr(start + 1, end - start - 1);
    }
  }
  
  out->overall_confidence = 0.8; // 默认置信度
  
  return true;
}

bool ProfileManager::ShouldProcess(const std::string& text) {
  // 检查是否与上次相同
  if (text == last_processed_text_) {
    return false;
  }
  
  // 检查防抖时间
  if (config_.debounce_ms > 0) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (now - last_process_time_ < config_.debounce_ms) {
      return false;
    }
  }
  
  return true;
}

void ProfileManager::UpdateLastProcessTime() {
  last_processed_text_ = "";
  last_process_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  // 检查是否需要重置每日计数
  auto now = std::chrono::system_clock::now();
  auto today = std::chrono::floor<std::chrono::days>(now);
  auto today_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      today.time_since_epoch()).count();
  
  if (day_start_time_ != today_ms) {
    day_start_time_ = today_ms;
    prompts_today_ = 0;
  }
}

bool ProfileManager::IsUICooldownActive() {
  if (config_.ui_cooldown_ms <= 0) return false;
  
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  return (now - last_ui_prompt_time_) < config_.ui_cooldown_ms;
}

void ProfileManager::UpdateUICooldown() {
  last_ui_prompt_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string ProfileManager::GenerateFeatureId() {
  // 生成 UUID 风格的 ID
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  
  const char* hex = "0123456789abcdef";
  std::string id;
  
  for (int i = 0; i < 32; ++i) {
    if (i == 8 || i == 12 || i == 16 || i == 20) {
      id += '-';
    }
    id += hex[dis(gen)];
  }
  
  return id;
}

}  // namespace ai
}  // namespace rime