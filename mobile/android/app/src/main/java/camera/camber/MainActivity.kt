package camera.camber

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.lifecycleScope
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    private var status by mutableStateOf("Connect your phone to Camber on Windows.")
    private var sharing by mutableStateOf(false)
    private var connected by mutableStateOf(false)
    private var paused by mutableStateOf(false)
    private var working by mutableStateOf(false)
    private var practicing by mutableStateOf(false)
    private var front by mutableStateOf(false)
    private var switching by mutableStateOf(false)
    private var cameras by mutableStateOf(emptySet<Boolean>())
    private var gestureFeedback by mutableStateOf("Hold a pose briefly to react.")
    private var requestedHeight by mutableStateOf(1080)
    private var enhancement by mutableStateOf("original")
    private var videoStatus by mutableStateOf("No additional image filters.")
    private var gestures by mutableStateOf(true)
    private var pcName by mutableStateOf<String?>(null)
    private lateinit var link: CameraLink
    private val permission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) {working=false;status="Camera permission granted. Tap Start or Try gestures to continue."} else { working=false;status = "Camera permission is required. Enable it in Android application settings." }
    }
    private val scan = registerForActivityResult(ScanContract()) { result ->
        result.contents?.let { payload -> lifecycleScope.launch {
            status = "Pairing securely…"
            working = true
            try { withContext(Dispatchers.IO) { link.pair(payload) } } catch (e: Exception) { working=false;status = e.message ?: "Pairing failed" }
        } }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        link = CameraLink(this, gestureUpdate={ text -> runOnUiThread { gestureFeedback=text } },videoUpdate={mode,info,height->runOnUiThread {enhancement=mode;videoStatus=info;requestedHeight=height}}) { text, active -> runOnUiThread {
            practicing=link.isPracticing;front=link.front; switching=link.switchingCamera; cameras=link.availableCameras
            status = text; sharing = active; connected=link.isConnected; pcName=link.pairedPcName; working=false
            if (active || practicing) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } }
        pcName=link.pairedPcName; gestures=link.localGesturesEnabled; front=link.front; cameras=link.availableCameras
        setContent { CamberTheme { CamberScreen(status,sharing,connected,paused,working,pcName,gestures,front,switching,cameras,gestureFeedback,practicing,enhancement,videoStatus,requestedHeight,onQuality={link.selectQuality(it)},onEnhancement={link.selectEnhancement(it)},onPractice={
                if(practicing) link.endPractice()
                else if(checkSelfPermission(Manifest.permission.CAMERA)!=PackageManager.PERMISSION_GRANTED) permission.launch(Manifest.permission.CAMERA)
                else {working=true;status="Opening local practice…";lifecycleScope.launch {try {withContext(Dispatchers.IO){link.startPractice()}} catch(e:Exception){link.endPractice();status=e.message ?: "Practice unavailable"} finally {working=false}}}
            },onCamera={link.selectCamera(it)},
            onGestures={gestures=it;link.setGesturePreference(it)},
            onStart={paused=false;working=true;status="Waiting for your PC…";if(checkSelfPermission(Manifest.permission.CAMERA)==PackageManager.PERMISSION_GRANTED) link.start() else permission.launch(Manifest.permission.CAMERA)},
            onPause={paused=true;link.pause()},onStop={paused=false;link.stop()},
            onScan={scan.launch(ScanOptions().setDesiredBarcodeFormats(ScanOptions.QR_CODE).setPrompt("Scan the code in Camber on Windows"))},
            onReconnect={working=true;lifecycleScope.launch {try {withContext(Dispatchers.IO){link.connect()}}catch(e:Exception){working=false;status=e.message ?: "Connection failed"}}},
            onForget={link.forget();pcName=null;connected=false;paused=false}) } }
    }
    override fun onResume() { super.onResume(); if (::link.isInitialized) link.setForeground(hasWindowFocus()) }
    override fun onWindowFocusChanged(focused: Boolean) { super.onWindowFocusChanged(focused); if (::link.isInitialized) { val wasActive=sharing || working; link.setForeground(focused); if(!focused){paused=paused || wasActive;working=false} } }
    override fun onPause() { if (::link.isInitialized) {val wasActive=sharing || working;link.setForeground(false);paused=paused || wasActive;working=false};super.onPause() }
    override fun onStop() { if (::link.isInitialized) link.setForeground(false); super.onStop() }
    override fun onDestroy() { link.dispose(); super.onDestroy() }
}
