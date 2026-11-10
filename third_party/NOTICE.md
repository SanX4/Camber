# Third-party source and binaries

`native/virtual-camera/media_source.*`, `dllmain.cpp`, `frame_transform.*`, and
`native/companion/phone_state.h` derive from MyCam by whoissaaif at commit
`c951bbc63ee38c952396debf9790941db3782d2d`, under MIT. The original license is
preserved in `licenses/MyCam.txt`. Camber changes include a separate COM class,
limited advertised modes, per-user frame mappings, restricted ACLs, explicit
privacy gating, and a one-second stale-frame cutoff. The frame reader is replaced.
Source: https://github.com/whoissaaif/mycam

The desktop media worker links the MIT-licensed webrtc-sdk/libwebrtc wrapper over
Google WebRTC (BSD-style and third-party licenses); the wrapper's bundled
license is preserved in `licenses/libwebrtc.txt`. Android and Apple use separately
pinned platform distributions. Their versions and the Windows archive checksum
are in `tools/dependencies.json`.

Additional dependencies: Microsoft Windows App SDK and Windows SDK build tools,
QRCoder (MIT), System.Security.Cryptography.ProtectedData (MIT), nlohmann/json
(MIT), AndroidX/Compose (Apache-2.0), OkHttp (Apache-2.0), ZXing Android Embedded
(Apache-2.0), Kotlin, and platform SDKs. A full transitive license/SBOM audit is a
public-release gate. No DroidCam GPL source is included.

Android gesture reactions use Google MediaPipe Tasks Vision 0.10.35 and the
Gesture Recognizer float16 model, revision 1, under Apache-2.0. The bundled model
is unchanged; its URL and SHA-256 are recorded in `tools/dependencies.json`.
License terms are in `licenses/MediaPipe-Apache-2.0.txt`.
Source: https://github.com/google-ai-edge/mediapipe
Model: https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/1/gesture_recognizer.task
The geometric gesture policy and all reaction artwork/animation code are original
Camber code. No Apple graphics, models, or implementation code are included.

Windows layered reaction rendering uses DirectX Tool Kit (MIT), pinned to commit 5a8f5d01cc1328e6451b588617c9985fa5a2a8ab (may2026). Original Camber sprites are generated in the media worker. Android MediaPipe Tasks Vision/Core are now pinned to 0.10.35; the bundled gesture model is unchanged.

The Android runtime explicitly selects Guava 33.7.2-android and Protobuf Java Lite
4.28.2 over older MediaPipe transitive versions to address version advisories.
Both are Apache-2.0 dependencies. Their transitive coordinates and artifact
checksums are recorded in the Gradle lock and verification metadata. These
Java-only changes do not replace the bundled native libraries or gesture model.
