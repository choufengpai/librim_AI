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
| `LlamaService.kt` | llama.cpp 推理服务（2026-08-20 替代 MLCService.kt） |
| `LLMNative.kt` | JNI 声明 |
| `RimeAi.kt` | Trime AI 接口 |
| `FeatureSuggestionCard.kt` | 提示卡片 UI |
| `PrivacyModeIndicator.kt` | 隐私指示器 UI |

---

## 五、下一步

1. ~~**编译验证** - 编译 librime + AI 模块~~ ✅ 2026-08-20 完成
2. ~~**键盘主题** - 搜狗风浅色主题 sogou_light.trime.yaml~~ ✅ 2026-08-20 完成
3. ~~**模型升级** - llama.cpp + Qwen3.5-2B，打通真实推理链路~~ ✅ 2026-08-20 完成（代码层，真机验收随阶段 5）
4. **Trime 重建** - APK 编译 ✅、真机安装/模型部署/AI 链路验证/rime-ice 部署 ✅、拼音输入实测（ni→你）✅、主题像素级确认 ✅ 2026-08-20 完成
5. **键盘体验升级（进行中）** - t9 九键方案 + sogou_light v2 + AI 建议条已编码部署；🚧 卡在真机主题加载崩溃（详见上方遗留问题），下次从 logcat 主题错误日志排查
6. **剩余验收** - 九宫格输入实测 / AI 键触发建议条实测 / 9 候选 / 纠错（shrufa→输入法）/ 中英混输（iphone→iPhone）/ AI 特征识别推理延迟 <3s

### 2026-08-20 基础输入体验升级记录

- 引入雾凇拼音 rime-ice 到 `data/rime-ice/`（替代 luna_pinyin 作为默认方案，修复繁体默认问题）
- 引入 librime-lua 插件（`plugins/lua`，in-tree lua5.4，BUILD_MERGED_PLUGINS=ON 合入主库）
- 本地部署验证通过（macOS，含 tencent 大词库仅 8.9s）：简体输出、长句整句、智能纠错（shrufa→输入法）、中英混输（iphone→iPhone）、lua 日期（rq→2026-08-20）、大词库命中（duanshipin→短视频）、page_size=9
- 搜狗风浅色主题 `data/rime-ice/sogou_light.trime.yaml`（28 键主键盘 + 符号/数字页，白底蓝主色圆角）

### 2026-08-20 端侧模型升级记录（阶段 4）

- **推理引擎切换**: MLC-LLM → llama.cpp（vendored 于 `llama.cpp/`，gitignored，Android 构建经 CMake 子项目集成）
- **模型**: Qwen3.5-2B-Instruct GGUF Q4_K_M（1.28GB，已下载到 `~/Developer/models/`）
- **代码变更**: `MLCService.kt` → `LlamaService.kt`（真实 llama.cpp 推理）；新增 `llama_jni.cc`（JNI 桥，参考官方 llama-android 示例）、`ai_module_jni.cc`（RimeAi.kt 全套 JNI 实现，此前完全缺失）；`llm_jni.cc/h` 补桥初始化入口并修复线程问题；`llm_engine.cc` Android 下推理/模型管理走 JNI 桥，桌面保留 MockInference
- **打通断链**: 修复 `LLMJNIBridge::Initialize`/`SetMLCService` 无人调用（新增 `LLMNative.setInferenceService`）、`ProcessRequest` 直连 mock、RimeAi JNI 无 C++ 实现三处断点
- **Qwen3.5 适配**: ChatML 模板（兼容 Qwen2-3.5），`/no_think` 默认关闭思考模式，输出后处理剥 <think> 块兑底；业务 prompt 模板加直接输出约束
- **验证**: macOS 桌面构建回归通过；llama_jni.cc / ai_module_jni.cc / llm_jni.cc 经 NDK r28c arm64 交叉语法验证通过
- 详细集成文档: `docs/MODEL_MIGRATION_Qwen35.md` 第十一节

### 2026-08-20 Trime 集成与 APK 编译（阶段 5）

- **Trime**: 重新克隆 osfans/trime（3.3.12，submodule 全量初始化含 librime 1.17.0 / librime-lua / OpenCC 等）
- **AI 模块移植**: `src/rime/ai/` 8 个源文件拷入 `trime/app/src/main/jni/librime/src/rime/ai/`，`src/CMakeLists.txt` 三处 AI 改动（源收集/追加 rime_src/附加依赖）同步移植
- **CMake**: `jni/cmake/RimeAiConfig.cmake`（SQLite vendored + llama.cpp 子项目）；`jni/CMakeLists.txt` 在 `include(Rime)` 前 `option(ENABLE_AI_MODULE ON)` + `include(RimeAiConfig)`；llama.cpp 以 symlink 挂在 `jni/llama.cpp`
- **SQLite**: Android NDK 无系统 sqlite，vendored amalgamation 3.53.4 于 `jni/sqlite/sqlite-amalgamation-3530400/`（静态链入）
- **rime_jni.cc**: `rime_require_module_ai()` 注册 + `ai_jni_force_link` 常量表强制保留 dlsym 解析的 JNI 对象（静态库丢弃问题）
- **Kotlin**: RimeAi.kt / LlamaService.kt / LLMNative.kt / FeatureSuggestionCard.kt / PrivacyModeIndicator.kt 放回 Trime 主模块；库名修复 `rime`→`rime_jni`；FeatureSuggestionCard 去 material 依赖（FrameLayout+GradientDrawable 圆角卡片）；TrimeApplication.onCreate 接线 `initializeLibrimAi()`（aiInit 传真实模型/DB 路径 + LlamaService 桥注册，懒加载）
- **默认方案**: `DataManager.SCHEMA_LIST_CUSTOM_PATCH` 由 luna_pinyin 改为 rime_ice（否则 default.custom.yaml 覆盖 schema_list 回繁体）
- **方案数据**: `data/rime-ice/` 全量进 `assets/shared/`（替换 prelude 的 default.yaml/symbols.yaml symlink），共 98 个文件；`sogou_light.trime.yaml` 同目录（Trime 自动发现）
- **llama.cpp 补丁**: `src/llama-mmap.cpp` Android API<23 的 posix_madvise 兼容 shim（Trime minSdk 21，bionic API 23+ 才有 posix_madvise/POSIX_MADV_*）
- **构建**: `JAVA_HOME=Android Studio JBR` + `NDK_VERSION=28.2.13676358` + `BUILD_ABI=arm64-v8a` + `make patch-apply`（lua.patch）；SDK cmake 3.31.6
- **产物**: `com.osfans.trime-2a79963-arm64-v8a-debug.apk`（41.7MB，librime_jni.so 25.2MB 含 llama.cpp+sqlite，96 个 AI/Trime JNI 符号，assets 98 文件含 rime-ice 全量数据 + sogou_light 主题）
- **模型部署方式**: 内部存储 stdin 流式写入（免 root，debug 包）：`adb shell "run-as com.osfans.trime.debug sh -c 'mkdir -p files/models && cat > files/models/<name>'" < model.gguf`
  - 外部私有目录 `adb push` 不可用：Android 11+ scoped storage 下 push 文件 FUSE 属主为 shell，app `stat` 即 Permission denied（`File.exists()` false）
  - `TrimeApplication.initializeLibrimAi()` 已改为 external→internal 双路径 fallback

### 2026-08-20 真机部署验证（vivo V2454A / Android 16，阶段 5 后半）

- **AI 链路验证通过**: `model present: true`（内部存储 1.28GB，MD5 校验一致）；aiInit success；LLMJNIBridge/LlamaService 桥注册全通（logcat LibrimAI_* 系列）
- **rime-ice 首次部署成功**: `rime_ice.prism.bin`/`reverse.bin`/`table.bin`(60.6MB 含 tencent 大词库) + `melt_eng.*` 全部编译，约 1 分钟内（达标 <2 分钟）
- **默认方案修正**: 清除旧版 Trime 残留的 `/sdcard/rime/default.custom.yaml`（schema_list 仍为 luna_pinyin，会盖住 rime_ice）+ build 缓存后重新部署生效
- **输入法状态**: Trime 已启用并设为当前输入法，键盘可正常唤起（mInputShown=true，短信编辑场景实测）
- **主题**: `selected_theme` 已设为 `sogou_light.trime`（shared_prefs 直改，待下次唤起键盘生效确认）
- **待完成验收（需用户解锁手机配合）**: 拼音输入实测（简体/纠错/9 候选/中英混输）、sogou_light 视觉确认、AI 特征识别真实推理（PRD <3s）
- **备注**: Trime 不处理硬键盘 keyevent（纯触屏 IME），输入验证需模拟软键盘 tap（键盘为自绘 Canvas，uiautomator 不可见，需截图定位按键坐标）；真机验收操作期间手机反复自动锁屏，已按用户要求暂停待续

### 2026-08-20（晚）拼音输入实测通过 + 键盘体验升级启动

**拼音输入验证（sogou_light v1 主题下）**:
- ✅ 简体输出: ni → 候选"你尼泥这拟腻悦"（首选蓝）→ 空格上屏 → EditText='你'
- ✅ 主题像素级验证: 键面 0xFFFFFF / 键盘背景 0xD3D8E0 / 字色 0x333640 / 首选候选蓝 0x168CE9 全部吻合
- 验证方法: PIL（`~/miniconda3/bin/pip install pillow`，系统 python3 被 PEP 668 拦）截图采样像素；键盘按键坐标经截图+像素行扫描定位（已验证映射: n=(748,1950)、i=(800,1619)、空格=(537,2203) 等）

**键盘体验升级（用户三点反馈: ①九宫格输入 ②样式对齐搜狗 UX ③AI 功能体现，参考 BuddyType 概念稿）**:
- **用户决策确认**: 搜狗浅色风配色 / 九宫格为默认布局（26 键一键互切）/ AI 手动键触发
- **调研结论**: Trime 九宫格 = 方案层（数字→拼音映射）+ 主题层（九宫格布局），参考仓输入法 t9 方案（雾凇作者 Dvel 参与）；Trime 按键支持 broadcast command → AI 键可行；工具栏由主题 `toolBar.buttons` 配置；AI 图标用内置 `ic@star_four_points`；上游已有 `t9_preedit.lua`（preedit 数字串转拼音显示滤镜）直接挂载
- **实施（6 个文件）**:
  1. `data/rime-ice/t9.schema.yaml` — 九键方案，继承雾凇拼音（全词库/纠错/emoji 保留），speller derive 数字映射（`derive/[abc]/2/` ... `[wxyz]/9`），挂 t9_preedit.lua
  2. `data/rime-ice/sogou_light.trime.yaml` **v2 完全重写** — 九宫格默认布局（3×4）+ 26 键键盘 + 搜狗式工具栏 + AI 四角星键（broadcast）+ 符号/数字页
  3. `trime/.../ime/ai/AiSuggestionBar.kt` — 键盘上方 AI 建议条浮层（生成中/结果/错误三态）
  4. `trime/.../ime/ai/AiSuggestionDelegate.kt` — 收 AI 键广播 → aiGetAllFeatures → LlamaService 推理 → 建议条展示
  5. `InputView.kt` 挂载建议条；`DataManager.kt` 默认方案 t9 首位
  6. 本地 YAML 语法校验通过（python yaml）
- **编译坑（新）**: NDK 版本冲突用 gradle 属性覆盖；armeabi-v7a FP16 报错改 `-PbuildABI=arm64-v8a` 只编 arm64；**assets 新增文件必须跑官方 checksums 任务重生成 `checksums.json`** 否则不会同步到真机
- **真机部署**: APK 安装成功（vivo 安装弹窗需人工确认）、default.custom.yaml 已写 t9 默认、t9.prism 部署成功（约 10s，prism 是音节级拼写树体积小属正常）
- **❌ 遗留问题（下次首先排查）**: 唤起键盘时 Trime 进程崩溃 — **主题加载失败导致 `_activeTheme` 未初始化**（怀疑 sogou_light v2 YAML 含 Trime 不识别的字段/结构）。排查路径: ①logcat 抓主题加载错误日志 ②对照 ThemeManager 初始化链路 ③拿官方 trime.yaml 结构逐段比对 v2 主题差异

---

## 六、决策记录

| 日期 | 决策内容 |
|------|----------|
| 2026-03-06 | 选择 MLC-LLM + Qwen2.5-1.5B |
| 2026-03-08 | 使用预编译模型 |
| 2026-03-08 | JNI 桥接架构 |
| 2026-03-08 | 真机测试延后到开发完成后 |
| 2026-08-20 | 键盘升级: 搜狗浅色风 / 九宫格默认布局 / AI 手动键（用户三点反馈决策） |

---

## 变更日志

| 时间 | 变更内容 |
|------|----------|
| 2026-03-08 | 完成所有核心模块开发 |
| 2026-03-08 | 完成 Android UI 组件 |
| 2026-08-20 | rime-ice 方案 + librime-lua 插件 + 搜狗风主题 + llama.cpp/Qwen3.5-2B 模型链路（详见“下一步”各小节） |
| 2026-08-20 | Trime APK 编译安装 + 真机部署验证（模型内部存储部署、rime-ice 首次部署成功；输入实测/AI 推理待用户配合解锁后继续） |
| 2026-08-20 | 拼音输入实测通过（ni→你 简体上屏 + 主题像素级验证）；键盘体验升级启动: t9 九键方案 + sogou_light v2 重写 + AI 建议条（AiSuggestionBar/Delegate），已部署真机但遇主题加载崩溃待排查 |