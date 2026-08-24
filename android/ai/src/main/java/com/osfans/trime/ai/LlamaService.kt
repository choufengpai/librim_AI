/**
 * Copyright Librim AI Developers
 * Distributed under the BSD License
 *
 * 2026-08-20 Librim AI Team
 *
 * llama.cpp 服务封装
 * 加载 GGUF 模型(Qwen3.5 系列)，提供统一的 LLM 推理接口
 * JNI 实现位于 src/rime/ai/llama_jni.cc(随 librime 编译)
 */

package com.osfans.trime.ai

import android.content.Context
import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * llama.cpp 推理配置
 * modelPath 指向 GGUF 文件(如 Qwen3.5-2B-Instruct-Q4_K_M.gguf)
 */
data class LlamaConfig(
    val modelPath: String = "",
    val contextLength: Int = 2048,          // 上下文窗口(prompt~300 + 输出上限512,2048 足够;更小 KV 更快)
    val threads: Int = 4,                   // CPU decode 线程数(prefill 线程数由 native 层自动取全部核心)
    val gpuLayers: Int = 0,                 // GPU 卸载层数(0 = 纯 CPU)
    val maxTokens: Int = 512,               // 单次生成最大 token 数
    val temperature: Float = 0.3f,
    val topP: Float = 0.9f,
    val systemPrompt: String = "你是一个简洁的中文智能输入助手。",
    val noThink: Boolean = true,            // Qwen3.5: 附加 /no_think 关闭思考模式(低延迟)
    val timeoutMs: Long = 10000,
    val preloadModel: Boolean = true
)

/**
 * 推理结果
 */
data class InferenceResult(
    val content: String,
    val elapsedMs: Long,
    val success: Boolean,
    val error: String? = null
)

/**
 * 推理状态
 */
enum class InferenceStatus {
    IDLE,           // 空闲
    LOADING,        // 加载模型中
    READY,          // 就绪
    INFERRING,      // 推理中
    ERROR           // 错误状态
}

/**
 * 流式生成回调(由 native 层每个 token 调用一次)
 */
interface TokenCallback {
    fun onToken(piece: String)
}

/**
 * llama.cpp 推理服务
 *
 * 封装 llama.cpp JNI 接口(参考 llama.cpp 官方 llama-android 示例的桥接模式):
 * - nativeLoadModel: 加载 GGUF 模型
 * - nativeGenerate: ChatML 模板 + 采样生成,支持逐 token 回调
 *
 * 同时保持与 C++ 侧 llm_jni.cc 桥约定的方法签名:
 * - inference(String, Long)   C++ LLMJNIBridge 经 JNI 调用,结果经 LLMNative.onInferenceResult 回调
 * - isModelReady() / loadModel(String) / unloadModel()
 */
class LlamaService(private val context: Context) {

    companion object {
        private const val TAG = "LlamaService"

        @Volatile
        private var instance: LlamaService? = null

        fun getInstance(context: Context): LlamaService {
            return instance ?: synchronized(this) {
                instance ?: LlamaService(context.applicationContext).also { instance = it }
            }
        }
    }

    // 状态流
    private val _status = MutableStateFlow(InferenceStatus.IDLE)
    val status: StateFlow<InferenceStatus> = _status.asStateFlow()

    // 配置
    @Volatile
    private var config: LlamaConfig = LlamaConfig()

    // 推理互斥(同一时刻仅一次推理,与 native 层互斥锁配合)
    private val inferring = AtomicBoolean(false)

    // 请求 ID 生成器(供直接调用方使用)
    private val requestIdGenerator = AtomicLong(0)

    // 推理作用域
    private val inferenceScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    // ============================================================
    // llama.cpp JNI 方法(实现于 src/rime/ai/llama_jni.cc)
    // ============================================================

    private external fun nativeLoadModel(
        path: String, contextLength: Int, threads: Int, gpuLayers: Int
    ): Boolean

    private external fun nativeGenerate(
        prompt: String, systemPrompt: String,
        maxTokens: Int, temperature: Float, topP: Float, noThink: Boolean,
        callback: TokenCallback?
    ): String

    private external fun nativeCancel()

    private external fun nativeFree()

    private external fun nativeIsLoaded(): Boolean

    /** 返回 JSON: {model_desc, model_size, n_ctx, n_vocab} */
    private external fun nativeGetModelInfo(): String

    // ============================================================
    // C++ llm_jni.cc 桥依赖的方法(签名不可变更,且必须非 suspend)
    // ============================================================

    /**
     * 异步推理入口(由 C++ LLMJNIBridge 经 JNI 调用)
     * @param prompt 输入提示词
     * @param requestId 请求 ID(由 C++ 层传入)
     */
    fun inference(prompt: String, requestId: Long) {
        inferenceScope.launch {
            val result = infer(prompt)
            // 通过 JNI 回调 C++ 层
            try {
                LLMNative.onInferenceResult(
                    requestId,
                    result.content,
                    result.elapsedMs,
                    result.success,
                    result.error ?: ""
                )
            } catch (e: Exception) {
                Log.e(TAG, "Failed to callback to native", e)
            }
        }
    }

    fun isModelReady(): Boolean {
        return _status.value == InferenceStatus.READY
    }

    /**
     * 加载模型(阻塞直至完成;由 C++ 桥同步调用)
     */
    fun loadModel(modelPath: String): Boolean {
        if (modelPath.isEmpty()) {
            Log.e(TAG, "Model path is empty")
            return false
        }
        _status.value = InferenceStatus.LOADING
        return try {
            val ok = nativeLoadModel(
                modelPath, config.contextLength, config.threads, config.gpuLayers
            )
            if (ok) {
                config = config.copy(modelPath = modelPath)
                _status.value = InferenceStatus.READY
                Log.i(TAG, "Model loaded: $modelPath")
                nativeGetModelInfo().let { Log.i(TAG, "Model info: $it") }
            } else {
                _status.value = InferenceStatus.ERROR
                Log.e(TAG, "Failed to load model: $modelPath")
            }
            ok
        } catch (e: Exception) {
            _status.value = InferenceStatus.ERROR
            Log.e(TAG, "Model load error", e)
            false
        }
    }

    fun unloadModel() {
        try {
            nativeFree()
        } catch (e: Exception) {
            Log.e(TAG, "Unload error", e)
        }
        _status.value = InferenceStatus.IDLE
        Log.i(TAG, "Model unloaded")
    }

    // ============================================================
    // Kotlin 侧 API
    // ============================================================

    /**
     * 初始化服务并注册到 C++ 桥
     */
    suspend fun initialize(config: LlamaConfig): Boolean = withContext(Dispatchers.IO) {
        if (_status.value == InferenceStatus.READY && config.modelPath == this@LlamaService.config.modelPath) {
            Log.i(TAG, "LlamaService already initialized")
            return@withContext true
        }

        _status.value = InferenceStatus.LOADING
        this@LlamaService.config = config

        // 注册到 C++ LLMJNIBridge(缓存 JavaVM 与服务实例,打通 C++ -> Java 推理链路)
        try {
            LLMNative.setInferenceService(this@LlamaService)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to register to native bridge", e)
        }

        if (config.modelPath.isEmpty() || !config.preloadModel) {
            // 延迟加载:首次推理时再加载
            _status.value = InferenceStatus.IDLE
            return@withContext true
        }

        val ok = loadModel(config.modelPath)
        if (!ok) {
            _status.value = InferenceStatus.ERROR
        }
        ok
    }

    /**
     * 同步推理(挂起)
     */
    suspend fun infer(prompt: String): InferenceResult = withContext(Dispatchers.Default) {
        val startTime = System.currentTimeMillis()

        if (!isModelReady() && _status.value != InferenceStatus.LOADING) {
            // 延迟加载场景:首次推理时加载模型
            val path = config.modelPath
            if (path.isEmpty() || !loadModel(path)) {
                return@withContext InferenceResult(
                    content = "", elapsedMs = 0, success = false,
                    error = "Model not ready: $path"
                )
            }
        }

        if (!inferring.compareAndSet(false, true)) {
            return@withContext InferenceResult(
                content = "", elapsedMs = 0, success = false,
                error = "Another inference in progress"
            )
        }

        _status.value = InferenceStatus.INFERRING
        try {
            val content = nativeGenerate(
                prompt, config.systemPrompt,
                config.maxTokens, config.temperature, config.topP, config.noThink,
                null
            )
            _status.value = InferenceStatus.READY
            InferenceResult(
                content = content,
                elapsedMs = System.currentTimeMillis() - startTime,
                success = true
            )
        } catch (e: Exception) {
            _status.value = InferenceStatus.READY
            InferenceResult(
                content = "",
                elapsedMs = System.currentTimeMillis() - startTime,
                success = false,
                error = e.message ?: "Inference failed"
            )
        } finally {
            inferring.set(false)
        }
    }

    /**
     * 流式推理(逐 token 输出)
     */
    fun inferStream(prompt: String): Flow<String> = channelFlow {
        if (!isModelReady()) {
            throw IllegalStateException("Model not ready")
        }
        if (!inferring.compareAndSet(false, true)) {
            throw IllegalStateException("Another inference in progress")
        }
        _status.value = InferenceStatus.INFERRING
        try {
            val callback = object : TokenCallback {
                override fun onToken(piece: String) {
                    trySend(piece)
                }
            }
            // nativeGenerate 在当前 channelFlow 的协程上下文执行,逐 token 回调
            nativeGenerate(
                prompt, config.systemPrompt,
                config.maxTokens, config.temperature, config.topP, config.noThink,
                callback
            )
            _status.value = InferenceStatus.READY
        } catch (e: Exception) {
            _status.value = InferenceStatus.READY
            throw e
        } finally {
            inferring.set(false)
        }
    }.flowOn(Dispatchers.Default)

    /**
     * 取消当前推理
     */
    fun cancel() {
        try {
            nativeCancel()
        } catch (e: Exception) {
            Log.e(TAG, "Cancel error", e)
        }
        _status.value = InferenceStatus.READY
        Log.i(TAG, "Inference cancelled")
    }

    /**
     * 关闭服务
     */
    fun shutdown() {
        unloadModel()
        inferenceScope.cancel()
        _status.value = InferenceStatus.IDLE
        Log.i(TAG, "LlamaService shutdown")
    }

    fun updateConfig(newConfig: LlamaConfig) {
        config = newConfig
    }

    fun getConfig(): LlamaConfig = config

    fun nextRequestId(): Long = requestIdGenerator.incrementAndGet()
}
