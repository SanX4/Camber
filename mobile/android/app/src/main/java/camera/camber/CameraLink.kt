package camera.camber

import android.content.Context
import android.os.Build
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import org.webrtc.*
import java.net.InetAddress
import java.net.URI
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import java.time.Instant
import javax.net.ssl.*

class CameraLink(private val context: Context, private val gestureUpdate: (String)->Unit = {}, private val videoUpdate: (String,String,Int)->Unit = {_,_,_->}, private val update: (String, Boolean) -> Unit) {
    private val store = EnrollmentStore(context)
    private var enrollment = store.load()
    val pairedPcName: String? get() = enrollment?.optString("name")
    @Volatile var isConnected = false
        private set
    private val preferences=context.getSharedPreferences("camber-appearance", Context.MODE_PRIVATE)
    @Volatile var localGesturesEnabled = preferences.getBoolean("gestures",true)
        private set
    fun setGesturePreference(enabled: Boolean) {localGesturesEnabled=enabled;preferences.edit().putBoolean("gestures",enabled).apply();gestures.setEnabled(isPracticing || (enabled&&gestureRequested&&sharing),generation)}
    @Volatile private var foreground = false
    private val heartbeat = android.os.Handler(android.os.Looper.getMainLooper())
    @Volatile private var lastPong = android.os.SystemClock.elapsedRealtime()
    private val beat = object : Runnable {
        override fun run() {
            if (isConnected) {
                if (android.os.SystemClock.elapsedRealtime() - lastPong > 6000) disconnect("PC connection lost. Sharing stopped.")
                else {send("ping");collectVideoStatistics()}
            }
            heartbeat.postDelayed(this, 1000)
        }
    }
    init { heartbeat.postDelayed(beat, 1000) }
    @Synchronized fun setForeground(value: Boolean) {
        if (foreground == value) return
        foreground = value
        orientation.foreground(value)
        if (!value) {
            val wasSharing = sharing || approvalRequested
            approvalRequested = false; track?.setEnabled(false)
            send("pause", JSONObject().put("reason", "outOfFocus"))
            releaseCamera()
            socket?.close(1000, "Out of focus"); socket = null; isConnected = false
            update(if(wasSharing) "Out of focus. Camera paused. Reconnect and tap Start to resume."
                else if(enrollment!=null) "Connection closed. Reconnect when ready."
                else "Connect your phone to Camber on Windows.", false)
        }
    }
    @Synchronized private fun disconnect(reason: String) {
        approvalRequested=false;releaseCamera();socket?.cancel();socket=null;isConnected=false;update(reason,false)
    }
    private var client: OkHttpClient? = null
    private var socket: WebSocket? = null
    @Volatile private var generation = 0L
    private var factory: PeerConnectionFactory? = null
    private var peer: PeerConnection? = null
    private var sender: RtpSender? = null
    private val videoStatistics=VideoStatistics()
    @Volatile private var enforcedBitrate:Int?=null
    @Volatile private var lastVideoReason="Waiting for video statistics."
    @Volatile private var videoSampleBusy=false
    fun selectQuality(height:Int) {
        if(height !in listOf(720,1080) || !sharing || !foreground || !isConnected || switchingCamera)return
        send("selectQuality",JSONObject().put("height",height))
    }
    fun selectEnhancement(mode:String) {
        if(mode !in listOf("original","natural","lowLight") || !isConnected)return
        send("setEnhancement",JSONObject().put("mode",mode))
    }
    private fun collectVideoStatistics() {
        val connection=peer ?: return
        if(!sharing || !foreground || videoSampleBusy)return
        val epoch=captureEpoch;val g=generation;videoSampleBusy=true
        connection.getStats { report ->
            try {synchronized(this) {
                if(epoch!=captureEpoch || g!=generation || !sharing || !foreground)return@synchronized
                val sample=videoStatistics.sample(report)
                orientation.captureFps(android.os.SystemClock.elapsedRealtime())?.let {sample.put("captureFps",it)}
                val power=context.getSystemService(Context.POWER_SERVICE) as android.os.PowerManager
                sample.put("thermalLimited",power.currentThermalStatus>=android.os.PowerManager.THERMAL_STATUS_MODERATE)
                sample.put("width",orientation.width).put("height",orientation.height)
                    .put("sourceRotation",orientation.incomingRotation).put("rotation",orientation.appliedRotation)
                    .put("maxBitrateBps",enforcedBitrate ?: JSONObject.NULL)
                send("videoStats",sample)
            }}finally{synchronized(this){if(epoch==captureEpoch)videoSampleBusy=false}}
        }
    }
    private fun applyVideoQuality(j:JSONObject) {
        val id=j.getString("id");val g=j.getLong("generation")
        val cap=j.getInt("maxBitrateBps");val capture=j.getJSONObject("capture")
        val w=capture.getInt("width");val h=capture.getInt("height")
        require(cap in 2_000_000..12_000_000 && capture.getInt("fps")==30 && ((w==1920 && h==1080)||(w==1280 && h==720)))
        val session=synchronized(this) {
            if(g!=generation || !sharing || !foreground || !remoteReady || configurationInFlight || switchingCamera)return
            Triple(captureEpoch,sender,capturer)
        }
        var ok=false
        try {
            val parameters=session.second?.parameters
            // An unnegotiated sender has no enforceable encoding. Leave it
            // unknown and let the host retry; never acknowledge a fake cap.
            if(parameters==null || parameters.encodings.isEmpty())return
            if(parameters!=null) {
                parameters.encodings.forEach {it.maxBitrateBps=cap;it.minBitrateBps=null;it.maxFramerate=30}
                ok=session.second!!.setParameters(parameters)
            }
            if(ok && (width!=w || height!=h))session.third?.changeCaptureFormat(w,h,30)
        }catch(_:Exception){ok=false}
        synchronized(this) {
            if(session.first!=captureEpoch || g!=generation || !sharing || !foreground)return
            if(ok){width=w;height=h;enforcedBitrate=cap}
            send("qualityResult",JSONObject().put("id",id).put("ok",ok).put("maxBitrateBps",enforcedBitrate ?: JSONObject.NULL))
        }
    }
    private var capturer: CameraVideoCapturer? = null
    private var source: VideoSource? = null
    private var track: VideoTrack? = null
    private var texture: SurfaceTextureHelper? = null
    private val orientation=FrameOrientation(context.applicationContext)
    private val egl = EglBase.create()
    private var width = 1920; private var height = 1080
    @Volatile var front = preferences.getBoolean("frontCamera", false)
        private set
    @Volatile var switchingCamera = false
        private set
    @Volatile private var captureEpoch = 0L
    @Volatile private var disposed = false
    private var configurationInFlight = false
    private var cameraOperation = 0L
    val availableCameras: Set<Boolean> get() = try {
        val cameras = Camera2Enumerator(context)
        cameras.deviceNames.mapNotNull { if(cameras.isFrontFacing(it)) true else if(cameras.isBackFacing(it)) false else null }.toSet()
    } catch (_: Exception) { emptySet() }

    @Synchronized fun selectCamera(useFront: Boolean) {
        if (switchingCamera || useFront == front) return
        if (useFront !in availableCameras) { update("This camera is unavailable on your phone.", sharing); return }
        if(isPracticing) {
            val current=capturer ?: return
            val epoch=captureEpoch
            switchingCamera=true;update("Switching practice camera…",false)
            val cameras=Camera2Enumerator(context)
            val name=cameras.deviceNames.first {if(useFront)cameras.isFrontFacing(it) else cameras.isBackFacing(it)}
            try {current.switchCamera(object:CameraVideoCapturer.CameraSwitchHandler {
                override fun onCameraSwitchDone(isFrontCamera:Boolean) {heartbeat.post { synchronized(this@CameraLink) {
                    if(epoch!=captureEpoch || !isPracticing)return@synchronized
                    switchingCamera=false;front=isFrontCamera;preferences.edit().putBoolean("frontCamera",front).apply()
                    update("Local practice. No video is sent to your PC.",false)
                } } }
                override fun onCameraSwitchError(error:String) {heartbeat.post {synchronized(this@CameraLink) {
                    if(epoch==captureEpoch && isPracticing){switchingCamera=false;update("Could not switch practice camera. Try again.",false)}
                } } }
            },name)}catch(_:Exception){switchingCamera=false;update("Could not switch practice camera.",false)}
            return
        }
        if (!sharing) {
            front=useFront; preferences.edit().putBoolean("frontCamera",front).apply()
            update(if(front) "Front camera selected. Start sharing when ready." else "Rear camera selected. Start sharing when ready.", false)
            return
        }
        if (!foreground || !isConnected) return
        switchingCamera=true
        val operation=++cameraOperation
        update("Switching camera…", true)
        send("selectCamera", JSONObject().put("camera",if(useFront) "front" else "back"))
        val epoch=captureEpoch
        heartbeat.postDelayed({ synchronized(this) {
            if(switchingCamera && epoch==captureEpoch && operation==cameraOperation) { stop(); update("Camera switch timed out. Start sharing to try again.", false) }
        } },5000)
    }
    @Volatile private var sharing = false
    @Volatile var isPracticing = false
        private set
    private val cleanupWorker=java.util.concurrent.Executors.newSingleThreadExecutor { r->Thread(r,"CamberCameraCleanup") }
    private var pendingCleanup: java.util.concurrent.Future<*>? = null
    fun startPractice() {
        val request = synchronized(this) {
            if(!foreground || sharing || disposed) return
            releaseCamera();captureEpoch to pendingCleanup
        }
        request.second?.get()
        synchronized(this) {
        if(!foreground || sharing || disposed || captureEpoch!=request.first) return
        PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions.builder(context).createInitializationOptions())
        factory=PeerConnectionFactory.builder().createPeerConnectionFactory()
        val cameras=Camera2Enumerator(context)
        val name=cameras.deviceNames.firstOrNull {if(front)cameras.isFrontFacing(it) else cameras.isBackFacing(it)} ?: error("Camera unavailable")
        capturer=cameras.createCapturer(name,cameraEvents(captureEpoch));source=factory!!.createVideoSource(false)
        texture=SurfaceTextureHelper.create("CamberPractice",egl.eglBaseContext)
        orientation.cameraOpened(name)
        val practiceEpoch=captureEpoch
        capturer!!.initialize(texture,context,orientation.observer(source!!.capturerObserver){foreground && captureEpoch==practiceEpoch})
        track=factory!!.createVideoTrack("camber-practice",source);track!!.addSink(gestures)
        isPracticing=true;gestures.setEnabled(true,generation);capturer!!.startCapture(1280,720,30)
        update("Local gesture practice. Camera is on; no video is sent to your PC.",false)
        }
    }
    @Synchronized fun endPractice() {releaseCamera();update("Practice finished. Camera is off.",false)}
    @Volatile private var gestureRequested = false
    private val gestures = GestureAnalyzer(context, { reaction, analyzedGeneration ->
        if (sharing && analyzedGeneration == generation) socket?.send(JSONObject().put("type", "reaction")
            .put("id",java.util.UUID.randomUUID().toString().replace("-",""))
            .put("reaction", reaction).put("generation", analyzedGeneration).toString())
    }, { message, available ->
        if (!available) send("capabilities", capabilities(false))
        gestureUpdate(if(isPracticing) message.replace("Reaction requested.","Practice only; nothing is sent.") else message)
    })
    @Volatile private var approvalRequested = false
    private val queuedCandidates = mutableListOf<IceCandidate>()
    private var remoteReady = false

    private fun pinnedClient(origin: String, fingerprint: String): OkHttpClient {
        require(fingerprint.matches(Regex("[A-Fa-f0-9]{64}"))) { "Invalid pairing fingerprint" }
        val uri = URI(origin)
        require(uri.scheme == "https" && uri.userInfo == null && (uri.path.isNullOrEmpty() || uri.path == "/"))
        val addresses = InetAddress.getAllByName(uri.host)
        require(addresses.isNotEmpty() && addresses.all { it.isSiteLocalAddress || it.isLinkLocalAddress }) { "Only local network PCs are supported" }
        val trust = object : X509TrustManager {
            override fun getAcceptedIssuers() = emptyArray<X509Certificate>()
            override fun checkClientTrusted(chain: Array<X509Certificate>, auth: String) { throw CertificateException("Client certificates unsupported") }
            override fun checkServerTrusted(chain: Array<X509Certificate>, auth: String) {
                val actual = MessageDigest.getInstance("SHA-256").digest(chain.first().encoded)
                val expected = fingerprint.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                if (!MessageDigest.isEqual(actual, expected)) throw CertificateException("PC identity changed. Pair again.")
                chain.first().checkValidity()
            }
        }
        val tls = SSLContext.getInstance("TLS").apply { init(null, arrayOf(trust), SecureRandom()) }
        return OkHttpClient.Builder().sslSocketFactory(tls.socketFactory, trust).hostnameVerifier { _, _ -> true }
            .pingInterval(java.time.Duration.ofSeconds(5)).followRedirects(false).followSslRedirects(false).build()
    }
    fun pair(payload: String) {
        val qr = JSONObject(payload)
        require(qr.getInt("version") == 1) { "Update both Camber applications" }
        require(Instant.now().isBefore(Instant.parse(qr.getString("expiresAt")))) { "Pairing code expired" }
        val endpoint = qr.getString("endpoint").trimEnd('/')
        val http = pinnedClient(endpoint, qr.getString("fingerprint"))
        val body = JSONObject().put("version", 1).put("secret", qr.getString("secret"))
            .put("name", Build.MODEL.take(80)).put("platform", "android")
        http.newCall(Request.Builder().url("$endpoint/pair").post(body.toString().toRequestBody("application/json".toMediaType())).build()).execute().use { response ->
            require(response.isSuccessful) { "Pairing refused. Create a new code on your PC." }
            val result = JSONObject(response.body!!.string())
            enrollment = JSONObject().put("endpoint", endpoint).put("fingerprint", qr.getString("fingerprint"))
                .put("name", qr.getString("name")).put("id", result.getJSONObject("device").getString("id")).put("token", result.getString("token"))
            store.save(enrollment!!)
        }
        connect()
    }
    private fun capabilities(gesture:Boolean)=JSONObject().put("gestureReactions",gesture).put("sessionLease",true)
        .put("videoQuality",true).put("enhancementControls",true).put("automaticOrientation",orientation.available)
    fun connect() {
        if(disposed) return
        val info = enrollment ?: run { update("Scan a pairing code on your PC.", false); return }
        isConnected=false
        client = pinnedClient(info.getString("endpoint"), info.getString("fingerprint"))
        socket?.cancel()
        val request = Request.Builder().url(info.getString("endpoint").replaceFirst("https://", "wss://") + "/session")
            .header("X-Camber-Device", info.getString("id")).header("Authorization", "Bearer " + info.getString("token")).build()
        socket = client!!.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) { if(webSocket !== socket || !foreground) {webSocket.cancel();return};isConnected=true;lastPong=android.os.SystemClock.elapsedRealtime();send("capabilities", capabilities(true)); update("Connected to ${info.getString("name")}. Tap Start to share.", false) }
            override fun onMessage(webSocket: WebSocket, text: String) {
                if (webSocket !== socket) return
                if (text.length > 65536) { webSocket.cancel(); return }
                try { message(JSONObject(text)) } catch (e: Exception) { releaseCamera(); update(e.message ?: "Camera connection failed", false) }
            }
            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) { if (webSocket === socket) { isConnected=false;approvalRequested = false; releaseCamera(); update("Connection lost. Reconnect to your PC.", false) } }
            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) { if (webSocket === socket) { isConnected=false;approvalRequested = false; releaseCamera(); update("Camera disconnected.", false) } }
        })
    }
    private fun send(type: String, data: JSONObject = JSONObject()) { data.put("type", type); data.put("generation", generation); socket?.send(data.toString()) }
    fun start() { if(!foreground || !isConnected) {update("Out of focus or disconnected. Reconnect before sharing.",false);return}; approvalRequested = true; send("approve",JSONObject().put("camera",if(front) "front" else "back")) }
    @Synchronized fun pause() { approvalRequested = false; track?.setEnabled(false); send("pause"); releaseCamera(); update("Camera paused. No video is being shared.", false) }
    @Synchronized fun stop() { approvalRequested = false; track?.setEnabled(false); send("stop"); releaseCamera(); update("Camera stopped. Tap Start to share again.", false) }
    fun forget() { stop(); socket?.cancel(); store.clear(); enrollment = null; update("Pairing removed from this phone.", false) }
    private fun message(j: JSONObject) {
        if (j.has("state") && j.getString("type") in listOf("state", "pause", "stop", "error")) generation = j.getJSONObject("state").getLong("generation")
        when (j.getString("type")) {
            "videoSettings" -> {
                if(j.getLong("generation")!=generation)return
                val requested=j.optString("requested","original")
                val applied=j.optString("applied","original")
                videoUpdate(requested,"Using ${when(applied){"natural"->"Natural";"lowLight"->"Low light";else->"Original"}}. ${j.optString("reason")} ${j.optString("captureInfo")}",j.optInt("requestedHeight",1080))
            }
            "quality" -> applyVideoQuality(j)
            "pong" -> lastPong=android.os.SystemClock.elapsedRealtime()
            "reactionResult" -> { if(j.getLong("generation")==generation && sharing) gestureUpdate(j.optString("message","Reaction updated.")) }
            "effects" -> {
                if (j.getLong("generation") < generation) return
                gestureRequested = j.getBoolean("enabled")
                gestures.setEnabled(isPracticing || (gestureRequested && sharing && localGesturesEnabled), generation)
            }
            "approved" -> { val g = j.getJSONObject("state").getLong("generation"); if (!approvalRequested || g < generation) return; generation = g
                j.getJSONObject("state").optJSONObject("configuration")?.let { configuration ->
                    val capture=configuration.getJSONObject("capture");width=capture.getInt("width");height=capture.getInt("height")
                    front=configuration.getString("camera")=="front"
                }
                beginVideo() }
            "answer" -> {
                if (j.getLong("generation") != generation || !sharing || !foreground) return
                peer?.setRemoteDescription(sdpObserver(captureEpoch,generation,onSet = { remoteReady = true; synchronized(queuedCandidates) { queuedCandidates.forEach { peer?.addIceCandidate(it) }; queuedCandidates.clear() } }),
                    SessionDescription(SessionDescription.Type.ANSWER, j.getString("sdp")))
            }
            "candidate" -> {
                if (j.getLong("generation") != generation || !sharing || !foreground) return
                val c = IceCandidate(j.getString("sdpMid"), j.getInt("sdpMLineIndex"), j.getString("candidate"))
                synchronized(queuedCandidates) { if (remoteReady) peer?.addIceCandidate(c) else if(queuedCandidates.size<128) queuedCandidates.add(c) else throw IllegalStateException("Video negotiation exceeded its limits") }
            }
            "pause", "stop" -> { approvalRequested = false; releaseCamera(); update("Camera stopped on your PC. Tap Start to share again.", false) }
            "resumeRequested" -> update("Your PC requests video. Tap Start to approve.", false)
            "error" -> { releaseCamera(); update(j.getString("message"), false) }
            "configure" -> configure(j.getJSONObject("command"))
        }
    }
    @Synchronized private fun configure(command: JSONObject) {
        if(command.getLong("generation") != generation || !sharing || !foreground) return
        if(configurationInFlight) {
            send("ack",JSONObject().put("ack",JSONObject().put("id",command.getString("id"))
                .put("ok",false).put("generation",generation).put("error","Wait for the camera switch to finish.")))
            return
        }
        val config=command.getJSONObject("payload")
        val requested=config.getJSONObject("capture")
        val newWidth=requested.getInt("width"); val newHeight=requested.getInt("height")
        require(requested.getInt("fps")==30 && ((newWidth==1920 && newHeight==1080) || (newWidth==1280 && newHeight==720)))
        require(config.getString("camera") in listOf("front","back"))
        val newFront=config.getString("camera")=="front"
        val current=capturer ?: return
        val epoch=captureEpoch
        val operation=++cameraOperation
        fun finish(ok: Boolean, error: String? = null) { synchronized(this@CameraLink) {
            if(epoch!=captureEpoch || operation!=cameraOperation || !sharing || !foreground || command.getLong("generation")!=generation) return
            switchingCamera=false; configurationInFlight=false
            if(ok) { front=newFront; width=newWidth; height=newHeight; preferences.edit().putBoolean("frontCamera",front).apply() }
            send("ack",JSONObject().put("ack",JSONObject().put("id",command.getString("id"))
                .put("ok",ok).put("generation",generation).put("applied",if(ok) config else JSONObject.NULL)
                .put("error",error ?: JSONObject.NULL)))
            update(if(ok) "${if(front) "Front" else "Rear"} camera sharing. Your microphone is off." else error ?: "Camera switch failed.",true)
        } }
        val enumerator=Camera2Enumerator(context)
        val name=enumerator.deviceNames.firstOrNull { if(newFront) enumerator.isFrontFacing(it) else enumerator.isBackFacing(it) }
        if(name==null) { finish(false,"Requested camera is unavailable."); return }
        try {
            current.changeCaptureFormat(newWidth,newHeight,30)
            if(newFront==front) finish(true)
            else {
                configurationInFlight=true; switchingCamera=true; update("Switching camera…",true)
                heartbeat.postDelayed({ synchronized(this) {
                    if(configurationInFlight && epoch==captureEpoch && operation==cameraOperation) { stop(); update("Camera switch timed out. Start sharing to try again.",false) }
                } },5000)
                current.switchCamera(object : CameraVideoCapturer.CameraSwitchHandler {
                    override fun onCameraSwitchDone(isFrontCamera: Boolean) { heartbeat.post { finish(isFrontCamera==newFront,if(isFrontCamera!=newFront) "The phone selected a different camera." else null) } }
                    override fun onCameraSwitchError(error: String) { heartbeat.post { finish(false,"Could not switch camera. Try again.") } }
                },name)
            }
        } catch (_: Exception) { finish(false,"Could not apply camera settings. Try again.") }
    }
    private fun beginVideo() {
        val request=synchronized(this) {
            if (!approvalRequested || !foreground || !isConnected || disposed) return
            approvalRequested = false
            releaseCamera();captureEpoch to pendingCleanup
        }
        request.second?.get()
        synchronized(this) {
        if(!foreground || !isConnected || disposed || captureEpoch!=request.first) return
        val sessionEpoch=captureEpoch
        val sessionGeneration=generation
        PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions.builder(context).createInitializationOptions())
        val encoder = DefaultVideoEncoderFactory(egl.eglBaseContext, true, false)
        factory = PeerConnectionFactory.builder().setVideoEncoderFactory(encoder).setVideoDecoderFactory(DefaultVideoDecoderFactory(egl.eglBaseContext)).createPeerConnectionFactory()
        val enumerator = Camera2Enumerator(context)
        val cameraName = enumerator.deviceNames.firstOrNull { if (front) enumerator.isFrontFacing(it) else enumerator.isBackFacing(it) }
            ?: throw IllegalStateException("Requested camera is unavailable")
        capturer = enumerator.createCapturer(cameraName, cameraEvents(sessionEpoch))
        source = factory!!.createVideoSource(false)
        texture = SurfaceTextureHelper.create("CamberCapture", egl.eglBaseContext)
        orientation.cameraOpened(cameraName)
        capturer!!.initialize(texture, context, orientation.observer(source!!.capturerObserver){foreground && captureEpoch==sessionEpoch})
        peer = factory!!.createPeerConnection(PeerConnection.RTCConfiguration(emptyList()).apply { sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN }, observer(sessionEpoch,sessionGeneration))
            ?: throw IllegalStateException("Could not create video connection")
        track = factory!!.createVideoTrack("camber-camera", source)
        track!!.addSink(gestures)
        sender=peer!!.addTrack(track, listOf("camber"))
        videoStatistics.reset();enforcedBitrate=null
        sender?.parameters?.let { p ->
            p.encodings.forEach {it.maxBitrateBps=8_000_000;it.minBitrateBps=null;it.maxFramerate=30}
            if(p.encodings.isNotEmpty() && sender!!.setParameters(p))enforcedBitrate=8_000_000
        }
        val h264 = factory!!.getRtpSenderCapabilities(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO).codecs.filter { it.mimeType.equals("video/H264", true) }
        require(h264.isNotEmpty()) { "H.264 encoding is unavailable on this phone" }
        peer!!.transceivers.first().setCodecPreferences(h264)
        capturer!!.startCapture(width, height, 30); sharing = true; remoteReady = false
        gestures.setEnabled(gestureRequested && localGesturesEnabled, generation)
        peer!!.createOffer(sdpObserver(sessionEpoch,sessionGeneration,onCreate = { description ->
            peer?.setLocalDescription(sdpObserver(sessionEpoch,sessionGeneration,onSet = { send("offer", JSONObject().put("sdp", description.description)) }), description)
        }), MediaConstraints())
        update("Sharing camera with ${enrollment!!.getString("name")}. Your microphone is off.", true)
        send("selectCamera",JSONObject().put("camera",if(front) "front" else "back"))
        }
    }
    @Synchronized private fun releaseCamera() {
        orientation.resetSample()
        captureEpoch++;switchingCamera=false;configurationInFlight=false;sharing=false;isPracticing=false
        gestures.setEnabled(false,generation)
        track?.setEnabled(false);track?.removeSink(gestures)
        val oldCapturer=capturer;val oldTrack=track;val oldPeer=peer;val oldSource=source;val oldTexture=texture;val oldFactory=factory
        videoStatistics.reset();videoSampleBusy=false;sender=null;enforcedBitrate=null
        capturer=null;track=null;peer=null;source=null;texture=null;factory=null;remoteReady=false
        synchronized(queuedCandidates){queuedCandidates.clear()}
        if(cleanupWorker.isShutdown)return
        pendingCleanup=cleanupWorker.submit {
            try{oldCapturer?.stopCapture()}catch(_:Exception){}
            listOf<()->Unit>({oldCapturer?.dispose();Unit},{oldTrack?.dispose();Unit},{oldPeer?.close();Unit},
                {oldPeer?.dispose();Unit},{oldSource?.dispose();Unit},{oldTexture?.dispose();Unit},{oldFactory?.dispose();Unit})
                .forEach { action->try{action()}catch(_:Exception){} }
        }
    }
    private fun cameraEvents(epoch:Long)=object:CameraVideoCapturer.CameraEventsHandler {
        private fun interrupted(message:String) {heartbeat.post {synchronized(this@CameraLink) {
            if(disposed || epoch!=captureEpoch || (!sharing && !isPracticing))return@synchronized
            approvalRequested=false;track?.setEnabled(false);send("stop");releaseCamera();update(message,false)
        } } }
        override fun onCameraError(errorDescription:String)=interrupted("Camera unavailable. Close other camera apps and tap Start again.")
        override fun onCameraDisconnected()=interrupted("Camera disconnected. Tap Start when it is available again.")
        override fun onCameraFreezed(errorDescription:String)=interrupted("Camera stopped responding. Tap Start to try again.")
        override fun onCameraOpening(cameraName:String) {if(epoch==captureEpoch)orientation.cameraOpened(cameraName)}
        override fun onFirstFrameAvailable() {}
        override fun onCameraClosed() {}
    }
    private fun sessionActive(epoch: Long, g: Long)=!disposed && foreground && sharing && epoch==captureEpoch && g==generation
    private fun sdpObserver(epoch: Long, g: Long, onCreate: (SessionDescription) -> Unit = {}, onSet: () -> Unit = {}) = object : SdpObserver {
        override fun onCreateSuccess(sdp: SessionDescription) { synchronized(this@CameraLink) {if(sessionActive(epoch,g)) onCreate(sdp)} }
        override fun onSetSuccess() { synchronized(this@CameraLink) {if(sessionActive(epoch,g)) onSet()} }
        private fun failed() { synchronized(this@CameraLink) {if(sessionActive(epoch,g)) {stop();update("Video negotiation failed. Reconnect and tap Start to try again.",false)}} }
        override fun onCreateFailure(error: String) = failed()
        override fun onSetFailure(error: String) = failed()
    }
    private fun observer(epoch: Long,g: Long) = object : PeerConnection.Observer {
        override fun onIceCandidate(c: IceCandidate) { synchronized(this@CameraLink) {if(sessionActive(epoch,g)) send("candidate", JSONObject().put("candidate", c.sdp).put("sdpMid", c.sdpMid).put("sdpMLineIndex", c.sdpMLineIndex))} }
        override fun onIceConnectionChange(state: PeerConnection.IceConnectionState) { synchronized(this@CameraLink) {if(sessionActive(epoch,g) && state == PeerConnection.IceConnectionState.FAILED) stop()} }
        override fun onSignalingChange(state: PeerConnection.SignalingState) {}
        override fun onIceConnectionReceivingChange(receiving: Boolean) {}
        override fun onIceGatheringChange(state: PeerConnection.IceGatheringState) {}
        override fun onIceCandidatesRemoved(candidates: Array<out IceCandidate>) {}
        override fun onAddStream(stream: MediaStream) {}
        override fun onRemoveStream(stream: MediaStream) {}
        override fun onDataChannel(channel: DataChannel) {}
        override fun onRenegotiationNeeded() {}
        override fun onAddTrack(receiver: RtpReceiver, streams: Array<out MediaStream>) {}
    }
    fun dispose() { disposed=true;foreground=false; heartbeat.removeCallbacks(beat);stop(); orientation.close(); gestures.close(); val oldSocket=socket;socket=null;oldSocket?.cancel(); client?.dispatcher?.executorService?.shutdown(); cleanupWorker.execute {egl.release()};cleanupWorker.shutdown() }
}
