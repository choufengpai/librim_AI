# Librim AI 技术方案大纲

> 文档版本: v1.1
> 创建日期: 2026-03-06
> 最后更新: 2026-03-06
> 基于文档: AI_FEATURES_PRD.md v0.8

---

## 一、项目概述

### 1.1 项目目标
基于 librime 输入法引擎，增加 AI 智能能力，实现个人特征识别、智能输入增强、隐私保护三大核心功能。

### 1.2 目标平台
- **首期**：Android (Trime 前端)
- **后续**：macOS、Windows、Linux、iOS

---

## 二、技术选型

| 类别 | 技术方案 | 说明 |
|------|----------|------|
| 本地 LLM | Qwen2.5-1.5B-Instruct | 中文能力强，体积~1.5GB |
| 推理引擎 | MLC-LLM | 移动端优化，有 Android SDK |
| 加密存储 | SQLCipher | 个人特征数据加密 |
| 本地存储 | SQLite / LevelDB | 特征库持久化 |

---

## 三、系统架构

### 3.1 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    Trime 输入法前端 (Android)                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │  UI Layer   │ │ Feature UI  │ │ Settings UI │           │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘           │
└─────────┼───────────────┼───────────────┼───────────────────┘
          │               │               │
          ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────┐
│                     Librim AI Core Layer                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                   local_llm 模块                     │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │   │
│  │  │ Model Mgr  │  │ Inference   │  │ Prompt Mgr  │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐         │
│  │personal_     │ │ smart_       │ │ privacy_     │         │
│  │profile       │ │ enhance      │ │ mode         │         │
│  │ 特征识别与存储│ │ 智能输入增强  │ │ 无痕模式管理  │         │
│  └──────┬───────┘ └──────┬───────┘ └──────────────┘         │
│         │                │                                   │
│         ▼                ▼                                   │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Feature Storage (SQLCipher)            │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐          │    │
│  │  │特征库    │  │配置存储   │  │日志存储   │          │    │
│  │  └──────────┘  └──────────┘  └──────────┘          │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│                     librime Core Engine                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ Speller  │ │ Dict     │ │ Translator│ │ Filter   │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 模块职责

| 模块 | 路径 | 职责 |
|------|------|------|
| `local_llm` | `src/rime/local_llm/` | LLM 模型管理、推理引擎封装、Prompt 管理 |
| `personal_profile` | `src/rime/personal_profile/` | 特征识别、存储、查询、过期清理 |
| `smart_enhance` | `src/rime/smart_enhance/` | 场景识别、特征匹配、增强内容生成 |
| `privacy_mode` | `src/rime/privacy_mode/` | 无痕模式状态管理、视觉提示 |

---

## 四、核心模块详细设计

### 4.1 local_llm 模块

#### 4.1.1 模块结构
```
local_llm/
├── llm_engine.h         # 引擎接口定义
├── llm_engine.cc
├── model_manager.h      # 模型加载/卸载管理
├── model_manager.cc
├── prompt_manager.h     # Prompt 模板管理
├── prompt_manager.cc
├── inference_worker.h   # 异步推理工作线程
├── inference_worker.cc
└── mlc_adapter.h        # MLC-LLM 适配层
    └── mlc_adapter.cc
```

#### 4.1.2 核心接口
```cpp
class LLMEngine {
public:
    // 初始化引擎
    bool Initialize(const LLMConfig& config);
    
    // 同步推理（阻塞）
    std::string Infer(const std::string& prompt);
    
    // 异步推理（回调）
    void InferAsync(const std::string& prompt, 
                    std::function<void(std::string)> callback);
    
    // 加载/卸载模型
    bool LoadModel(const std::string& model_path);
    void UnloadModel();
    
    // 获取模型状态
    ModelStatus GetStatus() const;
};
```

#### 4.1.3 Prompt 管理
- 特征识别 Prompt
- 特征匹配 Prompt
- 内容生成 Prompt
- 支持模板变量替换

---

### 4.2 personal_profile 模块

#### 4.2.1 模块结构
```
personal_profile/
├── profile_manager.h    # 特征管理器
├── profile_manager.cc
├── feature_recognizer.h # 特征识别器
├── feature_recognizer.cc
├── feature_storage.h    # 特征存储层
├── feature_storage.cc
├── feature_types.h      # 特征类型定义
└── exclusion_filter.h   # 排除规则过滤器
    └── exclusion_filter.cc
```

#### 4.2.2 数据模型
```cpp
struct PersonalFeature {
    std::string id;           // 特征ID
    std::string category;      // 类别：basic_info/preferences/habits/recent_events/relationships
    std::string key;           // 特征键名
    std::string value;         // 特征值
    double confidence;         // 置信度
    std::string source_text;   // 来源文本
    int64_t created_at;        // 创建时间
    int64_t updated_at;        // 更新时间
    bool enabled;              // 是否启用（用于智能增强）
};

// 特征类别枚举
enum class FeatureCategory {
    kBasicInfo = 0,      // 基本情况
    kPreferences = 1,    // 兴趣偏好
    kHabits = 2,         // 生活习惯
    kRecentEvents = 3,   // 近期事项
    kRelationships = 4   // 社交关系
};
```

#### 4.2.3 核心流程
```
用户输入 → 防抖处理 → 文本长度检查 → 排除规则过滤 
    → LLM特征识别 → 置信度判断 → UI提示 → 用户确认 → 加密存储
```

#### 4.2.4 存储设计
- 数据库：SQLite + SQLCipher 加密
- 表结构：
  ```sql
  CREATE TABLE features (
      id TEXT PRIMARY KEY,
      category TEXT NOT NULL,
      key TEXT NOT NULL,
      value TEXT NOT NULL,
      confidence REAL,
      source_text TEXT,
      enabled INTEGER DEFAULT 1,
      created_at INTEGER,
      updated_at INTEGER
  );
  
  CREATE INDEX idx_category ON features(category);
  CREATE INDEX idx_created ON features(created_at);
  ```

---

### 4.3 smart_enhance 模块

#### 4.3.1 模块结构
```
smart_enhance/
├── enhance_manager.h    # 增强管理器
├── enhance_manager.cc
├── scene_detector.h     # 场景识别器
├── scene_detector.cc
├── feature_matcher.h    # 特征匹配器
├── feature_matcher.cc
├── content_generator.h  # 内容生成器
├── content_generator.cc
└── app_registry.h       # AI应用注册表
    └── app_registry.cc
```

#### 4.3.2 场景识别
- 通过应用包名识别 AI 对话场景
- 预置常见 AI 应用列表
- 支持用户自定义添加

#### 4.3.3 特征匹配流程
```
用户输入 → 意图分析(LLM) → 特征库检索 → 相关度计算 
    → 阈值过滤 → 排序输出 → UI展示
```

#### 4.3.4 增强内容生成
- 调用 Prompt 2 生成自然语言增强内容
- 格式：原始输入 + "补充信息：" + 特征整合

---

### 4.4 privacy_mode 模块

#### 4.4.1 模块结构
```
privacy_mode/
├── privacy_controller.h # 无痕模式控制器
├── privacy_controller.cc
└── visual_indicator.h    # 视觉提示管理
    └── visual_indicator.cc
```

#### 4.4.2 核心功能
- 全局状态管理
- 自动关闭定时器
- 键盘视觉反馈
- 与特征识别模块的联动控制

---

## 五、数据流设计

### 5.1 个人特征识别数据流

```
┌──────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────┐
│ 用户输入  │───▶│ 文本预处理   │───▶│ LLM推理      │───▶│ 结果解析 │
└──────────┘    └──────────────┘    └──────────────┘    └──────────┘
                      │                                          │
                      ▼                                          ▼
               ┌──────────────┐                          ┌──────────────┐
               │ 排除规则过滤  │                          │ UI提示卡片   │
               └──────────────┘                          └──────────────┘
                                                                │
                                        ┌───────────────────────┴───────────────────────┐
                                        │                                               │
                                        ▼                                               ▼
                                ┌──────────────┐                               ┌──────────────┐
                                │ 用户确认保存 │                               │ 用户忽略     │
                                └──────┬───────┘                               └──────────────┘
                                       │
                                       ▼
                               ┌──────────────┐
                               │ 加密存储     │
                               │ (SQLCipher)  │
                               └──────────────┘
```

### 5.2 智能输入增强数据流

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ 场景识别     │───▶│ 意图分析     │───▶│ 特征匹配     │
│ (App包名)    │    │ (LLM)        │    │ (特征库)     │
└──────────────┘    └──────────────┘    └──────────────┘
                                               │
                                               ▼
                                       ┌──────────────┐
                                       │ 特征建议卡片 │
                                       │ (UI展示)     │
                                       └──────────────┘
                                               │
                                               ▼
                                       ┌──────────────┐
                                       │ 用户调整特征 │
                                       └──────────────┘
                                               │
                                               ▼
                                       ┌──────────────┐
                                       │ 内容生成     │
                                       │ (LLM)        │
                                       └──────────────┘
                                               │
                                               ▼
                                       ┌──────────────┐
                                       │ 发送增强内容 │
                                       └──────────────┘
```

---

## 六、接口设计

### 6.1 C++ API (librime 扩展)

```cpp
// rime_ai_api.h

// 特征识别 API
RIME_API bool rime_recognize_feature(RimeSessionId session,
                                     const char* text,
                                     PersonalFeature* result);

// 特征存储 API
RIME_API bool rime_save_feature(RimeSessionId session,
                                const PersonalFeature* feature);

RIME_API bool rime_get_features(RimeSessionId session,
                                const char* category,
                                PersonalFeature** features,
                                size_t* count);

// 智能增强 API
RIME_API bool rime_enhance_input(RimeSessionId session,
                                 const char* input,
                                 char** enhanced_output);

// 无痕模式 API
RIME_API void rime_set_privacy_mode(RimeSessionId session, bool enabled);
RIME_API bool rime_get_privacy_mode(RimeSessionId session);
```

### 6.2 Android JNI 接口

```java
// PersonalProfileManager.java
public class PersonalProfileManager {
    public void recognizeFeature(String text, FeatureCallback callback);
    public void saveFeature(PersonalFeature feature);
    public List<PersonalFeature> getFeatures(String category);
    public void deleteFeature(String featureId);
}

// SmartEnhanceManager.java
public class SmartEnhanceManager {
    public void matchFeatures(String input, MatchCallback callback);
    public void generateEnhancedContent(String input, 
                                        List<PersonalFeature> features,
                                        GenerateCallback callback);
    public boolean isAIApp(String packageName);
}

// PrivacyModeManager.java
public class PrivacyModeManager {
    public void setEnabled(boolean enabled);
    public boolean isEnabled();
    public void setAutoCloseDuration(int minutes);
}
```

---

## 七、配置管理

### 7.1 配置文件结构
```
data/
├── default.yaml           # 默认配置
├── ai_features.yaml       # AI功能配置
├── personal_profile.yaml  # 特征识别配置
├── smart_enhance.yaml     # 智能增强配置
├── privacy_mode.yaml      # 无痕模式配置
└── local_llm.yaml         # LLM配置
```

### 7.2 关键配置项

详见 PRD 第 5 节配置变更内容。

---

## 八、性能优化策略

### 8.1 LLM 推理优化
- 模型预加载，减少首次调用延迟
- 空闲自动卸载，控制内存占用
- GPU 加速（如设备支持）
- 批量推理，减少上下文切换

### 8.2 UI 响应优化
- 异步推理，不阻塞主线程
- 结果缓存，避免重复计算
- 防抖处理，减少无效调用

### 8.3 存储优化
- 特征索引，加速查询
- 定期清理过期特征
- 加密存储延迟加密策略

---

## 九、错误处理与降级策略

### 9.1 LLM 推理异常处理

| 异常类型 | 处理策略 |
|----------|----------|
| 推理超时 (>10s) | 取消当前请求，返回空结果，不提示用户 |
| 内存不足 | 自动卸载模型，降级为关闭 AI 功能 |
| 模型加载失败 | 提示用户检查模型文件，功能降级 |
| 输出解析失败 | 记录日志，丢弃结果，不影响正常输入 |

### 9.2 降级模式

```cpp
enum class LLMStatus {
    kNormal,        // 正常运行
    kDegraded,      // 降级模式（推理慢/不稳定）
    kDisabled       // 功能关闭
};
```

**降级触发条件**：
- 连续 3 次推理超时 → 进入降级模式，延长超时时间
- 连续 5 次失败 → 关闭 AI 功能，提示用户
- 内存占用 > 阈值 → 自动卸载模型

### 9.3 存储异常处理

| 异常类型 | 处理策略 |
|----------|----------|
| 数据库打开失败 | 使用内存缓存，定期重试持久化 |
| 写入失败 | 缓存到内存队列，下次启动重试 |
| 加密错误 | 重建数据库（丢失数据），记录日志 |

---

## 十、并发控制

### 10.1 推理请求队列

```
┌─────────────────────────────────────────────┐
│              Inference Request Queue        │
├─────────────────────────────────────────────┤
│  请求1 (进行中)  ←── 当前推理               │
│  请求2 (等待)                               │
│  请求3 (等待)                               │
└─────────────────────────────────────────────┘
```

**策略**：
- 单队列，串行执行（避免模型上下文切换开销）
- 新请求到达时，取消未执行的旧请求
- 最大等待时间：5秒，超时自动丢弃

### 10.2 输入防抖

```cpp
// 伪代码
void OnTextInput(text) {
    debounce_timer_.Cancel();  // 取消上次计时
    debounce_timer_.Start(500ms, [text]() {
        if (text.length() >= min_length && !privacy_mode_) {
            RequestInference(text);
        }
    });
}
```

---

## 十一、设备兼容性

### 11.1 最低要求

| 项目 | 要求 |
|------|------|
| Android 版本 | Android 8.0+ (API 26) |
| RAM | ≥ 4GB |
| 存储空间 | ≥ 3GB 可用（模型 + 数据） |
| CPU | ARM64 架构 |

### 11.2 推荐配置

| 项目 | 推荐配置 |
|------|----------|
| RAM | ≥ 6GB |
| GPU | 支持 OpenCL / Vulkan |
| 存储 | UFS / NVMe |

### 11.3 能力检测

```cpp
struct DeviceCapability {
    bool has_gpu;           // GPU 加速支持
    int ram_mb;             // 可用内存
    bool is_arm64;          // CPU 架构
    bool is_low_ram_device; // 低内存设备标记
};

// 启动时检测，决定是否启用 GPU 加速和模型预加载
DeviceCapability DetectCapability();
```

---

## 十二、依赖版本

| 依赖 | 版本 | 说明 |
|------|------|------|
| MLC-LLM Android SDK | ≥ 0.1.0 | 推理引擎 |
| SQLCipher | ≥ 4.5.0 | 加密存储 |
| Qwen2.5-1.5B-Instruct | MLC 编译版本 | 模型文件 |

### 12.1 模型格式要求

- 格式：MLC 编译后的模型（.so + params）
- 量化：INT4 优先（体积 ~800MB，速度更快）
- 备选：INT8（精度更高，体积 ~1.5GB）

---

## 十三、开发计划

### 13.1 MVP 开发计划（2-3天）

| 阶段 | 时间 | 任务 | 交付物 |
|------|------|------|--------|
| Day 1 | - | LLM 集成 + 特征识别基础 | local_llm 模块可运行，能调用 Prompt 识别特征 |
| Day 2 | - | 特征存储 + 智能增强 + UI | 特征可保存/读取，AI 场景触发增强，提示卡片 UI |
| Day 3 | - | 无痕模式 + 集成测试 + Bug 修复 | 完整 MVP 可用，基本测试通过 |

### 13.2 后续迭代
- P1: 手动选择特征功能
- P2: 其他平台适配
- 性能优化与用户体验改进

---

## 十四、风险评估

| 风险项 | 风险等级 | 应对措施 |
|--------|----------|----------|
| LLM 推理延迟过高 | 高 | 模型量化、GPU加速、异步处理 |
| 内存占用过大 | 中 | 模型动态加载、内存限制策略 |
| 隐私数据泄露 | 高 | SQLCipher 加密、本地化处理、无痕模式 |
| 特征识别准确率低 | 中 | Prompt 优化、置信度阈值调整 |
| 用户打扰过多 | 中 | 频率控制、冷却时间、智能降权 |

---

## 十五、参考资料

- PRD 文档: `docs/AI_FEATURES_PRD.md`
- librime API: `src/rime_api.h`
- Trime 项目: https://github.com/osfans/trime
- MLC-LLM: https://github.com/mlc-ai/mlc-llm