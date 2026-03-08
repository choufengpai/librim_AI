/**
 * Copyright Librim AI Developers
 * Distributed under the BSD License
 *
 * 2026-03-08 Librim AI Team
 *
 * LLM Native JNI 接口
 * 声明 C++ 层导出的 JNI 方法
 */

package com.osfans.trime.ai

/**
 * LLM 配置类
 */
data class LLMConfig(
    val modelPath: String = "",
    val timeoutMs: Long = 10000,
    val preloadModel: Boolean = false,
    val maxTokens: Int = 512,
    val temperature: Float = 0.7f,
    val topP: Float = 0.9f
)

/**
 * 设备能力信息
 */
data class DeviceCapability(
    val hasGpu: Boolean,
    val ramMb: Int,
    val isLowRamDevice: Boolean
)

/**
 * 推理回调接口
 */
interface InferenceCallback {
    fun onResult(content: String, elapsedMs: Long, success: Boolean, error: String?)
}

/**
 * LLM Native 接口
 * 
 * 与 C++ 层 llm_jni.cc 对应的 JNI 方法声明
 */
object LLMNative {
    
    init {
        // 加载 native 库
        // 在实际集成时需要确保 librime 已编译包含 AI 模块
        try {
            System.loadLibrary("rime")
        } catch (e: UnsatisfiedLinkError) {
            // 开发阶段可能还未编译
        }
    }
    
    /**
     * 初始化 LLM 引擎
     * @param config LLM 配置
     * @return 是否成功
     */
    @JvmStatic
    external fun initEngine(config: LLMConfig): Boolean
    
    /**
     * 关闭 LLM 引擎
     */
    @JvmStatic
    external fun shutdownEngine()
    
    /**
     * 同步推理
     * @param prompt 输入提示词
     * @return JSON 格式的推理结果
     */
    @JvmStatic
    external fun infer(prompt: String): String
    
    /**
     * 异步推理
     * @param prompt 输入提示词
     * @param callback 结果回调
     * @return 请求 ID
     */
    @JvmStatic
    external fun inferAsync(prompt: String, callback: InferenceCallback): Long
    
    /**
     * 取消当前推理
     */
    @JvmStatic
    external fun cancel()
    
    /**
     * 加载模型
     * @param modelPath 模型路径
     * @return 是否成功
     */
    @JvmStatic
    external fun loadModel(modelPath: String): Boolean
    
    /**
     * 卸载模型
     */
    @JvmStatic
    external fun unloadModel()
    
    /**
     * 检查模型是否已加载
     * @return 是否已加载
     */
    @JvmStatic
    external fun isModelLoaded(): Boolean
    
    /**
     * 获取引擎状态
     * @return 状态码 (0=NotLoaded, 1=Normal, 2=Degraded, 3=Disabled)
     */
    @JvmStatic
    external fun getStatus(): Int
    
    /**
     * 检查引擎是否就绪
     * @return 是否就绪
     */
    @JvmStatic
    external fun isReady(): Boolean
    
    /**
     * 检测设备能力
     * @return 设备能力信息
     */
    @JvmStatic
    external fun detectCapability(): DeviceCapability
    
    // ============================================================
    // Prompt 管理
    // ============================================================
    
    /**
     * 获取特征识别 Prompt
     */
    @JvmStatic
    external fun getRecognitionPrompt(input: String): String
    
    /**
     * 获取特征匹配 Prompt
     */
    @JvmStatic
    external fun getMatchPrompt(input: String, features: String): String
    
    /**
     * 获取内容生成 Prompt
     */
    @JvmStatic
    external fun getGenerationPrompt(input: String, features: String): String
    
    // ============================================================
    // 回调方法（由 C++ 层调用）
    // ============================================================
    
    /**
     * 推理结果回调
     * 此方法由 C++ 层调用，用于通知 Java 层推理完成
     */
    @JvmStatic
    external fun onInferenceResult(
        requestId: Long,
        content: String,
        elapsedMs: Long,
        success: Boolean,
        error: String?
    )
}