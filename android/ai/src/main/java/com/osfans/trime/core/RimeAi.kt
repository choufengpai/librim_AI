/**
 * Copyright Librim AI Developers
 * Distributed under the BSD License
 *
 * 2026-03-08 Librim AI Team
 *
 * Rime AI 扩展接口
 * 为 Trime 提供 AI 功能的 JNI 接口
 */

package com.osfans.trime.core

import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * AI 功能状态
 */
enum class AIState {
    DISABLED,       // 未启用
    INITIALIZING,   // 初始化中
    READY,          // 就绪
    INFERRING,      // 推理中
    ERROR           // 错误状态
}

/**
 * 特征识别结果
 */
data class FeatureRecognitionResult(
    val hasFeature: Boolean,
    val featureId: String = "",
    val category: Int = 0,
    val key: String = "",
    val value: String = "",
    val confidence: Double = 0.0,
    val shouldPrompt: Boolean = false,
    val suggestionText: String = ""
)

/**
 * 特征匹配结果
 */
data class FeatureMatchResult(
    val intent: String,
    val matchedFeatures: Array<FeatureInfo>,
    val confidence: Double = 0.0
)

/**
 * 特征信息
 */
data class FeatureInfo(
    val id: String,
    val category: Int,
    val key: String,
    val value: String,
    val confidence: Double,
    val enabled: Boolean
)

/**
 * 场景信息
 */
data class SceneInfo(
    val type: Int,
    val appPackage: String,
    val appName: String,
    val shouldEnhance: Boolean
)

/**
 * Rime AI API 接口
 */
interface RimeAiApi {
    // 状态
    val aiState: StateFlow<AIState>
    val isAiReady: Boolean
    
    // 特征识别
    suspend fun recognizeFeature(text: String): FeatureRecognitionResult
    suspend fun confirmFeature(feature: FeatureInfo): Boolean
    suspend fun ignoreFeature(featureId: String)
    
    // 特征匹配
    suspend fun matchFeatures(input: String, appPackage: String): FeatureMatchResult
    suspend fun generateEnhancedContent(
        input: String,
        features: Array<FeatureInfo>
    ): String
    
    // 特征管理
    suspend fun getAllFeatures(): Array<FeatureInfo>
    suspend fun getFeaturesByCategory(category: Int): Array<FeatureInfo>
    suspend fun searchFeatures(keyword: String): Array<FeatureInfo>
    suspend fun deleteFeature(featureId: String): Boolean
    suspend fun setFeatureEnabled(featureId: String, enabled: Boolean): Boolean
    
    // 隐私模式
    var privacyMode: Boolean
    
    // 场景检测
    fun detectScene(appPackage: String): SceneInfo
    fun isAiChatApp(appPackage: String): Boolean
    fun shouldEnhance(appPackage: String): Boolean
    
    // AI 应用管理
    fun registerAiApp(packageName: String, displayName: String)
    fun unregisterAiApp(packageName: String)
    fun getRegisteredAiApps(): Array<String>
}

/**
 * Rime AI JNI 方法
 * 与 C++ 层 llm_jni.cc 对应
 */
object RimeAi : RimeAiApi {
    
    // 加载 native 库（由 Rime 主模块加载）
    init {
        try {
            System.loadLibrary("rime")
        } catch (e: UnsatisfiedLinkError) {
            // 开发阶段可能还未编译
        }
    }
    
    // ============================================================
    // JNI Native 方法声明
    // ============================================================
    
    // 初始化与关闭
    @JvmStatic
    external fun aiInit(configJson: String): Boolean
    
    @JvmStatic
    external fun aiShutdown()
    
    // 状态查询
    @JvmStatic
    external fun aiGetState(): Int
    
    @JvmStatic
    external fun aiIsReady(): Boolean
    
    // LLM 推理
    @JvmStatic
    external fun aiInfer(prompt: String): String
    
    @JvmStatic
    external fun aiInferAsync(prompt: String, callback: AiInferenceCallback): Long
    
    @JvmStatic
    external fun aiCancel()
    
    // 模型管理
    @JvmStatic
    external fun aiLoadModel(modelPath: String): Boolean
    
    @JvmStatic
    external fun aiUnloadModel()
    
    @JvmStatic
    external fun aiIsModelLoaded(): Boolean
    
    // ============================================================
    // 特征识别 JNI 方法
    // ============================================================
    
    @JvmStatic
    external fun aiRecognizeFeature(text: String): String
    
    @JvmStatic
    external fun aiConfirmFeature(featureJson: String): Boolean
    
    @JvmStatic
    external fun aiIgnoreFeature(featureId: String)
    
    @JvmStatic
    external fun aiMatchFeatures(input: String, appPackage: String): String
    
    @JvmStatic
    external fun aiGenerateContent(input: String, featuresJson: String): String
    
    // ============================================================
    // 特征管理 JNI 方法
    // ============================================================
    
    @JvmStatic
    external fun aiGetAllFeatures(): String
    
    @JvmStatic
    external fun aiGetFeaturesByCategory(category: Int): String
    
    @JvmStatic
    external fun aiSearchFeatures(keyword: String): String
    
    @JvmStatic
    external fun aiDeleteFeature(featureId: String): Boolean
    
    @JvmStatic
    external fun aiSetFeatureEnabled(featureId: String, enabled: Boolean): Boolean
    
    // ============================================================
    // 隐私模式 JNI 方法
    // ============================================================
    
    @JvmStatic
    external fun aiSetPrivacyMode(enabled: Boolean)
    
    @JvmStatic
    external fun aiIsPrivacyMode(): Boolean
    
    // ============================================================
    // 场景检测 JNI 方法
    // ============================================================
    
    @JvmStatic
    external fun aiDetectScene(appPackage: String): String
    
    @JvmStatic
    external fun aiIsAiChatApp(appPackage: String): Boolean
    
    @JvmStatic
    external fun aiShouldEnhance(appPackage: String): Boolean
    
    @JvmStatic
    external fun aiRegisterAiApp(packageName: String, displayName: String)
    
    @JvmStatic
    external fun aiUnregisterAiApp(packageName: String)
    
    @JvmStatic
    external fun aiGetRegisteredAiApps(): Array<String>
    
    // ============================================================
    // RimeAiApi 接口实现
    // ============================================================
    
    override val isAiReady: Boolean
        get() = aiIsReady()
    
    // TODO: 实现 StateFlow
    override val aiState: StateFlow<AIState>
        get() = throw NotImplementedError("StateFlow requires coroutine setup")
    
    override suspend fun recognizeFeature(text: String): FeatureRecognitionResult {
        val json = aiRecognizeFeature(text)
        return parseRecognitionResult(json)
    }
    
    override suspend fun confirmFeature(feature: FeatureInfo): Boolean {
        val json = featureToJson(feature)
        return aiConfirmFeature(json)
    }
    
    override suspend fun ignoreFeature(featureId: String) {
        aiIgnoreFeature(featureId)
    }
    
    override suspend fun matchFeatures(input: String, appPackage: String): FeatureMatchResult {
        val json = aiMatchFeatures(input, appPackage)
        return parseMatchResult(json)
    }
    
    override suspend fun generateEnhancedContent(
        input: String,
        features: Array<FeatureInfo>
    ): String {
        val featuresJson = featuresToJson(features)
        return aiGenerateContent(input, featuresJson)
    }
    
    override suspend fun getAllFeatures(): Array<FeatureInfo> {
        val json = aiGetAllFeatures()
        return parseFeatures(json)
    }
    
    override suspend fun getFeaturesByCategory(category: Int): Array<FeatureInfo> {
        val json = aiGetFeaturesByCategory(category)
        return parseFeatures(json)
    }
    
    override suspend fun searchFeatures(keyword: String): Array<FeatureInfo> {
        val json = aiSearchFeatures(keyword)
        return parseFeatures(json)
    }
    
    override suspend fun deleteFeature(featureId: String): Boolean {
        return aiDeleteFeature(featureId)
    }
    
    override suspend fun setFeatureEnabled(featureId: String, enabled: Boolean): Boolean {
        return aiSetFeatureEnabled(featureId, enabled)
    }
    
    override var privacyMode: Boolean
        get() = aiIsPrivacyMode()
        set(value) { aiSetPrivacyMode(value) }
    
    override fun detectScene(appPackage: String): SceneInfo {
        val json = aiDetectScene(appPackage)
        return parseSceneInfo(json)
    }
    
    override fun isAiChatApp(appPackage: String): Boolean {
        return aiIsAiChatApp(appPackage)
    }
    
    override fun shouldEnhance(appPackage: String): Boolean {
        return aiShouldEnhance(appPackage)
    }
    
    override fun registerAiApp(packageName: String, displayName: String) {
        aiRegisterAiApp(packageName, displayName)
    }
    
    override fun unregisterAiApp(packageName: String) {
        aiUnregisterAiApp(packageName)
    }
    
    override fun getRegisteredAiApps(): Array<String> {
        return aiGetRegisteredAiApps()
    }
    
    // ============================================================
    // JSON 解析辅助方法
    // ============================================================
    
    private fun parseRecognitionResult(json: String): FeatureRecognitionResult {
        // 简单解析（实际应使用 JSON 库）
        val hasFeature = json.contains("\"hasFeature\":true")
        return FeatureRecognitionResult(
            hasFeature = hasFeature,
            shouldPrompt = json.contains("\"shouldPrompt\":true")
        )
    }
    
    private fun parseMatchResult(json: String): FeatureMatchResult {
        return FeatureMatchResult(
            intent = "",
            matchedFeatures = emptyArray(),
            confidence = 0.8
        )
    }
    
    private fun parseFeatures(json: String): Array<FeatureInfo> {
        return emptyArray()
    }
    
    private fun parseSceneInfo(json: String): SceneInfo {
        return SceneInfo(
            type = 0,
            appPackage = "",
            appName = "",
            shouldEnhance = false
        )
    }
    
    private fun featureToJson(feature: FeatureInfo): String {
        return "{\"id\":\"${feature.id}\",\"category\":${feature.category}," +
               "\"key\":\"${feature.key}\",\"value\":\"${feature.value}\"," +
               "\"confidence\":${feature.confidence},\"enabled\":${feature.enabled}}"
    }
    
    private fun featuresToJson(features: Array<FeatureInfo>): String {
        return features.joinToString(",", "[", "]") { featureToJson(it) }
    }
}

/**
 * AI 推理回调接口
 */
interface AiInferenceCallback {
    fun onResult(content: String, elapsedMs: Long, success: Boolean, error: String?)
}