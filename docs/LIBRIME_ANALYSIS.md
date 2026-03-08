# librime 项目分析与 AI 模块集成方案

> 文档版本: v1.0
> 创建日期: 2026-03-06

---

## 一、项目概述

当前项目是 **librime** 核心引擎（版本 1.16.1），而不是 Trime。Trime 是 Android 前端应用，需要单独克隆。

### 1.1 项目结构

```
librim_AI/
├── src/
│   ├── rime_api.h          # 公共 C API
│   ├── rime_api.cc
│   └── rime/               # 核心实现
│       ├── engine.h/cc     # 输入引擎
│       ├── context.h/cc    # 输入上下文
│       ├── module.h/cc     # 模块管理
│       ├── registry.h/cc   # 组件注册
│       ├── core_module.cc  # 核心模块
│       ├── gear/           # 处理器、翻译器、过滤器
│       ├── dict/           # 词典系统
│       └── config/         # 配置系统
├── plugins/                # 插件目录
├── deps/                   # 依赖库
└── data/                   # 数据文件
```

---

## 二、关键架构分析

### 2.1 模块系统

librime 使用模块化架构，通过 `RIME_REGISTER_MODULE` 宏注册模块：

```cpp
// 示例：core_module.cc
static void rime_core_initialize() {
    // 注册组件
    Registry& r = Registry::instance();
    r.Register("config_builder", config_builder);
    r.Register("config", config_loader);
    r.Register("schema", schema_component);
}

static void rime_core_finalize() {
    // 清理资源
}

RIME_REGISTER_MODULE(core)
```

**现有模块**：
- `core` - 核心组件（配置、schema）
- `dict` - 词典系统
- `gears` - 处理器、翻译器、过滤器
- `levers` - 部署工具

### 2.2 插件系统

插件位于 `plugins/` 目录，可以：
- 独立编译为动态库
- 或合并到主库中

插件可以注册新的组件类型（Processor、Translator、Filter 等）。

### 2.3 Context 通知机制

`Context` 类提供多个通知器，用于监听输入事件：

```cpp
class Context {
    // 通知器
    Notifier& commit_notifier();      // 提交文本时触发
    Notifier& update_notifier();      // 更新时触发
    Notifier& select_notifier();      // 选择候选时触发
    Notifier& delete_notifier();      // 删除时触发
    Notifier& abort_notifier();       // 取消时触发
};
```

### 2.4 公共 API

`rime_api.h` 提供了 C 语言接口，供前端调用：

```cpp
// 核心 API
void RimeInitialize(RimeTraits* traits);
void RimeFinalize(void);
RimeSessionId RimeCreateSession(void);
Bool RimeProcessKey(RimeSessionId session_id, int keycode, int mask);
Bool RimeGetCommit(RimeSessionId session_id, RimeCommit* commit);
Bool RimeGetContext(RimeSessionId session_id, RimeContext* context);
```

---

## 三、AI 模块集成方案

### 3.1 方案概述

有两种集成方式：

| 方案 | 说明 | 优点 | 缺点 |
|------|------|------|------|
| **A. librime 插件** | 在 librime 层实现 AI 模块 | 跨平台复用 | 需要修改 librime |
| **B. Trime 层实现** | 在 Android 前端实现 | 不修改核心库 | 无法跨平台复用 |

**推荐方案 A**：作为 librime 插件实现，可通过 `commit_notifier` 监听用户输入。

### 3.2 模块结构

```
src/rime/ai/
├── ai_module.cc           # 模块注册
├── llm_engine.h/cc        # LLM 引擎封装
├── profile_manager.h/cc   # 特征管理
├── enhance_manager.h/cc   # 智能增强
├── privacy_controller.h/cc# 无痕模式
└── feature_types.h        # 类型定义
```

### 3.3 模块注册代码

```cpp
// ai_module.cc
#include <rime/module.h>
#include <rime/registry.h>
#include "llm_engine.h"
#include "profile_manager.h"
#include "enhance_manager.h"
#include "privacy_controller.h"

static void rime_ai_initialize() {
    LOG(INFO) << "Initializing Librim AI module.";
    
    // 初始化各子模块
    rime::ai::LLMEngine::Instance().Initialize();
    rime::ai::ProfileManager::Instance().Initialize();
    rime::ai::EnhanceManager::Instance().Initialize();
    rime::ai::PrivacyController::Instance().Initialize();
}

static void rime_ai_finalize() {
    LOG(INFO) << "Finalizing Librim AI module.";
    
    rime::ai::LLMEngine::Instance().Shutdown();
}

// 注册自定义 API
static RimeCustomApi* rime_ai_get_api() {
    static RimeAiApi api;
    if (!api.data_size) {
        RIME_STRUCT_INIT(RimeAiApi, api);
        api.recognize_feature = rime_ai_recognize_feature;
        api.save_feature = rime_ai_save_feature;
        // ... 其他 API
    }
    return &api;
}

RIME_REGISTER_CUSTOM_MODULE(ai) {
    module->get_api = rime_ai_get_api;
}
```

### 3.4 监听用户输入

通过 Engine 的 Context 监听提交事件：

```cpp
// 在 Engine 初始化时绑定
void SetupAIListener(Engine* engine) {
    if (!engine || !engine->context()) return;
    
    engine->context()->commit_notifier().connect(
        [](Context* ctx) {
            // 获取提交的文本
            string text = ctx->GetCommitText();
            
            // 触发特征识别
            if (!PrivacyController::Instance().IsEnabled()) {
                ProfileManager::Instance().RecognizeFeature(text, 
                    [](const RecognitionResult& result) {
                        if (result.has_feature) {
                            // 通知 UI 显示提示卡片
                        }
                    });
            }
        });
}
```

### 3.5 CMake 配置

```cmake
# src/rime/CMakeLists.txt 添加
if(ENABLE_AI_MODULE)
  file(GLOB RIME_AI_SOURCES ai/*.cc)
  list(APPEND rime_sources ${RIME_AI_SOURCES})
  
  # MLC-LLM 依赖
  find_package(mlc_llm REQUIRED)
  target_link_libraries(rime mlc_llm::mlc_llm)
  
  # SQLCipher 依赖
  find_package(SQLCipher REQUIRED)
  target_link_libraries(rime sqlcipher)
endif()
```

---

## 四、Trime 集成要点

### 4.1 Trime 项目位置

Trime 是独立项目：https://github.com/osfans/trime

需要单独克隆：
```bash
git clone https://github.com/osfans/trime.git
```

### 4.2 Trime 调用 librime

Trime 通过 JNI 调用 librime 的 C API：

```java
// Trime 中的 Rime.java
public class Rime {
    static {
        System.loadLibrary("rime");
    }
    
    // 调用 librime API
    private native static void nativeInit(RimeTraits traits);
    private native static long nativeCreateSession();
    private native static boolean nativeProcessKey(long sessionId, int keycode, int mask);
}
```

### 4.3 AI 功能接入

在 Trime 中扩展 JNI 接口：

```java
// RimeAi.java
public class RimeAi {
    static {
        System.loadLibrary("rime");  // librime 已包含 AI 模块
    }
    
    // 调用 AI API
    private native static void nativeRecognizeFeature(String text, long callbackPtr);
    private native static String nativeGetFeatures(String category);
    private native static void nativeSetPrivacyMode(boolean enabled);
}
```

---

## 五、开发路径建议

### 5.1 第一步：创建 AI 模块骨架

在 `src/rime/ai/` 创建基础文件：
1. `ai_module.cc` - 模块注册
2. `feature_types.h` - 类型定义
3. 修改 `CMakeLists.txt` 添加编译配置

### 5.2 第二步：集成 LLM

1. 下载 MLC-LLM Android SDK
2. 编译 Qwen2.5-1.5B 模型（INT4 量化）
3. 在真机上测试推理延迟

### 5.3 第三步：克隆 Trime 并集成

1. 克隆 Trime 项目
2. 配置依赖关系
3. 扩展 JNI 接口
4. 实现 UI 组件

---

## 六、关键代码位置

| 功能 | 文件位置 |
|------|----------|
| 模块注册宏 | `src/rime_api.h:541` |
| 模块管理器 | `src/rime/module.h` |
| 组件注册 | `src/rime/registry.h` |
| 引擎类 | `src/rime/engine.h` |
| 上下文类 | `src/rime/context.h` |
| 插件系统 | `plugins/CMakeLists.txt` |
| 核心 API | `src/rime_api.h` |

---

## 七、下一步行动

1. **克隆 Trime 项目** - 了解 Android 前端的具体实现
2. **创建 AI 模块目录** - 在 librime 中添加 `src/rime/ai/`
3. **编译测试** - 确保 MLC-LLM 可以在 Android 上运行