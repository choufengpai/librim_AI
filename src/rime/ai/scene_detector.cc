//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 场景识别器实现
//

#include "scene_detector.h"
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "LibrimAI_Scene"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGE(...)
#endif

namespace rime {
namespace ai {

// ============================================================
// 预置 AI 应用列表
// ============================================================

namespace preset {

// 国际 AI 应用
constexpr const char* kChatGPT = "com.openai.chatgpt";
constexpr const char* kClaude = "com.anthropic.claude";
constexpr const char* kPerplexity = "com.perplexity.android";
constexpr const char* kPoe = "com.quora.poe";
constexpr const char* kGemini = "com.google.android.apps.bard";
constexpr const char* kCopilot = "com.microsoft.copilot";

// 国内 AI 应用
constexpr const char* kErnieBot = "com.baidu.erniebot";        // 文心一言
constexpr const char* kTongYiQianWen = "com.alibaba.tongyi";   // 通义千问
constexpr const char* kKimi = "com.moonshot.kimi";             // Kimi
constexpr const char* kDoubao = "com.larus.doubao";            // 豆包
constexpr const char* kZhiPu = "com.zhipu.ai";                 // 智谱清言
constexpr const char* kXunFei = "com.iflytek.xfyun";           // 讯飞星火
constexpr const char* kTiangong = "com.tiangong.ai";           // 天工
constexpr const char* kYuanBao = "com.tencent.yuanbao";        // 元宝
constexpr const char* kCoze = "com.coze.android";              // Coze

// 所有预置 AI 应用
const char* kAllAIApps[] = {
    kChatGPT, kClaude, kPerplexity, kPoe, kGemini, kCopilot,
    kErnieBot, kTongYiQianWen, kKimi, kDoubao, kZhiPu,
    kXunFei, kTiangong, kYuanBao, kCoze
};

// 应用包名到显示名称的映射
struct AppInfo {
    const char* package;
    const char* display_name;
    SceneType scene_type;
};

const AppInfo kAppInfoList[] = {
    // 国际 AI 应用
    {kChatGPT, "ChatGPT", SceneType::kAIChat},
    {kClaude, "Claude", SceneType::kAIChat},
    {kPerplexity, "Perplexity", SceneType::kAIChat},
    {kPoe, "Poe", SceneType::kAIChat},
    {kGemini, "Gemini", SceneType::kAIChat},
    {kCopilot, "Copilot", SceneType::kAIChat},
    
    // 国内 AI 应用
    {kErnieBot, "文心一言", SceneType::kAIChat},
    {kTongYiQianWen, "通义千问", SceneType::kAIChat},
    {kKimi, "Kimi", SceneType::kAIChat},
    {kDoubao, "豆包", SceneType::kAIChat},
    {kZhiPu, "智谱清言", SceneType::kAIChat},
    {kXunFei, "讯飞星火", SceneType::kAIChat},
    {kTiangong, "天工", SceneType::kAIChat},
    {kYuanBao, "元宝", SceneType::kAIChat},
    {kCoze, "Coze", SceneType::kAIChat},
    
    // 社交应用
    {"com.tencent.mm", "微信", SceneType::kSocial},
    {"com.tencent.mobileqq", "QQ", SceneType::kSocial},
    {"com.immomo.momo", "陌陌", SceneType::kSocial},
    
    // 浏览器
    {"com.android.chrome", "Chrome", SceneType::kBrowser},
    {"com.tencent.mtt", "QQ浏览器", SceneType::kBrowser},
    {"com.baidu.searchbox", "百度", SceneType::kBrowser},
    
    // 笔记应用
    {"com.evernote", "Evernote", SceneType::kNote},
    {"com.intsig.notes", "有道云笔记", SceneType::kNote},
    
    // 邮件应用
    {"com.google.android.gm", "Gmail", SceneType::kEmail},
    {"com.tencent.email", "QQ邮箱", SceneType::kEmail},
};

}  // namespace preset

// ============================================================
// SceneDetector 单例实现
// ============================================================

SceneDetector& SceneDetector::Instance() {
  static SceneDetector instance;
  return instance;
}

SceneDetector::SceneDetector() {
  // 初始化默认场景配置
  scene_configs_[SceneType::kAIChat] = {
    .auto_match_features = true,
    .show_suggestion_card = true,
    .min_input_length = 5,
    .keywords = {"帮我", "请", "能不能", "怎么"}
  };
  
  scene_configs_[SceneType::kSearch] = {
    .auto_match_features = true,
    .show_suggestion_card = false,
    .min_input_length = 3,
    .keywords = {}
  };
  
  scene_configs_[SceneType::kSocial] = {
    .auto_match_features = true,
    .show_suggestion_card = true,
    .min_input_length = 10,
    .keywords = {}
  };
}

bool SceneDetector::Initialize() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (initialized_) {
    return true;
  }
  
  LoadPresetApps();
  initialized_ = true;
  LOGI("SceneDetector initialized with %zu apps", app_registry_.size());
  return true;
}

// ============================================================
// 场景识别
// ============================================================

SceneInfo SceneDetector::DetectScene(const std::string& package_name) {
  SceneInfo info;
  info.app_package = package_name;
  
  auto it = app_registry_.find(package_name);
  if (it != app_registry_.end()) {
    info.type = it->second.scene_type;
    info.app_name = it->second.display_name;
    info.should_enhance = it->second.enabled && it->second.is_ai_app;
    
    // 获取场景配置
    auto config_it = scene_configs_.find(info.type);
    if (config_it != scene_configs_.end()) {
      info.config = config_it->second;
    }
    
    // AI 应用描述
    if (it->second.is_ai_app) {
      info.description = "AI 对话应用 - 启用智能增强";
    }
  } else {
    info.type = SceneType::kUnknown;
    info.app_name = package_name;
    info.should_enhance = false;
  }
  
  return info;
}

bool SceneDetector::IsAIChatApp(const std::string& package_name) {
  auto it = app_registry_.find(package_name);
  if (it != app_registry_.end()) {
    return it->second.is_ai_app;
  }
  return false;
}

bool SceneDetector::ShouldEnhance(const std::string& package_name) {
  auto it = app_registry_.find(package_name);
  if (it != app_registry_.end()) {
    return it->second.enabled && it->second.is_ai_app;
  }
  return false;
}

SceneInfo SceneDetector::GetSceneInfo(const std::string& package_name) {
  return DetectScene(package_name);
}

// ============================================================
// 应用注册管理
// ============================================================

void SceneDetector::RegisterApp(const AppRegistry& app) {
  std::lock_guard<std::mutex> lock(mutex_);
  app_registry_[app.package_name] = app;
  LOGI("App registered: %s (%s)", app.package_name.c_str(), app.display_name.c_str());
}

void SceneDetector::RegisterApps(const std::vector<AppRegistry>& apps) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& app : apps) {
    app_registry_[app.package_name] = app;
  }
  LOGI("%zu apps registered", apps.size());
}

void SceneDetector::UnregisterApp(const std::string& package_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  app_registry_.erase(package_name);
  LOGI("App unregistered: %s", package_name.c_str());
}

std::vector<AppRegistry> SceneDetector::GetRegisteredApps() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AppRegistry> result;
  for (const auto& pair : app_registry_) {
    result.push_back(pair.second);
  }
  return result;
}

std::vector<AppRegistry> SceneDetector::GetAppsByType(SceneType type) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AppRegistry> result;
  for (const auto& pair : app_registry_) {
    if (pair.second.scene_type == type) {
      result.push_back(pair.second);
    }
  }
  return result;
}

// ============================================================
// 场景配置
// ============================================================

void SceneDetector::SetSceneConfig(SceneType type, const SceneInfo::Config& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  scene_configs_[type] = config;
}

SceneInfo::Config SceneDetector::GetSceneConfig(SceneType type) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = scene_configs_.find(type);
  if (it != scene_configs_.end()) {
    return it->second;
  }
  return SceneInfo::Config{};
}

void SceneDetector::SetAppEnhancement(const std::string& package_name, bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = app_registry_.find(package_name);
  if (it != app_registry_.end()) {
    it->second.enabled = enabled;
    LOGI("App enhancement %s: %s", enabled ? "enabled" : "disabled", package_name.c_str());
  }
}

// ============================================================
// 预置应用
// ============================================================

std::vector<std::string> SceneDetector::GetPresetAIApps() {
  std::vector<std::string> result;
  for (const char* pkg : preset::kAllAIApps) {
    result.push_back(pkg);
  }
  return result;
}

bool SceneDetector::IsPresetAIApp(const std::string& package_name) {
  return preset_ai_apps_.count(package_name) > 0;
}

// ============================================================
// 私有方法
// ============================================================

void SceneDetector::LoadPresetApps() {
  // 加载预置 AI 应用
  for (const char* pkg : preset::kAllAIApps) {
    preset_ai_apps_.insert(pkg);
    
    AppRegistry app;
    app.package_name = pkg;
    app.scene_type = SceneType::kAIChat;
    app.is_ai_app = true;
    app.enabled = true;
    
    // 查找显示名称
    for (const auto& info : preset::kAppInfoList) {
      if (std::string(info.package) == pkg) {
        app.display_name = info.display_name;
        break;
      }
    }
    
    if (app.display_name.empty()) {
      app.display_name = pkg;
    }
    
    app_registry_[pkg] = app;
  }
  
  // 加载其他预置应用
  for (const auto& info : preset::kAppInfoList) {
    if (preset_ai_apps_.count(info.package) > 0) {
      continue;  // 已处理
    }
    
    AppRegistry app;
    app.package_name = info.package;
    app.display_name = info.display_name;
    app.scene_type = info.scene_type;
    app.is_ai_app = false;
    app.enabled = true;
    
    app_registry_[info.package] = app;
  }
  
  LOGI("Loaded %zu preset apps (%zu AI apps)", 
       app_registry_.size(), preset_ai_apps_.size());
}

}  // namespace ai
}  // namespace rime