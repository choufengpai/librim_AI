# Librim AI

> ⚠️ **本项目正在进行中（Work in Progress）**：功能尚未稳定，接口与架构可能随时调整，暂不适合生产环境使用。

**Librim AI** 是基于 [librime](https://github.com/rime/librime)（中州韵输入法引擎）的实验性分支，目标是在完全本地、离线的环境下，为输入法引入大语言模型（LLM）驱动的智能能力，同时严格保护用户隐私。

## 项目概要

传统输入法只负责"把按键变成文字"，Librim AI 希望输入法更进一步——在**设备端本地**理解用户，成为用户与 AI 应用之间的智能桥梁：

| 功能 | 说明 | 状态 |
|------|------|------|
| **个人特征识别** | 在日常输入中本地识别个人特征（偏好、习惯、近期事项等），以非侵入式卡片询问用户是否存入个人信息库 | 🚧 开发中 |
| **智能输入增强** | 在 AI 对话类应用中，自动匹配个人特征库，一键生成补充了个人背景的增强 Prompt | 🚧 开发中 |
| **无痕模式** | 一键关闭所有特征识别与记录，保护隐私输入场景 | 🚧 开发中 |

核心原则：

- **全本地推理**：LLM 推理完全在设备端进行，输入内容不出设备
- **用户完全可控**：所有特征需用户确认后才会保存，可查看、编辑、删除
- **加密存储**：个人特征库使用 SQLCipher 加密持久化

## 技术方案

### 技术选型

| 类别 | 方案 | 说明 |
|------|------|------|
| 输入法引擎 | librime (C++17) | 上游核心，模块化插件架构 |
| 推理引擎 | [llama.cpp](https://github.com/ggml-org/llama.cpp)（vendored） | 端侧 GGUF 推理，Android JNI 集成成熟 |
| 本地模型 | Qwen3.5-2B-Instruct（Q4_K_M 量化，~1.3GB） | Gated DeltaNet 混合架构，262K 上下文 |
| 特征存储 | SQLite + SQLCipher | 加密本地数据库 |
| 首期平台 | Android（[Trime](https://github.com/osfans/trime) 前端） | macOS / Windows / Linux / iOS 后续扩展 |

> 注：早期方案为 MLC-LLM + Qwen2.5-1.5B，因 MLC-LLM 对 Qwen3.5 新架构的支持未落地，已迁移至 llama.cpp，详见 [docs/MODEL_MIGRATION_Qwen35.md](docs/MODEL_MIGRATION_Qwen35.md)。

### 系统架构

```
┌────────────────────────────────────────────────────────┐
│             Trime 输入法前端 (Android / Kotlin)         │
│   LlamaService · RimeAi · FeatureSuggestionCard · ...  │
└──────────────────────────┬─────────────────────────────┘
                           │ JNI
┌──────────────────────────▼─────────────────────────────┐
│              librime AI 模块 (src/rime/ai/)             │
│  ┌───────────────┐ ┌──────────────┐ ┌───────────────┐  │
│  │ ProfileManager│ │SceneDetector │ │ FeatureStorage│  │
│  │ (识别/匹配/生成)│ │ (AI 场景识别) │ │ (SQLite 加密)  │  │
│  └───────┬───────┘ └──────────────┘ └───────────────┘  │
│  ┌───────▼───────┐ ┌──────────────┐                     │
│  │  LLMEngine    │ │PromptManager │                     │
│  │ (推理引擎封装) │ │ (Prompt 模板) │                     │
│  └───────┬───────┘ └──────────────┘                     │
│  ┌───────▼────────────────────────┐                     │
│  │ LLMJNIBridge → LlamaService    │  ← Android 推理桥    │
│  │ → llama_jni.cc → llama.cpp     │                     │
│  └────────────────────────────────┘                     │
└──────────────────────────────────────────────────────────┘
                           │
┌──────────────────────────▼─────────────────────────────┐
│                librime 核心引擎（上游）                  │
│        Speller · Dictionary · Translator · Filter       │
└──────────────────────────────────────────────────────────┘
```

依赖方向严格单向：`JNI → ProfileManager/SceneDetector → LLMEngine → LLMJNIBridge → Java LlamaService → llama.cpp`。桌面平台（macOS/Linux）当前保留 Mock 推理用于链路联调，真实推理在 Android 真机验证。

### 推理链路（Android）

```
RimeAi.aiRecognizeFeature()
  → ai_module_jni.cc          # RimeAi 全套 JNI 实现
  → ProfileManager            # 业务编排：识别/匹配/生成
  → PromptManager             # Prompt 模板（ChatML，默认关闭思考模式降延迟）
  → LLMEngine::Infer
  → LLMJNIBridge              # 请求 ID → 异步回调映射
  → LlamaService.inference()  # Kotlin 侧
  → llama_jni.cc              # llama.cpp JNI 桥
  → llama.cpp (GGUF 推理)
  → 回调逐级返回
```

推理请求由工作线程串行处理（`std::queue` + `condition_variable`），空闲超时自动卸载模型以控制内存。

## 仓库结构

```
├── src/rime/            # librime 核心引擎（算法、词典、齿轮组件等）
│   └── rime/ai/         # ★ AI 模块：LLM 引擎、特征管理、场景识别、JNI 桥
├── android/ai/          # ★ Android 端：LlamaService、RimeAi 接口、建议卡片 UI
├── llama.cpp/           # 推理引擎（vendored，gitignored，需自行获取）
├── plugins/             # librime 插件（lua 等）
├── data/                # 输入方案数据（minimal / rime-ice / test）
├── docs/                # ★ 项目文档（PRD、技术方案、API、测试、迁移报告）
├── cmake/               # CMake 模块（含 RimeAiConfig.cmake）
├── tools/               # 命令行工具（console、deployer、dict_manager 等）
└── test/                # 核心库单元测试（GoogleTest）
```

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/AI_FEATURES_PRD.md](docs/AI_FEATURES_PRD.md) | 产品需求：三大 AI 功能的详细规格 |
| [docs/TECHNICAL_SOLUTION.md](docs/TECHNICAL_SOLUTION.md) | 技术方案：模块设计、数据流、接口定义 |
| [docs/API_SPECIFICATION.md](docs/API_SPECIFICATION.md) | API 规格说明 |
| [docs/MODEL_MIGRATION_Qwen35.md](docs/MODEL_MIGRATION_Qwen35.md) | 模型选型调研与 llama.cpp 迁移决策 |
| [docs/LIBRIME_ANALYSIS.md](docs/LIBRIME_ANALYSIS.md) | librime 源码分析 |
| [docs/TEST_PLAN.md](docs/TEST_PLAN.md) | 测试方案 |
| [docs/TODO.md](docs/TODO.md) | 任务清单与进度 |

## 构建

上游 librime 的通用构建说明见 [README-mac.md](README-mac.md) 与 [README-windows.md](README-windows.md)，Linux 下：

```
make
sudo make install
```

Android (Trime) 集成需额外获取 llama.cpp 子项目与 GGUF 模型文件，具体见 [docs/MODEL_MIGRATION_Qwen35.md](docs/MODEL_MIGRATION_Qwen35.md) 第十一节。

## 许可证与致谢

本项目基于 librime，遵循 [The 3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause)。上游项目与贡献者信息见 [librime](https://github.com/rime/librime)。

感谢以下开源项目：[llama.cpp](https://github.com/ggml-org/llama.cpp)、[Qwen](https://github.com/QwenLM)、[Trime](https://github.com/osfans/trime)、Boost、glog、LevelDB、marisa-trie、OpenCC、yaml-cpp。
