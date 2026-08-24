# Qwen 模型迁移调研报告

> 文档版本: v2.0
> 创建日期: 2026-03-08
> 最后更新: 2026-08-20
> 状态: 已决策并实施（llama.cpp + Qwen3.5-2B，见第十一节）

---

## 一、背景

当前项目使用 **Qwen2.5-1.5B-Instruct** 作为本地 LLM，基于 MLC-LLM 推理引擎部署。2026年3月2日，阿里巴巴发布了 **Qwen3.5 系列小模型**（0.8B/2B/4B/9B），引发模型选型重新评估的需求。

**重要发现**：在 Qwen2.5 和 Qwen3.5 之间，还有 **Qwen3 系列**（2025年4月发布），其中包含已获 MLC-LLM 支持的端侧小模型。

---

## 二、候选模型概览

### 2.1 Qwen 系列时间线

```
2024-09  Qwen2.5  (1.5B, 3B, 7B, 14B, 32B, 72B)
    ↓
2025-04  Qwen3    (0.6B, 1.7B, 4B, 8B, 14B, 32B, 30B-A3B, 235B-A22B)  ← 新发现
    ↓
2026-03  Qwen3.5  (0.8B, 2B, 4B, 9B, 27B, 35B-A3B, 122B-A10B, 397B-A17B)
```

### 2.2 端侧小模型对比

| 模型 | 参数量 | 架构 | 上下文 | Thinking Mode | MLC-LLM | 多模态 |
|------|--------|------|--------|---------------|---------|--------|
| **Qwen2.5-1.5B** | 1.54B | Dense | 32K | ❌ | ✅ 支持 | ❌ 仅文本 |
| **Qwen3-0.6B** | 0.44B | Dense | 32K | ✅ | ✅ **已支持** | ❌ 仅文本 |
| **Qwen3-1.7B** | 1.4B | Dense | 32K | ✅ | ✅ **已支持** | ❌ 仅文本 |
| **Qwen3.5-0.8B** | 0.8B | Gated DeltaNet | 262K | ✅ | ❌ 不支持 | ✅ 多模态 |
| **Qwen3.5-2B** | 2B | Gated DeltaNet | 262K | ✅ | ❌ 不支持 | ✅ 多模态 |

> 🎯 **关键发现**：Qwen3-1.7B 是最佳候选 - 参数量接近、MLC-LLM 已支持、具备 Thinking Mode

---

## 三、详细参数对比

### 3.1 基础参数

| 维度 | Qwen2.5-1.5B | Qwen3-0.6B | **Qwen3-1.7B** | Qwen3.5-2B |
|------|-------------|------------|-----------------|------------|
| **参数量** | 1.54B | 0.44B | **1.4B** | 2B |
| **层数** | 28 | 28 | **28** | 24 |
| **隐藏维度** | 1,536 | 896 | **2,048** | 2,048 |
| **注意力头 (Q/KV)** | 12/2 | 16/8 | **16/8** | 8/2 |
| **上下文窗口** | 32K | 32K | **32K** | 262K |
| **Thinking Mode** | ❌ | ✅ | **✅** | ✅ |
| **多模态** | ❌ | ❌ | **❌** | ✅ |
| **语言支持** | 中英文 | 119语言 | **119语言** | 201语言 |
| **发布时间** | 2024-09 | 2025-04 | **2025-04** | 2026-03 |
| **许可证** | Apache 2.0 | Apache 2.0 | **Apache 2.0** | Apache 2.0 |

### 3.2 性能基准对比

| 基准测试 | Qwen2.5-1.5B | Qwen3-0.6B | **Qwen3-1.7B** | Qwen3.5-2B |
|----------|-------------|------------|-----------------|------------|
| **C-Eval** | ~60 (估) | ~50 | **~65** | 73.2 |
| **MMLU** | ~55 (估) | ~45 | **~62** | ~70 |
| **GSM8K** | ~45 (估) | ~35 | **~55** | ~65 |

> 注：Qwen3 官方声明：**Qwen3-1.7B 性能 ≈ Qwen2.5-3B**（跨代性能提升）

### 3.3 部署资源需求

| 量化级别 | Qwen2.5-1.5B | Qwen3-0.6B | **Qwen3-1.7B** | Qwen3.5-2B |
|----------|-------------|------------|-----------------|------------|
| **BF16** | ~3 GB | ~1.2 GB | **~3.4 GB** | ~4 GB |
| **8-bit** | ~1.5 GB | ~0.6 GB | **~1.7 GB** | ~2 GB |
| **4-bit** | ~800 MB | ~350 MB | **~900 MB** | ~1.5 GB |

---

## 四、架构差异分析

### 4.1 Qwen2.5-1.5B 架构

- **类型**: 标准 Transformer Decoder-only
- **注意力机制**: 全注意力 (Full Attention) + GQA
- **计算复杂度**: O(L²) 二次复杂度
- **KV Cache**: 标准 KV Cache

### 4.2 Qwen3-1.7B 架构

- **类型**: 标准 Transformer Decoder-only（与 Qwen2.5 相同）
- **注意力机制**: 全注意力 + GQA
- **Thinking Mode**: 通过训练方式支持，**非架构变更**
- **改进点**:
  - 训练数据翻倍 (18T → 36T tokens)
  - 强化学习增强推理能力
  - 支持 119 种语言

### 4.3 Qwen3.5-2B 架构

Qwen3.5 引入全新的混合架构：

#### Gated DeltaNet (75% 层)
- **类型**: 线性注意力机制
- **计算复杂度**: O(L) 线性复杂度
- **内存效率**: 比 KV Cache 更高效

#### Gated Full Attention (25% 层)
- **类型**: 标准 GQA 注意力

#### 架构差异总结

| 维度 | Qwen2.5 / Qwen3 | Qwen3.5 | 说明 |
|------|-----------------|---------|------|
| 架构类型 | 标准 Dense | Gated DeltaNet 混合 | Qwen3.5 全新架构 |
| 长上下文 | KV Cache 内存增长 | 线性内存增长 | Qwen3.5 优势 |
| MLC-LLM | ✅ 易支持 | ❌ 需新算子 | Qwen3 更易部署 |

---

## 五、推理引擎支持情况

### 5.1 MLC-LLM 支持状态

| 模型 | 支持状态 | 预编译模型 |
|------|----------|-----------|
| Qwen2.5-1.5B | ✅ 完全支持 | ✅ 有 |
| **Qwen3-0.6B** | ✅ **完全支持** | ✅ [mlc-ai/Qwen3-0.6B-q0f16-MLC](https://huggingface.co/mlc-ai/Qwen3-0.6B-q0f16-MLC) |
| **Qwen3-1.7B** | ✅ **完全支持** | 需自行编译 |
| Qwen3.5-2B | ❌ 不支持 | - |

### 5.2 Qwen3.5 MLC-LLM 阻塞原因

GitHub Issue: [mlc-ai/web-llm #778](https://github.com/mlc-ai/web-llm/issues/778)

Gated DeltaNet 架构需要 TVM 编译器新增算子支持：
1. Gated DeltaNet 线性注意力算子
2. 混合层调度 (3:1 交替模式)
3. Multi-Token Prediction (MTP)

**预估支持时间**: 2周 - 2个月

### 5.3 llama.cpp 支持状态

| 模型 | 支持状态 | GGUF 可用 |
|------|----------|-----------|
| Qwen2.5-1.5B | ✅ | ✅ |
| Qwen3-0.6B | ✅ | ✅ |
| Qwen3-1.7B | ✅ | ✅ |
| Qwen3.5-2B | ✅ | ✅ |

---

## 六、迁移方案分析

### 6.1 方案一：迁移到 Qwen3-1.7B（推荐）

**优点**:
- ✅ MLC-LLM **已支持**，可立即使用
- ✅ Thinking Mode 支持复杂推理
- ✅ 参数量接近，体积增量小 (~12%)
- ✅ 119 语言支持
- ✅ 架构相同，迁移成本低

**缺点**:
- 无多模态能力
- 上下文仍为 32K

**风险评估**: **低风险**

**开发工作量**: 1-2 天（模型替换 + Prompt 适配）

### 6.2 方案二：继续使用 Qwen2.5-1.5B

**优点**:
- 无需任何变更
- 已验证稳定

**缺点**:
- 无 Thinking Mode
- 性能不如 Qwen3

**风险评估**: 低风险

### 6.3 方案三：等待 Qwen3.5 MLC-LLM 支持

**优点**:
- 最新架构，性能最优
- 多模态能力
- 262K 上下文

**缺点**:
- 时间不确定
- 体积增加 87%

**风险评估**: 中等风险

### 6.4 方案对比表

| 方案 | 开发成本 | 风险 | 性能提升 | 体积变化 | 推荐度 |
|------|----------|------|----------|----------|--------|
| 迁移到 Qwen3-1.7B | 低 | 低 | 中等 (+10-15%) | +12% | ⭐⭐⭐⭐⭐ |
| 继续用 Qwen2.5-1.5B | 无 | 低 | 无 | 无 | ⭐⭐⭐ |
| 等待 Qwen3.5-2B | 中 | 中 | 高 (+20-30%) | +87% | ⭐⭐ |

---

## 七、Qwen3 Thinking Mode 说明

Qwen3 支持 **混合思考模式**，可在同一模型中切换：

### 7.1 两种模式

| 模式 | 特点 | 适用场景 |
|------|------|----------|
| **Non-Thinking** | 快速响应，类似 Qwen2.5 | 简单查询、日常对话 |
| **Thinking** | 深度推理，逐步思考 | 复杂问题、特征识别 |

### 7.2 使用方式

```python
# 启用 Thinking Mode（默认）
text = tokenizer.apply_chat_template(
    messages,
    enable_thinking=True  # 默认值
)

# 禁用 Thinking Mode
text = tokenizer.apply_chat_template(
    messages,
    enable_thinking=False
)

# 运行时动态切换（在 prompt 中添加）
user_input = "帮我分析这段文本 /think"     # 强制思考
user_input = "快速回答 /no_think"          # 快速响应
```

### 7.3 对输入法场景的意义

| 功能 | 推荐模式 | 原因 |
|------|----------|------|
| 特征识别 | Thinking | 需要准确分析用户输入 |
| 智能增强 | Thinking | 需要理解上下文意图 |
| 快速补全 | Non-Thinking | 响应速度优先 |

---

## 八、决策建议

### 8.1 推荐方案：迁移到 Qwen3-1.7B

**理由**:
1. **MLC-LLM 已支持** - 无需等待，可立即开发
2. **Thinking Mode** - 提升特征识别准确率
3. **迁移成本低** - 架构相同，仅需模型替换
4. **体积增量小** - 4-bit 量化仅增加 ~100MB

### 8.2 实施计划

| 阶段 | 时间 | 任务 |
|------|------|------|
| Day 1 | 0.5 天 | 编译 Qwen3-1.7B MLC 模型 |
| Day 1-2 | 1 天 | 替换模型，适配 Prompt |
| Day 2-3 | 1 天 | 测试 Thinking Mode 效果 |
| Day 3-4 | 1 天 | 性能调优，内存测试 |

### 8.3 后续规划

1. **短期**：完成 Qwen3-1.7B 迁移
2. **中期**：持续关注 MLC-LLM 对 Qwen3.5 的支持
3. **长期**：评估 Qwen3.5 多模态能力在输入法场景的价值

---

## 九、参考链接

### Qwen3
- [Qwen3 官方博客](https://qwenlm.github.io/blog/qwen3/)
- [Qwen3 HuggingFace](https://huggingface.co/collections/Qwen/qwen3-67dd247413f0e2e4f653967f)
- [MLC-LLM Qwen3-0.6B](https://huggingface.co/mlc-ai/Qwen3-0.6B-q0f16-MLC)

### Qwen3.5
- [Qwen3.5 官方博客](https://qwen.ai/blog?id=qwen3.5)
- [Qwen3.5-2B HuggingFace](https://huggingface.co/Qwen/Qwen3.5-2B)
- [MLC-LLM Qwen3.5 支持请求](https://github.com/mlc-ai/web-llm/issues/778)

### 其他
- [Gated DeltaNet 论文](https://arxiv.org/abs/2412.06464)
- [llama.cpp Qwen 支持](https://github.com/ggml-org/llama.cpp)

---

## 十、更新日志

| 日期 | 版本 | 更新内容 |
|------|------|----------|
| 2026-03-08 | v1.0 | 初始调研报告 |
| 2026-03-08 | v1.1 | 新增 Qwen3 系列调研，推荐 Qwen3-1.7B 迁移方案 |
| 2026-08-20 | v2.0 | **最终决策**:切换推理引擎至 llama.cpp，选型 Qwen3.5-2B Q4_K_M；记录集成方式与实施结果（见第十一节） |

---

## 十一、最终决策与实施（2026-08-20）

### 11.1 决策结果

**推理引擎: MLC-LLM → llama.cpp，模型: Qwen2.5-1.5B → Qwen3.5-2B-Instruct GGUF (Q4_K_M)**

推翻 v1.1 推荐的「Qwen3-1.7B + MLC-LLM」方案，理由：

1. **MLC-LLM 对 Qwen3.5 的支持始终未落地**（Gated DeltaNet 需 TVM 新算子，预估 2 周 - 2 个月，5 个月后仍无进展）
2. **llama.cpp 已确认支持 Qwen3.5 全系**，且其 Android JNI 集成模式（官方 llama-android 示例）成熟
3. **原有 MLC 集成本为 mock**（`MLCService.kt` 为桩实现，推理链路从未真实跑通），切换成本≈0
4. Qwen3.5-2B 相比 Qwen3-1.7B：多模态、262K 上下文、线性注意力内存友好，更适合输入法长期演进

### 11.2 模型选型

| 项 | 值 |
|------|------|
| 模型 | Qwen3.5-2B-Instruct |
| 量化 | Q4_K_M |
| 文件 | `Qwen3.5-2B-Q4_K_M.gguf`（1.28 GB） |
| 来源 | [unsloth/Qwen3.5-2B-GGUF](https://huggingface.co/unsloth/Qwen3.5-2B-GGUF)（Apache 2.0） |
| 运行内存预估 | ~2 GB（超限则降级 Qwen3.5-0.8B） |

### 11.3 集成方式（已实施）

#### Android 推理链路（真实）

```
Trime Java 层 (RimeAi.aiRecognizeFeature 等)
  → ai_module_jni.cc (RimeAi 全套 JNI 实现)
  → ProfileManager → PromptManager (业务 prompt)
  → LLMEngine::Infer
  → [Android] LLMJNIBridge → Java LlamaService.inference(prompt, requestId)
  → llama_jni.cc nativeGenerate → llama.cpp (GGUF 推理)
  → LLMNative.onInferenceResult 回调 C++ → 结果返回调用方
```

#### 代码变更清单

| 文件 | 变更 |
|------|------|
| `android/ai/.../LlamaService.kt` | 新增，替代 MLCService.kt（已删除）：GGUF 加载、推理、流式输出 |
| `android/ai/.../LLMNative.kt` | 新增 `setInferenceService` 声明（桥初始化入口） |
| `src/rime/ai/llama_jni.cc` | 新增，llama.cpp JNI 桥（参考官方 llama-android 示例） |
| `src/rime/ai/llm_jni.cc/.h` | MLCService → LlamaService 类名与桥初始化入口，修复回调线程问题 |
| `src/rime/ai/llm_engine.cc` | Android 下 ProcessRequest/LoadModel 走 JNI 桥；桌面保留 MockInference |
| `src/rime/ai/ai_module_jni.cc` | 新增，实现 RimeAi.kt 全套 JNI 声明（此前完全缺失） |
| `src/rime/ai/llm_engine.cc` (PromptManager) | 三个业务 prompt 模板适配 thinking 模型输出约束 |
| `cmake/RimeAiConfig.cmake` + `src/CMakeLists.txt` | Android 构建集成 llama.cpp 子项目（vendored 于 `llama.cpp/`，gitignored） |

#### Qwen3.5 ChatML 模板适配

- `llama_jni.cc` 内置 ChatML 拼装（`<|im_start|>...<|im_end|>`，兼容 Qwen2/Qwen2.5/Qwen3/Qwen3.5）
- **低延迟场景默认关闭思考模式**：`LlamaConfig.noThink = true`，在 user 消息末尾附加 `/no_think`
- 兕底：输出后处理剥离 `<think>...</think>` 块（`/no_think` 未生效时保证返回纯文本）

### 11.4 部署方式（2026-08-20 真机验证后更新）

1. 模型不进 APK（体积考虑），通过 stdin 流式写入应用**内部存储**（免 root，debuggable 包适用）：
   ```bash
   adb shell "run-as com.osfans.trime.debug sh -c 'mkdir -p files/models \
     && cat > files/models/qwen3.5-2b-instruct-q4_k_m.gguf'" \
     < ~/Developer/models/Qwen3.5-2B-Q4_K_M.gguf
   ```
2. **不要用 `adb push` 到外部私有目录**：Android 11+ scoped storage 下，push 的文件 FUSE 属主为 shell，
   app 进程 `stat` 即被拒（实测 vivo Android 16 `run-as` 下 `Permission denied`），
   表现为 `File.exists()` 返回 false、懒加载时模型加载失败。内部存储 `run-as` 流式写入后属主为 app，可正常读写。
3. `TrimeApplication.initializeLibrimAi()` 模型路径为 external→internal 双 fallback：
   外部私有目录 `files/models/`（用户可放置）→ 内部 `files/models/`（开发期 adb 部署）；
   选中的路径经 `aiInit` 的 `model_path` 传入（兼容 debug 包名后缀）
4. 桌面（macOS/Linux）构建仍为 MockInference，仅链路联调用；真实验证在 Android 真机
5. APK 编译产物：`com.osfans.trime-2a79963-arm64-v8a-debug.apk`（41.7MB，librime_jni.so 静态链入 llama.cpp + vendored SQLite 3.53.4）
6. 真机验证（vivo V2454A / Android 16）：`model present: true`，aiInit success，
   LLMJNIBridge/LlamaService 桥注册链路全部打通（logcat LibrimAI_* 系列日志）

### 11.5 待验证项（真机验收，随阶段 5）

- [ ] 特征识别返回真实 LLM 结果（非 mock 固定值）
- [ ] 单次推理延迟（PRD 目标：< 3s）
- [ ] 内存占用实测（超 2GB 预算则降级 Qwen3.5-0.8B）
- [ ] `/no_think` 生效确认（输出无 `<think>` 块）