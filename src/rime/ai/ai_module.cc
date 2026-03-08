//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-06 Librim AI Team
//
// AI 模块注册和初始化
//

#include <rime_api.h>
#include <rime/common.h>
#include <rime/registry.h>
#include <rime/module.h>

#include "feature_types.h"

// 子模块头文件
#include "llm_engine.h"
// #include "profile_manager.h"
// #include "enhance_manager.h"
// #include "privacy_controller.h"

namespace rime {
namespace ai {

// 模块配置
static LLMConfig g_llm_config;
static bool g_ai_enabled = true;

// 初始化 AI 模块
static void rime_ai_initialize() {
  LOG(INFO) << "Initializing Librim AI module.";
  
  if (!g_ai_enabled) {
    LOG(INFO) << "AI module is disabled.";
    return;
  }
  
  // 初始化 LLM 引擎
  if (!LLMEngine::Instance().Initialize(g_llm_config)) {
    LOG(ERROR) << "Failed to initialize LLM engine.";
    return;
  }
  
  // TODO: 初始化其他子模块
  // ProfileManager::Instance().Initialize();
  // EnhanceManager::Instance().Initialize();
  // PrivacyController::Instance().Initialize();
  
  LOG(INFO) << "Librim AI module initialized successfully.";
}

// 清理 AI 模块
static void rime_ai_finalize() {
  LOG(INFO) << "Finalizing Librim AI module.";
  
  // 关闭 LLM 引擎
  LLMEngine::Instance().Shutdown();
  
  // TODO: 清理其他子模块
  
  LOG(INFO) << "Librim AI module finalized.";
}

// 获取 AI 模块 API
static RimeCustomApi* rime_ai_get_api() {
  // TODO: 返回自定义 API 结构
  return nullptr;
}

}  // namespace ai
}  // namespace rime

// ============================================================
// 模块注册
// ============================================================

// 使用自定义模块宏，以便提供 get_api
void rime_require_module_ai() {}

static void rime_customize_module_ai(RimeModule* module);

RIME_MODULE_INITIALIZER(rime_register_module_ai) {
  static RimeModule module = {0};
  if (!module.data_size) {
    RIME_STRUCT_INIT(RimeModule, module);
    module.module_name = "ai";
    module.initialize = rime::ai::rime_ai_initialize;
    module.finalize = rime::ai::rime_ai_finalize;
    module.get_api = rime::ai::rime_ai_get_api;
  }
  RimeRegisterModule(&module);
}

// 模块配置函数
namespace rime {
namespace ai {

// 设置 LLM 配置
void SetLLMConfig(const LLMConfig& config) {
  g_llm_config = config;
}

// 获取 LLM 配置
const LLMConfig& GetLLMConfig() {
  return g_llm_config;
}

// 启用/禁用 AI 功能
void SetEnabled(bool enabled) {
  g_ai_enabled = enabled;
}

// 检查 AI 功能是否启用
bool IsEnabled() {
  return g_ai_enabled;
}

}  // namespace ai
}  // namespace rime