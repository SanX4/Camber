# Mirroring and reactions

Camber defaults to natural video: text reads normally. **Mirror outgoing
video** changes the virtual camera seen by other applications. **Mirror this
preview** changes only the Camber window. These controls apply immediately and
are stored locally; neither requires a phone camera restart.

Open **Camera > Reactions** to enable effects, control automatic gestures, reduce
motion or play an effect manually. Effects are part of the virtual-camera image,
including in Discord, Meet and recording applications that select Camber Camera.

Android automatic gestures require the matching companion APK. Keep the app visible,
enable hand gestures on Windows and hold a pose for about a second with hands
away from your face. Lower your hands or change poses for at least 350ms before trying again, and allow
at least four seconds between effects. Keep both hands visible for two-hand poses.

| Gesture | Reaction |
| --- | --- |
| One thumb up | Thumbs up |
| One thumb down | Thumbs down |
| One V/peace sign | Balloons |
| Two V/peace signs | Confetti |
| Two thumbs up | Fireworks |
| Two thumbs down | Rain |
| Two hands forming a heart, index tips together above touching thumbs | Hearts |
| Two horns signs, index and pinky extended | Lasers |

Recognition runs entirely on the phone using a bundled model. No frame or hand
landmark is uploaded or saved. Only a reaction name, random request ID and session generation travel
to the paired PC. Bright, even lighting and a clear view of both hands help.
These controls and automatic gestures are independent of meeting-app reactions.

If gesture initialization fails, camera streaming continues and Windows explains
that gestures are unavailable. Use manual buttons, or reconnect to retry. Turning
reactions off clears the current effect. Pause and Stop clear all live output and
do not allow queued gestures from the old session to start an animation.

Reduced motion uses stationary effects with a gentle fade. Windows'
disabled animation preference enables reduced motion when Camber starts.

Camber uses original 2D artwork and animations. Apple artwork and implementation
code are not included. The gesture model/SDK version, provenance and checksum are
recorded in `tools/dependencies.json` and `third_party/NOTICE.md`.

iOS is excluded from this beta. Existing iPhone source is retained for future work.

The temporal policy requires a 600ms single-hand or 800ms two-hand hold, at least four observations, and a
four-second cooldown. Tracking gaps longer than 250ms restart the hold. The phone
shows hold progress, cooldown and release guidance. Recognition runs at up to
12Hz, reducing to 8/6Hz under thermal pressure. Horns use 3D joint angles when
world landmarks are available; two-hand hearts use aspect-corrected geometry.
Matching two-hand model categories use a 60% per-hand threshold; single-hand
categories retain 70%. A matching tentative second hand blocks premature single
reactions. Rounded hearts allow close index knuckles, while requiring separated
wrists, joined fingertips and a visible opening. Horns allow relaxed index/pinky
joints while requiring folded middle/ring fingers on both hands. Recognized pose
names remain visible until release. The local diagnostic log contains only hand
counts, category scores, policy phases and boolean shape checks, never frames or
landmark coordinates.
These changes have not established the physical accuracy targets yet.

Try gestures locally opens the selected camera for inference only. It creates no
peer connection, SDP, or network video track. Leaving the app ends practice.
Camera interruptions stop capture; fresh Start is required to share again.

The Windows compositor keeps one pending decoded frame, replacing older pending
frames. D3D11/DirectXTK renders original lit sprites, continuous laser beams,
particle streaks and layered motion. CPU drawing remains the fallback; reduced
motion keeps stationary effects with fades. No segmentation or person occlusion
is applied. The current path includes GPU readback and CPU NV12 blending.

Lasers use smooth, time-based sweeps from the lower corners, layered atmospheric
scattering, colored shafts, near-white cores and traveling light glints. The
beams are a screen-space light show; they do not attach to tracked fingers.
Rain has three depth layers with independent speeds and phases, coherent wind,
soft drifting cloud haze, short impact droplets and expanding broken ripples
near the bottom edge. Both scenes fade in/out smoothly over the existing
four-second reaction lifetime. There is no strobing or frame-random jitter.
The same bounded scene geometry drives D3D11 and the CPU fallback; reduced
motion freezes the scene and retains only the fade. All artwork is generated
locally, without downloads, new models or additional runtime dependencies.

For synthetic animation inspection, run `Camber.ReactionTests.exe --preview
FOLDER`, then `python tools/render-reaction-previews.py FOLDER`. The latter is
an optional developer tool using local OpenCV/Pillow, not an app dependency.
It encodes generated frames only and checks that all 120 frames decode.
These previews cannot establish smoothness in an actual meeting app.

Phone reaction results mean accepted/rejected, not confirmed visible to a remote
participant. Output recognition latency must be measured from the actual camera
output. The request cache rejects duplicates and alternate GUID casing and is
bounded per session. Privacy generation checks precede publication.

To measure isolated compositor cost, run Camber.ReactionTests.exe --benchmark
PATH.json from a native build. This synthetic timing excludes camera, transport,
decoder and display and cannot certify glass-to-glass latency or call smoothness.
