package camera.camber

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.SystemClock
import com.google.mediapipe.framework.image.BitmapImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.gesturerecognizer.GestureRecognizer
import org.webrtc.VideoFrame
import org.webrtc.VideoSink
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import kotlin.math.roundToInt

/** One analysis frame in flight; inference never blocks capture or encoding. */
class GestureAnalyzer(context: Context, private val reaction: (String, Long) -> Unit, private val status: (String, Boolean) -> Unit) : VideoSink {
    private val appContext = context.applicationContext
    private val worker = Executors.newSingleThreadExecutor { r -> Thread(r, "CamberGestures") }
    private val busy = AtomicBoolean(false)
    private val epoch = AtomicLong(0)
    @Volatile private var enabled = false
    @Volatile private var closed = false
    @Volatile private var generation = 0L
    private var lastFrame = 0L
    private var lastFeedbackAt = 0L
    private var lastDiagnosticAt = 0L
    private val power=appContext.getSystemService(Context.POWER_SERVICE) as android.os.PowerManager
    @Volatile private var thermal=power.currentThermalStatus
    private val thermalListener=android.os.PowerManager.OnThermalStatusChangedListener {thermal=it}
    init {power.addThermalStatusListener(thermalListener)}
    private var pixels=IntArray(0)
    private var reusableBitmap: Bitmap? = null
    private var recognizer: GestureRecognizer? = null
    private val policy = GesturePolicy()
    fun setEnabled(value: Boolean, sessionGeneration: Long) {
        if (closed || (enabled == value && generation == sessionGeneration)) return
        enabled = value; generation = sessionGeneration; epoch.incrementAndGet()
        try { worker.execute { policy.reset() } } catch (_: java.util.concurrent.RejectedExecutionException) { }
    }
    override fun onFrame(frame: VideoFrame) {
        val now = SystemClock.elapsedRealtime()
        val interval=when {thermal>=android.os.PowerManager.THERMAL_STATUS_SEVERE->166;thermal>=android.os.PowerManager.THERMAL_STATUS_MODERATE->125;else->83}
        if (!enabled || closed || now - lastFrame < interval || !busy.compareAndSet(false, true)) return
        lastFrame = now
        val token = epoch.get(); val frameGeneration = generation
        // Release the camera's texture before running ML. Retaining it during
        // inference would hold SurfaceTextureHelper's single outstanding frame
        // and stall the webcam even with a separate inference thread.
        val rotation = frame.rotation
        val cpu: VideoFrame.I420Buffer
        try {
            val input = frame.buffer
            val scale = minOf(1f, 480f / maxOf(input.width, input.height))
            val w = maxOf(2, (input.width * scale).roundToInt() and -2)
            val h = maxOf(2, (input.height * scale).roundToInt() and -2)
            val small = input.cropAndScale(0, 0, input.width, input.height, w, h)
            try { cpu = small.toI420() ?: error("Cannot read camera pixels") } finally { small.release() }
        } catch (_: Exception) { busy.set(false); return }
        try { worker.execute {
            try {
                if (!enabled || epoch.get() != token) return@execute
                if (recognizer == null) {
                    val options = GestureRecognizer.GestureRecognizerOptions.builder()
                        .setBaseOptions(BaseOptions.builder().setModelAssetPath("gesture_recognizer.task").build())
                        .setRunningMode(RunningMode.VIDEO).setNumHands(2)
                        .setMinHandDetectionConfidence(.55f).setMinHandPresenceConfidence(.55f).setMinTrackingConfidence(.55f).build()
                    recognizer = GestureRecognizer.createFromOptions(appContext, options)
                    if (enabled && epoch.get() == token) status("Hand gestures are ready. Hold a pose for about a second.", true)
                }
                if (!enabled || epoch.get() != token) return@execute
                val bitmap = bitmap(cpu, rotation)
                val image = try { BitmapImageBuilder(bitmap).build() }
                    catch (failure: Exception) { bitmap.recycle(); throw failure }
                try {
                    val result = recognizer!!.recognizeForVideo(image, now)
                    val hands = result.landmarks().mapIndexed { index, points ->
                        val category = result.gestures().getOrNull(index)?.firstOrNull()
                        HandPose(category?.categoryName() ?: "None", category?.score() ?: 0f, points.map { HandPoint(it.x()*bitmap.width/minOf(bitmap.width,bitmap.height), it.y()*bitmap.height/minOf(bitmap.width,bitmap.height),it.z()*bitmap.width/minOf(bitmap.width,bitmap.height)) },
                            result.worldLandmarks().getOrNull(index)?.map {HandPoint(it.x(),it.y(),it.z())} ?: emptyList())
                    }
                    val found = policy.observe(hands, now)
                    if(now-lastDiagnosticAt>=1000) {
                        lastDiagnosticAt=now
                        android.util.Log.i("CamberGestureState","hands=${hands.size}; categories=${hands.joinToString { "${it.category}:${(it.confidence*100).toInt()}" }}; pose=${policy.feedback.pose}; phase=${policy.feedback.phase}; ${policy.geometryDiagnostic}")
                    }
                    if (enabled && epoch.get() == token) {
                        val f=policy.feedback
                        val text=when(f.phase) {
                            "holding" -> "Hold ${when(f.pose){"thumbsUp"->"your thumb up";"thumbsDown"->"your thumb down";"balloons"->"a peace sign";"confetti"->"two peace signs";"fireworks"->"two thumbs up";"rain"->"two thumbs down";"hearts"->"a heart";"lasers"->"two horns signs";else->"your pose"}}: ${(f.progress*100).toInt()}%"
                            "triggered" -> "Gesture recognized. Reaction requested."
                            "release" -> "Recognized: ${when(f.pose){"fireworks"->"two thumbs up";"rain"->"two thumbs down";"confetti"->"two peace signs";"hearts"->"heart";"lasers"->"two horns signs";"thumbsUp"->"thumbs up";"thumbsDown"->"thumbs down";else->"peace sign"}}. Lower your hands or change pose."
                            "cooldown" -> "Next reaction in ${(f.cooldownMs+999)/1000}s"
                            else -> "Gestures ready. Hold a pose briefly."
                        }
                        if(found!=null || now-lastFeedbackAt>=100) {lastFeedbackAt=now;status(text,true)}
                        if(found!=null) reaction(found,frameGeneration)
                    }
                } finally { image.close() }
            } catch (failure: LinkageError) {
                android.util.Log.w("CamberGestures","Gesture runtime unavailable",failure)
                if (enabled && epoch.get() == token) {
                    enabled = false; policy.reset()
                    status("Hand gestures are unavailable on this phone. Use the reaction buttons on Windows.", false)
                }
            } catch (failure: Exception) {
                android.util.Log.w("CamberGestures","Gesture processing unavailable",failure)
                if (enabled && epoch.get() == token) {
                    enabled = false; policy.reset()
                    status("Hand gestures are unavailable on this phone. Use the reaction buttons on Windows.", false)
                }
            } finally { cpu.release(); busy.set(false) }
        } } catch (_: java.util.concurrent.RejectedExecutionException) { cpu.release(); busy.set(false) }
    }
    private fun bitmap(i420: VideoFrame.I420Buffer, rotation: Int): Bitmap {
            val w = i420.width; val h = i420.height
            if(pixels.size!=w*h) pixels=IntArray(w*h)
            val y = i420.dataY; val u = i420.dataU; val v = i420.dataV
            for (row in 0 until h) for (col in 0 until w) {
                val luma = (y.get(row * i420.strideY + col).toInt() and 255) - 16
                val blue = (u.get(row / 2 * i420.strideU + col / 2).toInt() and 255) - 128
                val red = (v.get(row / 2 * i420.strideV + col / 2).toInt() and 255) - 128
                val r = ((298 * luma + 409 * red + 128) shr 8).coerceIn(0, 255)
                val g = ((298 * luma - 100 * blue - 208 * red + 128) shr 8).coerceIn(0, 255)
                val b = ((298 * luma + 516 * blue + 128) shr 8).coerceIn(0, 255)
                pixels[row * w + col] = (255 shl 24) or (r shl 16) or (g shl 8) or b
            }
            if(reusableBitmap?.isRecycled==true || reusableBitmap?.width!=w || reusableBitmap?.height!=h) {reusableBitmap?.recycle();reusableBitmap=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888)}
            val raw=reusableBitmap!!;raw.setPixels(pixels,0,w,0,0,w,h)
            // MPImage.close() recycles the bitmap supplied to its builder.
            // Keep the reusable conversion buffer private; transfer a distinct
            // image even at zero rotation (other angles already create one).
            if (rotation == 0) return raw.copy(Bitmap.Config.ARGB_8888,false)
            val rotated = Bitmap.createBitmap(raw, 0, 0, w, h, Matrix().apply { postRotate(rotation.toFloat()) }, true)
            return rotated
    }
    fun close() { if (closed) return; closed = true; power.removeThermalStatusListener(thermalListener); enabled = false; epoch.incrementAndGet(); worker.execute { recognizer?.close(); recognizer = null;reusableBitmap?.recycle();reusableBitmap=null }; worker.shutdown() }
}
