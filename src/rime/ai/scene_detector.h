//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 场景识别器
// 识别用户当前输入场景（如 AI 对话应用），触发智能增强
//

#ifndef RIME_AI_SCENE_DETECTOR_H_
#define RIME_AI_SCENE_DETECTOR_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace rime {
namespace ai {

// ============================================================
// 场景类型
// ============================================================

enum class SceneType {
  kUnknown = 0,          // 未知场景
  kAIChat = 1,           // AI 对话应用（ChatGPT、Claude、文心一言等）
  kSearch = 2,           // 搜索引擎
  kSocial = 3,           // 社交应用（微信、QQ等）
  kEmail = 4,            // 邮件应用
  kNote = 5,             // 笔记应用
  kMessaging = 6,        // 即时通讯
  kBrowser = 7,          // 浏览器
  kCodeEditor = 8,       // 代码编辑器
  kCustom = 100          // 自定义场景
};

// ============================================================
// 场景信息
// ============================================================

struct SceneInfo {
  SceneType type = SceneType::kUnknown;
  std::string app_package;           // 应用包名
  std::string app_name;              // 应用名称
  std::string description;           // 场景描述
  bool should_enhance = false;       // 是否启用智能增强
  double priority = 1.0;             // 优先级
  
  // 场景配置
  struct Config {
    bool auto_match_features = true; // 自动匹配特征
    bool show_suggestion_card = true;// 显示建议卡片
    int min_input_length = 5;        // 最小输入长度
    std::vector<std::string> keywords; // 触发关键词
  } config;
};

// ============================================================
// 应用注册信息
// ============================================================

struct AppRegistry {
  std::string package_name;          // 包名
  std::string display_name;          // 显示名称
  SceneType scene_type;              // 场景类型
  bool is_ai_app = false;            // 是否是 AI 应用
  bool enabled = true;               // 是否启用
  std::vector<std::string> aliases;  // 别名列表
};

// ============================================================
// SceneDetector - 场景识别器
// ============================================================

class SceneDetector {
public:
  // 获取单例实例
  static SceneDetector& Instance();
  
  // 初始化
  bool Initialize();
  
  // ============================================================
  // 场景识别
  // ============================================================
  
  // 根据应用包名识别场景
  SceneInfo DetectScene(const std::string& package_name);
  
  // 检查是否是 AI 对话应用
  bool IsAIChatApp(const std::string& package_name);
  
  // 检查是否应该启用智能增强
  bool ShouldEnhance(const std::string& package_name);
  
  // 获取场景信息
  SceneInfo GetSceneInfo(const std::string& package_name);
  
  // ============================================================
  // 应用注册管理
  // ============================================================
  
  // 注册应用
  void RegisterApp(const AppRegistry& app);
  
  // 批量注册应用
  void RegisterApps(const std::vector<AppRegistry>& apps);
  
  // 取消注册应用
  void UnregisterApp(const std::string& package_name);
  
  // 获取所有已注册应用
  std::vector<AppRegistry> GetRegisteredApps();
  
  // 获取指定类型的应用
  std::vector<AppRegistry> GetAppsByType(SceneType type);
  
  // ============================================================
  // 自定义场景配置
  // ============================================================
  
  // 设置场景配置
  void SetSceneConfig(SceneType type, const SceneInfo::Config& config);
  
  // 获取场景配置
  SceneInfo::Config GetSceneConfig(SceneType type);
  
  // 启用/禁用应用的智能增强
  void SetAppEnhancement(const std::string& package_name, bool enabled);
  
  // ============================================================
  // 预置 AI 应用列表
  // ============================================================
  
  // 获取预置 AI 应用包名列表
  std::vector<std::string> GetPresetAIApps();
  
  // 检查是否是预置 AI 应用
  bool IsPresetAIApp(const std::string& package_name);
  
private:
  SceneDetector();
  ~SceneDetector() = default;
  
  // 禁止拷贝
  SceneDetector(const SceneDetector&) = delete;
  SceneDetector& operator=(const SceneDetector&) = delete;
  
  // 加载预置应用
  void LoadPresetApps();
  
  // 成员变量
  std::mutex mutex_;
  bool initialized_ = false;
  
  // 应用注册表（包名 -> 注册信息）
  std::unordered_map<std::string, AppRegistry> app_registry_;
  
  // 场景配置（场景类型 -> 配置）
  std::unordered_map<SceneType, SceneInfo::Config> scene_configs_;
  
  // 预置 AI 应用包名集合
  std::unordered_set<std::string> preset_ai_apps_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_SCENE_DETECTOR_H_