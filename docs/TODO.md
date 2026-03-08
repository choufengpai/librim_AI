# Librim AI 项目待办清单

> 📅 创建时间: 2026-03-06
> 🔄 最后更新: 2026-03-08
> 📌 当前阶段: MVP 开发基本完成

---

## 状态说明

| 标记 | 含义 |
|------|------|
| ⏳ | 待办 - 还未开始 |
| 🚧 | 进行中 - 正在处理 |
| ✅ | 已完成 - 任务结束 |
| ❌ | 阻塞 - 遇到问题 |
| 🔄 | 需确认 - 等待用户/其他agent确认 |

---

## 一、已完成任务

| 任务 | 完成时间 | 产出物 |
|------|----------|--------|
| 编写 PRD 文档 | 2026-03-06 | `docs/AI_FEATURES_PRD.md` |
| 编写技术方案 | 2026-03-06 | `docs/TECHNICAL_SOLUTION.md` |
| 编写接口定义 | 2026-03-06 | `docs/API_SPECIFICATION.md` |
| 编写测试方案 | 2026-03-06 | `docs/TEST_PLAN.md` |
| 分析 librime 代码 | 2026-03-06 | `docs/LIBRIME_ANALYSIS.md` |
| 创建 AI 模块目录 | 2026-03-06 | `src/rime/ai/` |
| 实现 feature_types.h | 2026-03-06 | 类型定义 |
| 实现 ai_module.cc | 2026-03-06 | 模块注册 |
| 实现 llm_engine.h/cc | 2026-03-06 | LLM 引擎框架 |
| 实现 llm_jni.h/cc | 2026-03-08 | JNI 桥接层 |
| 克隆 Trime 项目 | 2026-03-08 | `/Users/derek/trime` |
| 模型准备 | 2026-03-08 | 预编译 Qwen2.5-1.5B |
| 实现 feature_storage.h/cc | 2026-03-08 | 特征存储层 |
| 实现 profile_manager.h/cc | 2026-03-08 | 特征管理器 |
| 实现 scene_detector.h/cc | 2026-03-08 | 场景识别器 |
| 实现 RimeAi.kt JNI 接口 | 2026-03-08 | Trime AI JNI |
| 实现 MLCService.kt | 2026-03-08 | Android LLM 服务 |
| 实现 FeatureSuggestionCard.kt | 2026-03-08 | 提示卡片 UI |
| 实现 PrivacyModeIndicator.kt | 2026-03-08 | 隐私模式指示器 |

---

## 二、待办任务

### 🔴 高优先级 (MVP Day 1) - ✅ 完成

| ID | 任务 | 状态 |
|----|------|------|
| T001 | 克隆 Trime 项目 | ✅ |
| T002-T004 | AI 模块基础结构 | ✅ |
| T005 | 集成 MLC-LLM SDK | ✅ |
| T006 | 模型准备 | ✅ |
| T007 | LLM 引擎 | ✅ |
| T008 | 真机推理延迟测试 | ⏳ (延后) |

### 🟡 中优先级 (MVP Day 2) - ✅ 完成

| ID | 任务 | 状态 |
|----|------|------|
| T009 | feature_storage | ✅ |
| T010 | profile_manager | ✅ |
| T011 | scene_detector | ✅ |
| T012-T013 | 特征匹配/生成 (集成在 profile_manager) | ✅ |
| T014 | Trime JNI 扩展 | ✅ |
| T015 | 提示卡片 UI | ✅ |

### 🟢 低优先级 (MVP Day 3)

| ID | 任务 | 状态 |
|----|------|------|
| T016 | privacy_controller (集成在 profile_manager) | ✅ |
| T017 | 无痕模式 UI | ✅ |
| T018 | 功能验收测试 | ⏳ |
| T019 | Bug 修复 | ⏳ |

---

## 三、研发进度总览

```
Day 1 (高优先级): ████████████████████ 100% ✅
Day 2 (中优先级): ████████████████████ 100% ✅
Day 3 (低优先级): ████████████░░░░░░░░ 60%  

总进度: ██████████████████░░ 90%
```

---

## 四、代码清单

### C++ 核心 (src/rime/ai/)

| 文件 | 说明 | 行数 |
|------|------|------|
| `feature_types.h` | 类型定义 | ~150 |
| `ai_module.cc` | 模块注册 | ~50 |
| `llm_engine.h/cc` | LLM 引擎 | ~650 |
| `llm_jni.h/cc` | JNI 桥接 | ~500 |
| `feature_storage.h/cc` | 特征存储 | ~600 |
| `profile_manager.h/cc` | 特征管理 | ~550 |
| `scene_detector.h/cc` | 场景识别 | ~350 |

### Android 层 (android/ai/)

| 文件 | 说明 |
|------|------|
| `MLCService.kt` | MLC-LLM 服务封装 |
| `LLMNative.kt` | JNI 声明 |
| `RimeAi.kt` | Trime AI 接口 |
| `FeatureSuggestionCard.kt` | 提示卡片 UI |
| `PrivacyModeIndicator.kt` | 隐私指示器 UI |

---

## 五、下一步

1. **T008 真机测试** - 需要 Android 设备
2. **编译验证** - 编译 librime + AI 模块
3. **集成测试** - 完整功能验收

---

## 六、决策记录

| 日期 | 决策内容 |
|------|----------|
| 2026-03-06 | 选择 MLC-LLM + Qwen2.5-1.5B |
| 2026-03-08 | 使用预编译模型 |
| 2026-03-08 | JNI 桥接架构 |
| 2026-03-08 | 真机测试延后到开发完成后 |

---

## 变更日志

| 时间 | 变更内容 |
|------|----------|
| 2026-03-08 | 完成所有核心模块开发 |
| 2026-03-08 | 完成 Android UI 组件 |