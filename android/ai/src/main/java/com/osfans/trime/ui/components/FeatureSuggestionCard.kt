/**
 * Copyright Librim AI Developers
 * Distributed under the BSD License
 *
 * 2026-03-08 Librim AI Team
 *
 * 特征建议卡片 UI 组件
 * 显示识别到的特征，让用户确认保存
 */

package com.osfans.trime.ui.components

import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.animation.DecelerateInterpolator
import android.widget.FrameLayout
import android.widget.TextView
import com.google.android.material.card.MaterialCardView
import com.google.android.material.button.MaterialButton
import com.osfans.trime.core.FeatureInfo
import com.osfans.trime.core.FeatureRecognitionResult

/**
 * 特征建议卡片
 */
class FeatureSuggestionCard @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : MaterialCardView(context, attrs, defStyleAttr) {
    
    private val container: View
    private val categoryIcon: TextView
    private val titleText: TextView
    private val featureKeyText: TextView
    private val featureValueText: TextView
    private val confidenceText: TextView
    private val confirmButton: MaterialButton
    private val ignoreButton: MaterialButton
    
    private var onConfirmListener: ((FeatureInfo) -> Unit)? = null
    private var onIgnoreListener: (() -> Unit)? = null
    
    private var currentFeature: FeatureInfo? = null
    
    init {
        // 加载布局
        container = LayoutInflater.from(context)
            .inflate(R.layout.feature_suggestion_card, this, true)
        
        categoryIcon = container.findViewById(R.id.category_icon)
        titleText = container.findViewById(R.id.title_text)
        featureKeyText = container.findViewById(R.id.feature_key)
        featureValueText = container.findViewById(R.id.feature_value)
        confidenceText = container.findViewById(R.id.confidence_text)
        confirmButton = container.findViewById(R.id.btn_confirm)
        ignoreButton = container.findViewById(R.id.btn_ignore)
        
        // 设置按钮监听
        confirmButton.setOnClickListener {
            currentFeature?.let { feature ->
                onConfirmListener?.invoke(feature)
            }
            hideWithAnimation()
        }
        
        ignoreButton.setOnClickListener {
            onIgnoreListener?.invoke()
            hideWithAnimation()
        }
        
        // 初始隐藏
        visibility = View.GONE
        alpha = 0f
        translationY = 100f
    }
    
    /**
     * 显示特征建议
     */
    fun showFeature(result: FeatureRecognitionResult) {
        if (!result.hasFeature) return
        
        currentFeature = FeatureInfo(
            id = result.featureId,
            category = result.category,
            key = result.key,
            value = result.value,
            confidence = result.confidence,
            enabled = true
        )
        
        // 设置图标（根据类别）
        categoryIcon.text = getCategoryIcon(result.category)
        
        // 设置文本
        titleText.text = "检测到个人特征"
        featureKeyText.text = result.key
        featureValueText.text = result.value
        confidenceText.text = "置信度: ${(result.confidence * 100).toInt()}%"
        
        showWithAnimation()
    }
    
    /**
     * 显示增强建议卡片
     */
    fun showEnhancementSuggestion(
        matchedFeatures: List<FeatureInfo>,
        onApply: (List<FeatureInfo>) -> Unit,
        onDismiss: () -> Unit
    ) {
        if (matchedFeatures.isEmpty()) return
        
        currentFeature = matchedFeatures.first()
        
        categoryIcon.text = "✨"
        titleText.text = "智能增强建议"
        featureKeyText.text = "已匹配 ${matchedFeatures.size} 个特征"
        
        val valuesPreview = matchedFeatures.take(3)
            .joinToString(", ") { it.value }
        featureValueText.text = valuesPreview
        
        confidenceText.visibility = View.GONE
        
        confirmButton.text = "应用增强"
        confirmButton.setOnClickListener {
            onApply(matchedFeatures)
            hideWithAnimation()
        }
        
        ignoreButton.text = "忽略"
        ignoreButton.setOnClickListener {
            onDismiss()
            hideWithAnimation()
        }
        
        showWithAnimation()
    }
    
    /**
     * 设置确认监听器
     */
    fun setOnConfirmListener(listener: (FeatureInfo) -> Unit) {
        onConfirmListener = listener
    }
    
    /**
     * 设置忽略监听器
     */
    fun setOnIgnoreListener(listener: () -> Unit) {
        onIgnoreListener = listener
    }
    
    /**
     * 显示动画
     */
    private fun showWithAnimation() {
        visibility = View.VISIBLE
        
        val animatorSet = AnimatorSet()
        val alphaAnim = ObjectAnimator.ofFloat(this, "alpha", 0f, 1f)
        val translationAnim = ObjectAnimator.ofFloat(this, "translationY", 100f, 0f)
        
        animatorSet.playTogether(alphaAnim, translationAnim)
        animatorSet.duration = 300
        animatorSet.interpolator = DecelerateInterpolator()
        animatorSet.start()
    }
    
    /**
     * 隐藏动画
     */
    private fun hideWithAnimation() {
        val animatorSet = AnimatorSet()
        val alphaAnim = ObjectAnimator.ofFloat(this, "alpha", 1f, 0f)
        val translationAnim = ObjectAnimator.ofFloat(this, "translationY", 0f, -50f)
        
        animatorSet.playTogether(alphaAnim, translationAnim)
        animatorSet.duration = 200
        animatorSet.interpolator = DecelerateInterpolator()
        
        animatorSet.start()
        
        postDelayed({
            visibility = View.GONE
        }, 200)
    }
    
    /**
     * 获取类别图标
     */
    private fun getCategoryIcon(category: Int): String {
        return when (category) {
            0 -> "👤"  // 基本情况
            1 -> "❤️"  // 兴趣偏好
            2 -> "📅"  // 生活习惯
            3 -> "📌"  // 近期事项
            4 -> "👥"  // 社交关系
            else -> "📝"
        }
    }
}