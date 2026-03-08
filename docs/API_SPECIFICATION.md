# Librim AI 接口定义

> 文档版本: v1.0
> 创建日期: 2026-03-06
> 基于文档: TECHNICAL_SOLUTION.md v1.1

---

## 一、C++ 头文件

### 1.1 feature_types.h - 特征类型定义

```cpp
#ifndef RIME_AI_FEATURE_TYPES_H_
#define RIME_AI_FEATURE_TYPES_H_

#include <string>
#include <cstdint>

namespace rime {
namespace ai {

// 特征类别枚举
enum class FeatureCategory {
    kBasicInfo = 0,      // 基本情况
    kPreferences = 1,    // 兴趣偏好
    kHabits = 2,         // 生活习惯
    kRecentEvents = 3,   // 近期事项
    kRelationships = 4   // 社交关系
};

// LLM 引擎状态
enum class LLMStatus {
    kNormal,             // 正常运行
    kDegraded,           // 降级模式
    kDisabled,           // 功能关闭
    kNotLoaded           // 模型未加载
};

// 推理结果
struct InferenceResult {
    bool success;
    std::string content;
    std::string error_message;
    int elapsed_ms;
};

// 个人特征
struct PersonalFeature {
    std::string id;
    FeatureCategory category;
    std::string key;
    std::string value;
    double confidence;
    std::string source_text;
    int64_t created_at;
    int64_t updated_at;
    bool enabled;

    PersonalFeature() 
        : category(FeatureCategory::kBasicInfo)
        , confidence(0.0)
        , created_at(0)
        , updated_at(0)
        , enabled(true) {}
};

// 特征识别结果
struct RecognitionResult {
    bool has_feature;
    PersonalFeature feature;
    std::string error_message;
};

// 特征匹配结果
struct MatchResult {
    std::string intent;
    std::vector<PersonalFeature> matched_features;
    std::string error_message;
};

// 设备能力
struct DeviceCapability {
    bool has_gpu;
    int ram_mb;
    bool is_arm64;
    bool is_low_ram_device;
};

// 类别转字符串
inline const char* FeatureCategoryToString(FeatureCategory cat) {
    switch (cat) {
        case FeatureCategory::kBasicInfo:    return "basic_info";
        case FeatureCategory::kPreferences:  return "preferences";
        case FeatureCategory::kHabits:       return "habits";
        case FeatureCategory::kRecentEvents: return "recent_events";
        case FeatureCategory::kRelationships:return "relationships";
        default: return "unknown";
    }
}

// 字符串转类别
inline FeatureCategory StringToFeatureCategory(const std::string& str) {
    if (str == "basic_info")     return FeatureCategory::kBasicInfo;
    if (str == "preferences")    return FeatureCategory::kPreferences;
    if (str == "habits")         return FeatureCategory::kHabits;
    if (str == "recent_events")  return FeatureCategory::kRecentEvents;
    if (str == "relationships")  return FeatureCategory::kRelationships;
    return FeatureCategory::kBasicInfo;
}

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_FEATURE_TYPES_H_
```

---

### 1.2 llm_engine.h - LLM 引擎接口

```cpp
#ifndef RIME_AI_LLM_ENGINE_H_
#define RIME_AI_LLM_ENGINE_H_

#include "feature_types.h"
#include <functional>
#include <memory>
#include <string>

namespace rime {
namespace ai {

// LLM 配置
struct LLMConfig {
    std::string model_path;
    int threads = 4;
    int gpu_layers = 32;
    int max_tokens = 512;
    float temperature = 0.3f;
    float top_p = 0.9f;
    int timeout_ms = 10000;
    bool preload_model = true;
    int unload_after_idle_sec = 300;
};

// 推理回调
using InferenceCallback = std::function<void(const InferenceResult&)>;

class LLMEngine {
public:
    static LLMEngine& Instance();

    // 初始化引擎
    bool Initialize(const LLMConfig& config);
    
    // 关闭引擎
    void Shutdown();
    
    // 同步推理
    InferenceResult Infer(const std::string& prompt);
    
    // 异步推理
    void InferAsync(const std::string& prompt, InferenceCallback callback);
    
    // 取消当前推理
    void Cancel();
    
    // 模型管理
    bool LoadModel();
    void UnloadModel();
    
    // 状态查询
    LLMStatus GetStatus() const;
    bool IsReady() const;
    
    // 获取设备能力
    static DeviceCapability DetectCapability();

private:
    LLMEngine() = default;
    ~LLMEngine() = default;
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_LLM_ENGINE_H_
```

---

### 1.3 profile_manager.h - 特征管理接口

```cpp
#ifndef RIME_AI_PROFILE_MANAGER_H_
#define RIME_AI_PROFILE_MANAGER_H_

#include "feature_types.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rime {
namespace ai {

// 特征识别回调
using RecognitionCallback = std::function<void(const RecognitionResult&)>;

// 特征列表回调
using FeatureListCallback = std::function<void(std::vector<PersonalFeature>)>;

class ProfileManager {
public:
    static ProfileManager& Instance();

    // 初始化
    bool Initialize();
    
    // 特征识别（异步）
    void RecognizeFeature(const std::string& text, RecognitionCallback callback);
    
    // 特征存储
    bool SaveFeature(const PersonalFeature& feature);
    bool UpdateFeature(const PersonalFeature& feature);
    bool DeleteFeature(const std::string& feature_id);
    
    // 特征查询
    void GetFeatures(FeatureCategory category, FeatureListCallback callback);
    void GetAllFeatures(FeatureListCallback callback);
    void SearchFeatures(const std::string& keyword, FeatureListCallback callback);
    
    // 特征统计
    int GetFeatureCount();
    int GetFeatureCount(FeatureCategory category);
    
    // 清理过期特征
    void CleanExpiredFeatures(int expire_days);
    
    // 检查是否应该识别（排除规则）
    bool ShouldRecognize(const std::string& text, 
                         const std::string& app_package,
                         const std::string& input_type);

private:
    ProfileManager() = default;
    ~ProfileManager() = default;
    ProfileManager(const ProfileManager&) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_PROFILE_MANAGER_H_
```

---

### 1.4 enhance_manager.h - 智能增强接口

```cpp
#ifndef RIME_AI_ENHANCE_MANAGER_H_
#define RIME_AI_ENHANCE_MANAGER_H_

#include "feature_types.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rime {
namespace ai {

// 增强内容生成回调
using EnhanceCallback = std::function<void(bool success, const std::string& enhanced_content)>;

// 匹配回调
using MatchCallback = std::function<void(const MatchResult&)>;

class EnhanceManager {
public:
    static EnhanceManager& Instance();

    // 初始化
    bool Initialize();
    
    // 检查是否为 AI 应用
    bool IsAIApp(const std::string& package_name);
    
    // 添加/移除 AI 应用
    void AddAIApp(const std::string& package_name);
    void RemoveAIApp(const std::string& package_name);
    
    // 特征匹配（异步）
    void MatchFeatures(const std::string& input, MatchCallback callback);
    
    // 生成增强内容（异步）
    void GenerateEnhancedContent(const std::string& original_input,
                                 const std::vector<PersonalFeature>& features,
                                 EnhanceCallback callback);
    
    // 设置最大特征数量
    void SetMaxFeatures(int max_features);
    
    // 设置最低相关度阈值
    void SetMinRelevance(float threshold);

private:
    EnhanceManager() = default;
    ~EnhanceManager() = default;
    EnhanceManager(const EnhanceManager&) = delete;
    EnhanceManager& operator=(const EnhanceManager&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_ENHANCE_MANAGER_H_
```

---

### 1.5 privacy_controller.h - 无痕模式接口

```cpp
#ifndef RIME_AI_PRIVACY_CONTROLLER_H_
#define RIME_AI_PRIVACY_CONTROLLER_H_

#include <functional>
#include <memory>

namespace rime {
namespace ai {

// 状态变更回调
using PrivacyStateCallback = std::function<void(bool enabled)>;

class PrivacyController {
public:
    static PrivacyController& Instance();

    // 初始化
    bool Initialize();
    
    // 获取/设置状态
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    
    // 切换状态
    void Toggle();
    
    // 设置自动关闭时间（分钟，0 表示不自动关闭）
    void SetAutoCloseDuration(int minutes);
    int GetAutoCloseDuration() const;
    
    // 注册状态变更监听
    void RegisterStateCallback(PrivacyStateCallback callback);
    
    // 重置为默认状态
    void Reset();

private:
    PrivacyController() = default;
    ~PrivacyController() = default;
    PrivacyController(const PrivacyController&) = delete;
    PrivacyController& operator=(const PrivacyController&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_PRIVACY_CONTROLLER_H_
```

---

### 1.6 rime_ai_api.h - 公共 API（供 Trime 调用）

```cpp
#ifndef RIME_AI_API_H_
#define RIME_AI_API_H_

#include "feature_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 初始化与关闭
// ============================================================

// 初始化 AI 模块
bool rime_ai_init(const char* config_path);

// 关闭 AI 模块
void rime_ai_shutdown();

// ============================================================
// 特征识别
// ============================================================

// 异步识别特征（回调在后台线程执行）
// callback: void(bool success, const char* json_result)
void rime_ai_recognize_feature(const char* text, void* callback);

// ============================================================
// 特征存储
// ============================================================

// 保存特征
bool rime_ai_save_feature(const char* feature_json);

// 删除特征
bool rime_ai_delete_feature(const char* feature_id);

// 获取所有特征（返回 JSON 字符串，需要调用方释放）
char* rime_ai_get_features(const char* category);

// ============================================================
// 智能增强
// ============================================================

// 检查是否为 AI 应用
bool rime_ai_is_ai_app(const char* package_name);

// 异步匹配特征
// callback: void(bool success, const char* json_result)
void rime_ai_match_features(const char* input, void* callback);

// 异步生成增强内容
// callback: void(bool success, const char* enhanced_content)
void rime_ai_generate_enhanced(const char* input, 
                               const char* features_json,
                               void* callback);

// ============================================================
// 无痕模式
// ============================================================

bool rime_ai_privacy_enabled();
void rime_ai_privacy_set_enabled(bool enabled);
void rime_ai_privacy_toggle();

// ============================================================
// 状态查询
// ============================================================

// 获取 LLM 状态（返回 LLMStatus 枚举值）
int rime_ai_get_llm_status();

// 检查是否应该识别（根据排除规则）
bool rime_ai_should_recognize(const char* text,
                              const char* app_package,
                              const char* input_type);

#ifdef __cplusplus
}
#endif

#endif  // RIME_AI_API_H_
```

---

## 二、Android JNI 接口

### 2.1 RimeAIBridge.java - JNI 桥接类

```java
package com.osfans.trime.ai;

import org.json.JSONArray;
import org.json.JSONObject;

/**
 * Librim AI 功能的 JNI 桥接类
 */
public class RimeAIBridge {
    
    static {
        System.loadLibrary("rime_ai");
    }
    
    // ============================================================
    // 单例模式
    // ============================================================
    
    private static volatile RimeAIBridge instance;
    
    public static RimeAIBridge getInstance() {
        if (instance == null) {
            synchronized (RimeAIBridge.class) {
                if (instance == null) {
                    instance = new RimeAIBridge();
                }
            }
        }
        return instance;
    }
    
    private RimeAIBridge() {}
    
    // ============================================================
    // Native 方法
    // ============================================================
    
    // 初始化
    private native boolean nativeInit(String configPath);
    private native void nativeShutdown();
    
    // 特征识别
    private native void nativeRecognizeFeature(String text, long callbackPtr);
    
    // 特征存储
    private native boolean nativeSaveFeature(String featureJson);
    private native boolean nativeDeleteFeature(String featureId);
    private native String nativeGetFeatures(String category);
    
    // 智能增强
    private native boolean nativeIsAIApp(String packageName);
    private native void nativeMatchFeatures(String input, long callbackPtr);
    private native void nativeGenerateEnhanced(String input, String featuresJson, long callbackPtr);
    
    // 无痕模式
    private native boolean nativePrivacyEnabled();
    private native void nativePrivacySetEnabled(boolean enabled);
    private native void nativePrivacyToggle();
    
    // 状态
    private native int nativeGetLLMStatus();
    private native boolean nativeShouldRecognize(String text, String appPackage, String inputType);
    
    // ============================================================
    // Java 公共接口
    // ============================================================
    
    public boolean init(String configPath) {
        return nativeInit(configPath);
    }
    
    public void shutdown() {
        nativeShutdown();
    }
    
    // 特征识别（异步）
    public void recognizeFeature(String text, RecognitionCallback callback) {
        nativeRecognizeFeature(text, callback != null ? callback.hashCode() : 0);
        // 实际实现中需要维护 callback 映射
    }
    
    // 保存特征
    public boolean saveFeature(PersonalFeature feature) {
        try {
            String json = feature.toJson();
            return nativeSaveFeature(json);
        } catch (Exception e) {
            return false;
        }
    }
    
    // 删除特征
    public boolean deleteFeature(String featureId) {
        return nativeDeleteFeature(featureId);
    }
    
    // 获取特征列表
    public List<PersonalFeature> getFeatures(String category) {
        String json = nativeGetFeatures(category);
        return PersonalFeature.fromJsonArray(json);
    }
    
    // 检查是否为 AI 应用
    public boolean isAIApp(String packageName) {
        return nativeIsAIApp(packageName);
    }
    
    // 匹配特征
    public void matchFeatures(String input, MatchCallback callback) {
        nativeMatchFeatures(input, callback != null ? callback.hashCode() : 0);
    }
    
    // 生成增强内容
    public void generateEnhanced(String input, List<PersonalFeature> features, 
                                 EnhanceCallback callback) {
        try {
            JSONArray arr = new JSONArray();
            for (PersonalFeature f : features) {
                arr.put(new JSONObject(f.toJson()));
            }
            nativeGenerateEnhanced(input, arr.toString(), callback != null ? callback.hashCode() : 0);
        } catch (Exception e) {
            if (callback != null) {
                callback.onResult(false, null, e.getMessage());
            }
        }
    }
    
    // 无痕模式
    public boolean isPrivacyEnabled() {
        return nativePrivacyEnabled();
    }
    
    public void setPrivacyEnabled(boolean enabled) {
        nativePrivacySetEnabled(enabled);
    }
    
    public void togglePrivacy() {
        nativePrivacyToggle();
    }
    
    // LLM 状态
    public int getLLMStatus() {
        return nativeGetLLMStatus();
    }
    
    // ============================================================
    // 回调接口
    // ============================================================
    
    public interface RecognitionCallback {
        void onResult(boolean success, PersonalFeature feature, String error);
    }
    
    public interface MatchCallback {
        void onResult(boolean success, List<PersonalFeature> features, String intent, String error);
    }
    
    public interface EnhanceCallback {
        void onResult(boolean success, String enhancedContent, String error);
    }
}
```

---

### 2.2 PersonalFeature.java - 特征数据类

```java
package com.osfans.trime.ai;

import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.List;

/**
 * 个人特征数据类
 */
public class PersonalFeature {
    
    public enum Category {
        BASIC_INFO("basic_info"),
        PREFERENCES("preferences"),
        HABITS("habits"),
        RECENT_EVENTS("recent_events"),
        RELATIONSHIPS("relationships");
        
        private final String value;
        Category(String value) { this.value = value; }
        public String getValue() { return value; }
        
        public static Category fromString(String str) {
            for (Category c : values()) {
                if (c.value.equals(str)) return c;
            }
            return BASIC_INFO;
        }
    }
    
    // 字段
    private String id;
    private Category category;
    private String key;
    private String value;
    private double confidence;
    private String sourceText;
    private long createdAt;
    private long updatedAt;
    private boolean enabled;
    
    // 构造函数
    public PersonalFeature() {
        this.enabled = true;
        this.confidence = 0.0;
    }
    
    // Getters & Setters
    public String getId() { return id; }
    public void setId(String id) { this.id = id; }
    
    public Category getCategory() { return category; }
    public void setCategory(Category category) { this.category = category; }
    
    public String getKey() { return key; }
    public void setKey(String key) { this.key = key; }
    
    public String getValue() { return value; }
    public void setValue(String value) { this.value = value; }
    
    public double getConfidence() { return confidence; }
    public void setConfidence(double confidence) { this.confidence = confidence; }
    
    public String getSourceText() { return sourceText; }
    public void setSourceText(String sourceText) { this.sourceText = sourceText; }
    
    public long getCreatedAt() { return createdAt; }
    public void setCreatedAt(long createdAt) { this.createdAt = createdAt; }
    
    public long getUpdatedAt() { return updatedAt; }
    public void setUpdatedAt(long updatedAt) { this.updatedAt = updatedAt; }
    
    public boolean isEnabled() { return enabled; }
    public void setEnabled(boolean enabled) { this.enabled = enabled; }
    
    // JSON 序列化
    public String toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("id", id);
            json.put("category", category != null ? category.getValue() : "basic_info");
            json.put("key", key);
            json.put("value", value);
            json.put("confidence", confidence);
            json.put("source_text", sourceText);
            json.put("created_at", createdAt);
            json.put("updated_at", updatedAt);
            json.put("enabled", enabled);
        } catch (Exception e) {
            return "{}";
        }
        return json.toString();
    }
    
    // JSON 反序列化
    public static PersonalFeature fromJson(String jsonStr) {
        try {
            JSONObject json = new JSONObject(jsonStr);
            PersonalFeature feature = new PersonalFeature();
            feature.setId(json.optString("id"));
            feature.setCategory(Category.fromString(json.optString("category")));
            feature.setKey(json.optString("key"));
            feature.setValue(json.optString("value"));
            feature.setConfidence(json.optDouble("confidence", 0.0));
            feature.setSourceText(json.optString("source_text"));
            feature.setCreatedAt(json.optLong("created_at"));
            feature.setUpdatedAt(json.optLong("updated_at"));
            feature.setEnabled(json.optBoolean("enabled", true));
            return feature;
        } catch (Exception e) {
            return null;
        }
    }
    
    // JSON 数组反序列化
    public static List<PersonalFeature> fromJsonArray(String jsonStr) {
        List<PersonalFeature> list = new ArrayList<>();
        try {
            JSONArray arr = new JSONArray(jsonStr);
            for (int i = 0; i < arr.length(); i++) {
                PersonalFeature f = fromJson(arr.getString(i));
                if (f != null) list.add(f);
            }
        } catch (Exception e) {
            // ignore
        }
        return list;
    }
}
```

---

## 三、数据结构说明

### 3.1 JSON 格式示例

**PersonalFeature JSON:**
```json
{
    "id": "feat_abc123",
    "category": "preferences",
    "key": "food_preference",
    "value": "喜欢吃辣",
    "confidence": 0.85,
    "source_text": "我最爱吃辣的菜",
    "created_at": 1709692800000,
    "updated_at": 1709692800000,
    "enabled": true
}
```

**MatchResult JSON:**
```json
{
    "intent": "用户想要规划旅行",
    "matched_features": [
        {
            "id": "feat_001",
            "category": "preferences",
            "key": "travel_preference",
            "value": "喜欢海岛游",
            "confidence": 0.9
        },
        {
            "id": "feat_002",
            "category": "habits",
            "key": "family_info",
            "value": "已婚，有一个孩子",
            "confidence": 0.95
        }
    ]
}
```

---

## 四、错误码定义

| 错误码 | 说明 |
|--------|------|
| 0 | 成功 |
| -1 | 未知错误 |
| -2 | 模型未加载 |
| -3 | 推理超时 |
| -4 | 内存不足 |
| -5 | 解析失败 |
| -6 | 存储错误 |
| -7 | 无效参数 |
| -8 | 无痕模式下禁止操作 |