package camera.camber
import android.content.Context
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.os.SystemClock
import android.view.OrientationEventListener
import org.webrtc.CapturerObserver
import org.webrtc.VideoFrame
import java.util.concurrent.atomic.AtomicLong
class FrameOrientation(context: Context) {
    private val policy=OrientationPolicy()
    private val cameras=context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    @Volatile private var sensor=0
    @Volatile private var front=false
    @Volatile private var knownCamera=false
    @Volatile var width=0; private set
    @Volatile var height=0; private set
    @Volatile var incomingRotation=0; private set
    @Volatile var appliedRotation=0; private set
    private val frames=AtomicLong(0)
    private var sampleAt=0L
    private var sampledFrames=0L
    private val listener=object:OrientationEventListener(context.applicationContext) {
        override fun onOrientationChanged(orientation:Int) {policy.observe(orientation,SystemClock.elapsedRealtime())}
    }
    val available get()=listener.canDetectOrientation()
    fun foreground(active:Boolean) {if(active && available)listener.enable() else listener.disable()}
    fun cameraOpened(name:String) {
        try {
            val c=cameras.getCameraCharacteristics(name)
            sensor=c.get(CameraCharacteristics.SENSOR_ORIENTATION)?:0
            front=c.get(CameraCharacteristics.LENS_FACING)==CameraCharacteristics.LENS_FACING_FRONT
            knownCamera=true
        } catch(_:Exception){knownCamera=false}
    }
    fun observer(target:CapturerObserver, valid:()->Boolean)=object:CapturerObserver {
        override fun onCapturerStarted(success:Boolean) {if(valid())target.onCapturerStarted(success)}
        override fun onCapturerStopped() {target.onCapturerStopped()}
        override fun onFrameCaptured(frame:VideoFrame) {
            if(!valid())return
            val rotation=if(knownCamera && available)policy.frameRotation(sensor,front,SystemClock.elapsedRealtime()) else frame.rotation
            width=frame.buffer.width;height=frame.buffer.height;incomingRotation=frame.rotation;appliedRotation=rotation;frames.incrementAndGet()
            frame.buffer.retain()
            val normalized=VideoFrame(frame.buffer,rotation,frame.timestampNs)
            try {target.onFrameCaptured(normalized)} finally {normalized.release()}
        }
    }
    @Synchronized fun captureFps(now:Long):Double? {
        val count=frames.get();val elapsed=now-sampleAt
        val fps=if(sampleAt!=0L && elapsed>0) (count-sampledFrames)*1000.0/elapsed else null
        sampleAt=now;sampledFrames=count;return fps
    }
    fun resetSample(){synchronized(this){sampleAt=0;sampledFrames=frames.get()}}
    fun close(){listener.disable()}
}
