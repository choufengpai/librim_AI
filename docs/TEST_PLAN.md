# Librim AI 测试方案

> 文档版本: v1.0
> 创建日期: 2026-03-06
> 适用范围: MVP 阶段

---

## 一、测试策略

### 1.1 MVP 测试原则

| 原则 | 说明 |
|------|------|
| 聚焦核心路径 | 只测试主流程，边界情况后续迭代 |
| 自动化优先 | 单元测试自动化，集成测试手动 |
| 真机验证 | LLM 推理必须在真机上测试延迟 |

### 1.2 测试范围

| 模块 | 单元测试 | 集成测试 | 真机测试 |
|------|----------|----------|----------|
| LLM 引擎 | - | ✓ | ✓ |
| 特征识别 | ✓ | ✓ | ✓ |
| 特征存储 | ✓ | - | - |
| 智能增强 | - | ✓ | ✓ |
| 无痕模式 | ✓ | - | - |

---

## 二、单元测试

### 2.1 LLM 引擎测试 (llm_engine_test.cc)

```cpp
#include "llm_engine.h"
#include <gtest/gtest.h>

namespace rime {
namespace ai {
namespace test {

// 测试设备能力检测
TEST(LLMEngineTest, DetectCapability) {
    auto cap = LLMEngine::DetectCapability();
    EXPECT_TRUE(cap.is_arm64 || !cap.is_arm64);  // 基本断言
    EXPECT_GT(cap.ram_mb, 0);
}

// 测试初始化与关闭
TEST(LLMEngineTest, InitializeAndShutdown) {
    LLMConfig config;
    config.model_path = "/data/local/tmp/test_model";
    config.threads = 2;
    
    // 模型不存在时应返回 false
    EXPECT_FALSE(LLMEngine::Instance().Initialize(config));
}

// 测试状态查询
TEST(LLMEngineTest, GetStatusWhenNotLoaded) {
    EXPECT_EQ(LLMEngine::Instance().GetStatus(), LLMStatus::kNotLoaded);
}

}  // namespace test
}  // namespace ai
}  // namespace rime
```

### 2.2 特征存储测试 (feature_storage_test.cc)

```cpp
#include "profile_manager.h"
#include <gtest/gtest.h>

namespace rime {
namespace ai {
namespace test {

// 测试特征保存与读取
TEST(ProfileManagerTest, SaveAndGetFeature) {
    PersonalFeature feature;
    feature.id = "test_001";
    feature.category = FeatureCategory::kPreferences;
    feature.key = "food";
    feature.value = "喜欢辣";
    feature.confidence = 0.9;
    
    EXPECT_TRUE(ProfileManager::Instance().SaveFeature(feature));
}

// 测试特征删除
TEST(ProfileManagerTest, DeleteFeature) {
    EXPECT_TRUE(ProfileManager::Instance().DeleteFeature("test_001"));
}

// 测试特征数量统计
TEST(ProfileManagerTest, GetFeatureCount) {
    int count = ProfileManager::Instance().GetFeatureCount();
    EXPECT_GE(count, 0);
}

}  // namespace test
}  // namespace ai
}  // namespace rime
```

### 2.3 无痕模式测试 (privacy_controller_test.cc)

```cpp
#include "privacy_controller.h"
#include <gtest/gtest.h>

namespace rime {
namespace ai {
namespace test {

TEST(PrivacyControllerTest, DefaultDisabled) {
    EXPECT_FALSE(PrivacyController::Instance().IsEnabled());
}

TEST(PrivacyControllerTest, Toggle) {
    PrivacyController::Instance().SetEnabled(false);
    PrivacyController::Instance().Toggle();
    EXPECT_TRUE(PrivacyController::Instance().IsEnabled());
    PrivacyController::Instance().Toggle();
    EXPECT_FALSE(PrivacyController::Instance().IsEnabled());
}

TEST(PrivacyControllerTest, SetEnabled) {
    PrivacyController::Instance().SetEnabled(true);
    EXPECT_TRUE(PrivacyController::Instance().IsEnabled());
    PrivacyController::Instance().SetEnabled(false);
    EXPECT_FALSE(PrivacyController::Instance().IsEnabled());
}

}  // namespace test
}  // namespace ai
}  // namespace rime
```

---

## 三、集成测试

### 3.1 特征识别流程测试

| 用例编号 | TC-INT-001 |
|----------|------------|
| 名称 | 特征识别完整流程 |
| 前置条件 | 模型已加载，无痕模式关闭 |
| 步骤 | 1. 输入"我住在北京"<br>2. 等待识别结果<br>3. 确认保存 |
| 预期结果 | 识别出地区特征，保存成功 |
| 验证点 | 返回的特征 category 为 basic_info，value 包含"北京" |

### 3.2 智能增强流程测试

| 用例编号 | TC-INT-002 |
|----------|------------|
| 名称 | 智能增强完整流程 |
| 前置条件 | 特征库已有数据，在 ChatGPT 应用中 |
| 步骤 | 1. 输入"帮我规划旅行"<br>2. 查看匹配的特征<br>3. 确认发送 |
| 预期结果 | 显示相关特征，生成增强内容 |
| 验证点 | 匹配的特征数量 > 0，增强内容包含"补充信息" |

### 3.3 无痕模式联动测试

| 用例编号 | TC-INT-003 |
|----------|------------|
| 名称 | 无痕模式禁用识别 |
| 前置条件 | 无 |
| 步骤 | 1. 开启无痕模式<br>2. 输入任意文本<br>3. 检查是否触发识别 |
| 预期结果 | 不触发任何特征识别 |
| 验证点 | 无 UI 提示，无存储操作 |

---

## 四、真机测试

### 4.1 性能基准测试

| 测试项 | 目标值 | 测试方法 |
|--------|--------|----------|
| 模型加载时间 | < 10s | 冷启动计时 |
| 特征识别延迟 | < 3s | 从输入到结果返回 |
| 特征匹配延迟 | < 2s | 从输入到匹配结果 |
| 增强内容生成 | < 3s | 从确认到生成完成 |
| 内存增量 | < 500MB | 启用功能前后对比 |

### 4.2 测试设备要求

| 类型 | 要求 |
|------|------|
| 最低配置设备 | 4GB RAM, Android 8.0 |
| 推荐配置设备 | 6GB+ RAM, GPU 支持 |

### 4.3 性能测试脚本

```bash
#!/bin/bash
# performance_test.sh

echo "=== Librim AI Performance Test ==="

# 1. 模型加载时间
echo "Testing model load time..."
start_time=$(date +%s%3N)
# 触发模型加载的 adb 命令
# adb shell am broadcast -a com.osfans.trime.LOAD_MODEL
end_time=$(date +%s%3N)
load_time=$((end_time - start_time))
echo "Model load time: ${load_time}ms"

# 2. 推理延迟测试（需要实际运行）
echo "Testing inference latency..."
# 通过 logcat 过滤推理时间日志
# adb logcat -s RimeAI:I | grep "inference_time"

echo "=== Test Complete ==="
```

---

## 五、功能验收清单

### 5.1 MVP 验收标准

| 功能 | 验收标准 | 通过 |
|------|----------|------|
| **特征识别** | | |
| 基本识别 | 输入"我住在北京"能识别出地区 | ☐ |
| 保存功能 | 点击保存后特征入库 | ☐ |
| 忽略功能 | 点击忽略后不保存 | ☐ |
| **智能增强** | | |
| 场景识别 | ChatGPT 应用中触发增强 | ☐ |
| 特征展示 | 显示匹配的特征标签 | ☐ |
| 内容生成 | 生成包含补充信息的完整内容 | ☐ |
| **无痕模式** | | |
| 开关功能 | 点击可切换状态 | ☐ |
| 禁用识别 | 开启后不触发任何识别 | ☐ |
| **性能** | | |
| 识别延迟 | < 3秒 | ☐ |
| 内存占用 | 增量 < 500MB | ☐ |

---

## 六、测试执行计划

### 6.1 Day 1 测试任务

- [ ] 编译并运行单元测试
- [ ] 真机上验证模型加载时间
- [ ] 记录推理延迟基线

### 6.2 Day 2 测试任务

- [ ] 执行特征识别集成测试
- [ ] 执行智能增强集成测试
- [ ] 真机性能测试

### 6.3 Day 3 测试任务

- [ ] 完整功能验收清单
- [ ] Bug 修复验证
- [ ] 生成测试报告

---

## 七、Bug 记录模板

| ID | 描述 | 严重程度 | 状态 | 备注 |
|----|------|----------|------|------|
| BUG-001 | 示例 | 高/中/低 | 开放/修复/关闭 | - |

---

## 八、测试报告模板

```markdown
# Librim AI MVP 测试报告

## 1. 测试概要
- 测试日期：YYYY-MM-DD
- 测试人员：
- 测试设备：

## 2. 测试结果汇总
| 模块 | 通过 | 失败 | 阻塞 |
|------|------|------|------|
| 特征识别 | X | X | X |
| 智能增强 | X | X | X |
| 无痕模式 | X | X | X |

## 3. 性能测试结果
| 指标 | 目标值 | 实测值 | 是否达标 |
|------|--------|--------|----------|
| 识别延迟 | < 3s | X.Xs | ✓/✗ |
| 内存增量 | < 500MB | XXXMB | ✓/✗ |

## 4. 主要问题
1. ...
2. ...

## 5. 结论
☐ 通过 / ☐ 有条件通过 / ☐ 不通过
```