import UIKit
import AVFoundation
import WebRTC

final class CameraViewController: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
    private let link = CameraLink()
    private let status = UILabel()
    private let preview = RTCMTLVideoView(frame: .zero)
    private var scanner: AVCaptureSession?
    private var scannerLayer: AVCaptureVideoPreviewLayer?
    private var displayedTrack: RTCVideoTrack?
    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground
        let scroll = UIScrollView(); scroll.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(scroll)
        let stack = UIStackView(); stack.axis = .vertical; stack.spacing = 20; stack.translatesAutoresizingMaskIntoConstraints = false; scroll.addSubview(stack)
        let title = UILabel(); title.text = "Camber"; title.font = .preferredFont(forTextStyle: .largeTitle); title.adjustsFontForContentSizeCategory = true
        status.text = "Connect your phone to Camber on Windows."; status.numberOfLines = 0; status.font = .preferredFont(forTextStyle: .body); status.adjustsFontForContentSizeCategory = true
        preview.videoContentMode = .scaleAspectFit; preview.backgroundColor = .secondarySystemBackground; preview.layer.cornerRadius = 8; preview.clipsToBounds = true
        [title, status, preview].forEach { stack.addArrangedSubview($0) }
        preview.heightAnchor.constraint(equalTo: preview.widthAnchor, multiplier: 9.0 / 16.0).isActive = true
        func button(_ text: String, _ action: @escaping () -> Void) {
            var configuration = UIButton.Configuration.bordered(); configuration.title = text
            let item = UIButton(configuration: configuration, primaryAction: UIAction { _ in action() }); item.heightAnchor.constraint(greaterThanOrEqualToConstant: 48).isActive = true
            stack.addArrangedSubview(item)
        }
        button("Start sharing") { [weak self] in self?.link.start() }
        button("Pause camera") { [weak self] in self?.link.pause() }
        button("Stop sharing") { [weak self] in self?.link.stop() }
        button("Scan PC pairing code") { [weak self] in self?.scan() }
        button("Reconnect to paired PC") { [weak self] in self?.link.connect() }
        button("Forget paired PC") { [weak self] in self?.link.forget() }
        let privacy = UILabel(); privacy.text = "Video stays on your local network. Your microphone is never requested."; privacy.numberOfLines = 0; privacy.font = .preferredFont(forTextStyle: .footnote); stack.addArrangedSubview(privacy)
        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor), scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor), scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor), scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 24), stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -24), stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 24), stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -24), stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -48)
        ])
        link.update = { [weak self] message, _ in self?.status.text = message; UIAccessibility.post(notification: .announcement, argument: message) }
        link.localTrack = { [weak self] track in guard let self else { return }; self.displayedTrack?.remove(self.preview); self.displayedTrack = track; track?.add(self.preview) }
    }
    func stopSharing() { scanner?.stopRunning(); link.stop() }
    private func scan() {
        link.stop()
        AVCaptureDevice.requestAccess(for: .video) { [weak self] allowed in
            guard let self, allowed, let device = AVCaptureDevice.default(for: .video), let input = try? AVCaptureDeviceInput(device: device) else { return }
            let session = AVCaptureSession(); session.addInput(input); let output = AVCaptureMetadataOutput(); session.addOutput(output)
            output.setMetadataObjectsDelegate(self, queue: .main); output.metadataObjectTypes = [.qr]
            DispatchQueue.main.async {
                self.scanner = session; let layer = AVCaptureVideoPreviewLayer(session: session); layer.frame = self.preview.bounds; self.preview.layer.addSublayer(layer); self.scannerLayer = layer
                self.status.text = "Point your camera at the pairing code in Camber on Windows."
                DispatchQueue.global(qos: .userInitiated).async { session.startRunning() }
            }
        }
    }
    func metadataOutput(_ output: AVCaptureMetadataOutput, didOutput metadataObjects: [AVMetadataObject], from connection: AVCaptureConnection) {
        guard let text = (metadataObjects.first as? AVMetadataMachineReadableCodeObject)?.stringValue, scanner != nil else { return }
        scanner?.stopRunning(); scanner = nil; scannerLayer?.removeFromSuperlayer(); scannerLayer = nil
        Task { do { try await link.pair(text) } catch { status.text = error.localizedDescription } }
    }
}
