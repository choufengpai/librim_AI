/**
 * Copyright Librim AI Developers
 * Distributed under the BSD License
 *
 * 2026-03-08 Librim AI Team
 *
 * 隐私模式指示器
 * 显示无痕模式状态的 UI 组件
 */

package com.osfans.trime.ui.components

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import android.view.animation.AccelerateDecelerateInterpolator
import androidx.core.content.ContextCompat
import com.osfans.trime.R

/**
 * 隐私模式指示器视图
 * 在键盘上显示一个视觉提示，表示无痕模式已启用
 */
class PrivacyModeIndicator @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {
    
    private val paint: Paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path: Path = Path()
    private val rect: RectF = RectF()
    
    private var isPrivacyModeEnabled = false
    private var animationProgress = 0f
    
    private val indicatorColor: Int
    private val backgroundColor: Int
    private val textColor: Int
    
    private var pulseAnimator: ValueAnimator? = null
    
    init {
        indicatorColor = ContextCompat.getColor(context, 
            R.color.privacy_indicator_color)
        backgroundColor = ContextCompat.getColor(context, 
            R.color.privacy_background_color)
        textColor = ContextCompat.getColor(context, 
            R.color.privacy_text_color)
        
        paint.textAlign = Paint.Align.CENTER
        paint.textSize = 28f
        
        visibility = GONE
    }
    
    /**
     * 设置隐私模式状态
     */
    fun setPrivacyMode(enabled: Boolean, animated: Boolean = true) {
        if (isPrivacyModeEnabled == enabled) return
        
        isPrivacyModeEnabled = enabled
        
        if (enabled) {
            visibility = VISIBLE
            if (animated) {
                startPulseAnimation()
            }
        } else {
            stopPulseAnimation()
            if (animated) {
                fadeOutAndHide()
            } else {
                visibility = GONE
            }
        }
        
        invalidate()
    }
    
    /**
     * 检查隐私模式是否启用
     */
    fun isPrivacyModeEnabled(): Boolean = isPrivacyModeEnabled
    
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        if (!isPrivacyModeEnabled) return
        
        val width = width.toFloat()
        val height = height.toFloat()
        
        // 绘制背景
        paint.color = backgroundColor
        paint.alpha = ((200 * (1 - animationProgress * 0.3)).toInt())
        
        rect.set(0f, 0f, width, height)
        val radius = height / 2
        canvas.drawRoundRect(rect, radius, radius, paint)
        
        // 绘制图标区域背景
        paint.color = indicatorColor
        paint.alpha = ((255 * (1 - animationProgress * 0.2)).toInt())
        
        val iconSize = height * 0.7f
        val iconLeft = (height - iconSize) / 2
        val iconTop = (height - iconSize) / 2
        
        // 绘制盾牌图标
        drawShieldIcon(canvas, iconLeft + iconSize / 2, iconTop + iconSize / 2, iconSize / 2)
        
        // 绘制文字
        paint.color = textColor
        paint.textSize = height * 0.35f
        paint.alpha = 255
        
        val text = "无痕模式"
        val textX = height + (width - height) / 2
        val textY = height / 2 - (paint.descent() + paint.ascent()) / 2
        
        canvas.drawText(text, textX, textY, paint)
    }
    
    /**
     * 绘制盾牌图标
     */
    private fun drawShieldIcon(canvas: Canvas, cx: Float, cy: Float, size: Float) {
        path.reset()
        
        // 盾牌形状
        path.moveTo(cx, cy - size)
        path.lineTo(cx + size * 0.8f, cy - size * 0.6f)
        path.lineTo(cx + size * 0.8f, cy + size * 0.2f)
        path.quadTo(cx + size * 0.8f, cy + size * 0.8f, cx, cy + size)
        path.quadTo(cx - size * 0.8f, cy + size * 0.8f, cx - size * 0.8f, cy + size * 0.2f)
        path.lineTo(cx - size * 0.8f, cy - size * 0.6f)
        path.close()
        
        canvas.drawPath(path, paint)
    }
    
    /**
     * 开始脉冲动画
     */
    private fun startPulseAnimation() {
        stopPulseAnimation()
        
        pulseAnimator = ValueAnimator.ofFloat(0f, 1f).apply {
            duration = 1500
            repeatMode = ValueAnimator.REVERSE
            repeatCount = ValueAnimator.INFINITE
            interpolator = AccelerateDecelerateInterpolator()
            
            addUpdateListener { animator ->
                animationProgress = animator.animatedValue as Float
                invalidate()
            }
        }
        
        pulseAnimator?.start()
    }
    
    /**
     * 停止脉冲动画
     */
    private fun stopPulseAnimation() {
        pulseAnimator?.cancel()
        pulseAnimator = null
        animationProgress = 0f
    }
    
    /**
     * 淡出并隐藏
     */
    private fun fadeOutAndHide() {
        animate()
            .alpha(0f)
            .setDuration(300)
            .withEndAction {
                visibility = GONE
                alpha = 1f
            }
            .start()
    }
    
    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        stopPulseAnimation()
    }
}