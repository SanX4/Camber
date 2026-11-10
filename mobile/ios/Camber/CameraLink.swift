import UIKit
import AVFoundation
import WebRTC
import Darwin

final class CameraLink: NSObject, RTCPeerConnectionDelegate {
    var update: ((String, Bool) -> Void)?
    var localTrack: ((RTCVideoTrack?) -> Void)?
    private var enrollment = EnrollmentStore.load()
    private var pinned: PinnedSession?
    private var socket: URLSessionWebSocketTask?
    private var factory: RTCPeerConnectionFactory?
    private var peer: RTCPeerConnection?
    private var capturer: RTCCameraVideoCapturer?
    private var track: RTCVideoTrack?
    private var generation: Int64 = 0
    private var front = false
    private var width: Int32 = 1920
    private var height: Int32 = 1080
    private var remoteReady = false
    private var approvalRequested = false
    private var captureEpoch = 0
    private var candidates: [RTCIceCandidate] = []
    private func status(_ message: String, _ sharing: Bool = false) {
        DispatchQueue.main.async { self.update?(message, sharing); UIApplication.shared.isIdleTimerDisabled = sharing }
    }
    func pair(_ payload: String) async throws {
        guard let data = payload.data(using: .utf8), let qr = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              qr["version"] as? Int == 1, let endpoint = qr["endpoint"] as? String, let url = URL(string: endpoint),
              url.scheme == "https", url.user == nil, let host = url.host,
              isLocalAddress(host),
              let fingerprint = qr["fingerprint"] as? String, fingerprint.count == 64,
              let secret = qr["secret"] as? String, let name = qr["name"] as? String,
              let expires = qr["expiresAt"] as? String else { throw CamberError.message("This is not a valid Camber pairing code.") }
        let formatter = ISO8601DateFormatter(); formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        guard let date = formatter.date(from: expires), date > Date() else { throw CamberError.message("Pairing code expired. Create a new one on your PC.") }
        let tls = PinnedSession(fingerprint: fingerprint)
        var request = URLRequest(url: url.appendingPathComponent("pair")); request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: ["version": 1, "secret": secret, "name": UIDevice.current.name, "platform": "ios"])
        let (responseData, response) = try await tls.session.data(for: request)
        guard (response as? HTTPURLResponse)?.statusCode == 200,
              let result = try JSONSerialization.jsonObject(with: responseData) as? [String: Any],
              let device = result["device"] as? [String: Any], let id = device["id"] as? String, let token = result["token"] as? String else {
            throw CamberError.message("Pairing refused. Create a new code on your PC.")
        }
        let value = Enrollment(endpoint: url, fingerprint: fingerprint, name: name, id: id, token: token)
        try EnrollmentStore.save(value); enrollment = value; connect()
    }
    func connect() {
        guard let info = enrollment else { status("Scan the code in Camber on Windows."); return }
        socket?.cancel(with: .goingAway, reason: nil)
        pinned = PinnedSession(fingerprint: info.fingerprint)
        var parts = URLComponents(url: info.endpoint.appendingPathComponent("session"), resolvingAgainstBaseURL: false)!
        parts.scheme = "wss"
        var request = URLRequest(url: parts.url!)
        request.setValue(info.id, forHTTPHeaderField: "X-Camber-Device")
        request.setValue("Bearer \(info.token)", forHTTPHeaderField: "Authorization")
        socket = pinned!.session.webSocketTask(with: request); socket?.resume(); receive()
        status("Connected to \(info.name). Tap Start to approve camera sharing.")
    }
    private func send(_ type: String, _ values: [String: Any] = [:]) {
        var value = values; value["type"] = type; value["generation"] = generation
        guard let data = try? JSONSerialization.data(withJSONObject: value), let text = String(data: data, encoding: .utf8) else { return }
        socket?.send(.string(text)) { [weak self] error in if error != nil { self?.releaseCamera(); self?.status("Connection lost. Reconnect to your PC.") } }
    }
    func start() {
        AVCaptureDevice.requestAccess(for: .video) { [weak self] allowed in
            guard allowed else { self?.status("Enable camera access for Camber in iPhone Settings."); return }
            self?.approvalRequested = true; self?.send("approve")
        }
    }
    func pause() { approvalRequested = false; releaseCamera(); send("pause"); status("Camera paused. No live video is shared.") }
    func stop() { approvalRequested = false; releaseCamera(); send("stop"); status("Camera stopped. Tap Start to share again.") }
    func forget() { stop(); socket?.cancel(with: .normalClosure, reason: nil); enrollment = nil; EnrollmentStore.remove(); status("Pairing removed from this phone.") }
    private func receive() {
        socket?.receive { [weak self] result in
            guard let self else { return }
            do {
                let message = try result.get()
                guard case .string(let text) = message, text.utf8.count <= 65536,
                      let data = text.data(using: .utf8), let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else { throw CamberError.message("Invalid camera message.") }
                DispatchQueue.main.async { self.handle(json); self.receive() }
            } catch { self.releaseCamera(); self.status("Connection lost. Reconnect to your PC.") }
        }
    }
    private func isLocalAddress(_ host: String) -> Bool {
        var ipv4 = in_addr()
        if inet_pton(AF_INET, host, &ipv4) == 1 {
            let bytes = withUnsafeBytes(of: ipv4.s_addr) { Array($0) }
            return bytes[0] == 10 || (bytes[0] == 192 && bytes[1] == 168) ||
                (bytes[0] == 172 && (16...31).contains(bytes[1])) || (bytes[0] == 169 && bytes[1] == 254)
        }
        var ipv6 = in6_addr()
        let unscoped = String(host.split(separator: "%").first ?? "")
        if inet_pton(AF_INET6, unscoped, &ipv6) == 1 {
            let bytes = withUnsafeBytes(of: ipv6) { Array($0) }
            return bytes[0] & 0xfe == 0xfc || (bytes[0] == 0xfe && bytes[1] & 0xc0 == 0x80)
        }
        return false
    }
    private func handle(_ message: [String: Any]) {
        guard let type = message["type"] as? String else { return }
        if ["state", "pause", "stop", "error"].contains(type), let state = message["state"] as? [String: Any], let g = state["generation"] as? Int64 { generation = g }
        switch type {
        case "approved":
            guard approvalRequested else { return }; approvalRequested = false
            guard let state = message["state"] as? [String: Any], let g = state["generation"] as? Int64 else { return }
            guard g >= generation else { return }
            generation = g; beginVideo()
        case "answer":
            guard message["generation"] as? Int64 == generation, let sdp = message["sdp"] as? String else { return }
            peer?.setRemoteDescription(RTCSessionDescription(type: .answer, sdp: sdp)) { [weak self] error in
                guard let self, error == nil else { return }
                self.remoteReady = true; self.candidates.forEach { self.peer?.add($0) { _ in } }; self.candidates.removeAll()
            }
        case "candidate":
            guard message["generation"] as? Int64 == generation, let candidate = message["candidate"] as? String,
                  let mid = message["sdpMid"] as? String, let index = message["sdpMLineIndex"] as? Int32 else { return }
            let ice = RTCIceCandidate(sdp: candidate, sdpMLineIndex: index, sdpMid: mid)
            if remoteReady { peer?.add(ice) { _ in } } else { candidates.append(ice) }
        case "pause", "stop": approvalRequested = false; releaseCamera(); status("Camera stopped on your PC. Tap Start to share again.")
        case "resumeRequested": status("Your PC requests video. Tap Start to approve.")
        case "error": releaseCamera(); status(message["message"] as? String ?? "Video connection failed.")
        case "configure": applyConfiguration(message)
        default: break
        }
    }
    private func beginVideo() {
        releaseCamera(); RTCInitializeSSL()
        let epoch = captureEpoch
        let encoder = RTCDefaultVideoEncoderFactory(); encoder.preferredCodec = RTCVideoCodecInfo(name: kRTCH264CodecName)
        factory = RTCPeerConnectionFactory(encoderFactory: encoder, decoderFactory: RTCDefaultVideoDecoderFactory())
        let config = RTCConfiguration(); config.sdpSemantics = .unifiedPlan; config.iceServers = []
        peer = factory!.peerConnection(with: config, constraints: RTCMediaConstraints(mandatoryConstraints: nil, optionalConstraints: nil), delegate: self)
        let source = factory!.videoSource(); capturer = RTCCameraVideoCapturer(delegate: source)
        track = factory!.videoTrack(with: source, trackId: "camber-camera"); peer?.add(track!, streamIds: ["camber"])
        startCapture { [weak self] error in
            guard let self, self.captureEpoch == epoch, self.capturer != nil else { return }
            if let error { self.releaseCamera(); self.status(error.localizedDescription); return }
            DispatchQueue.main.async { self.localTrack?(self.track) }
            self.peer?.offer(for: RTCMediaConstraints(mandatoryConstraints: ["OfferToReceiveAudio": "false", "OfferToReceiveVideo": "false"], optionalConstraints: nil)) { [weak self] sdp, error in
                guard let self, let sdp, error == nil else { return }
                self.peer?.setLocalDescription(sdp) { error in if error == nil { self.send("offer", ["sdp": sdp.sdp]) } }
            }
            self.status("Sharing camera with \(self.enrollment?.name ?? "your PC"). Microphone is off.", true)
        }
    }
    private func startCapture(_ completion: @escaping (Error?) -> Void) {
        guard let device = RTCCameraVideoCapturer.captureDevices().first(where: { $0.position == (front ? .front : .back) }) else { completion(CamberError.message("Requested camera unavailable.")); return }
        let formats = RTCCameraVideoCapturer.supportedFormats(for: device).filter {
            let d = CMVideoFormatDescriptionGetDimensions($0.formatDescription)
            return d.width <= 1920 && d.height <= 1080 && $0.videoSupportedFrameRateRanges.contains { $0.maxFrameRate >= 30 }
        }
        guard let format = formats.min(by: {
            let a = CMVideoFormatDescriptionGetDimensions($0.formatDescription), b = CMVideoFormatDescriptionGetDimensions($1.formatDescription)
            return abs(a.width - width) + abs(a.height - height) < abs(b.width - width) + abs(b.height - height)
        }) else { completion(CamberError.message("No supported 30 fps camera mode.")); return }
        capturer?.startCapture(with: device, format: format, fps: 30, completionHandler: completion)
    }
    private func applyConfiguration(_ message: [String: Any]) {
        guard let command = message["command"] as? [String: Any], command["generation"] as? Int64 == generation,
              let config = command["payload"] as? [String: Any], let mode = config["capture"] as? [String: Any],
              let id = command["id"] as? String, let w = mode["width"] as? Int32, let h = mode["height"] as? Int32,
              (w == 1920 && h == 1080) || (w == 1280 && h == 720), capturer != nil else { return }
        front = config["camera"] as? String == "front"; width = w; height = h
        capturer?.stopCapture { [weak self] in self?.startCapture { error in
            self?.send("ack", ["ack": ["id": id, "ok": error == nil, "generation": self?.generation ?? 0, "applied": config, "error": error?.localizedDescription ?? ""]])
        } }
    }
    private func releaseCamera() {
        captureEpoch += 1
        capturer?.stopCapture(); capturer = nil; track = nil; peer?.close(); peer = nil; factory = nil
        remoteReady = false; candidates.removeAll()
        DispatchQueue.main.async { self.localTrack?(nil); UIApplication.shared.isIdleTimerDisabled = false }
    }
    func peerConnection(_ peerConnection: RTCPeerConnection, didGenerate candidate: RTCIceCandidate) { send("candidate", ["candidate": candidate.sdp, "sdpMid": candidate.sdpMid ?? "0", "sdpMLineIndex": candidate.sdpMLineIndex]) }
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange stateChanged: RTCSignalingState) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didAdd stream: RTCMediaStream) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didRemove stream: RTCMediaStream) {}
    func peerConnectionShouldNegotiate(_ peerConnection: RTCPeerConnection) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceConnectionState) { if newState == .failed { stop() } }
    func peerConnection(_ peerConnection: RTCPeerConnection, didChange newState: RTCIceGatheringState) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didRemove candidates: [RTCIceCandidate]) {}
    func peerConnection(_ peerConnection: RTCPeerConnection, didOpen dataChannel: RTCDataChannel) {}
}
