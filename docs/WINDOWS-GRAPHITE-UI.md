# Windows graphite interface / 0.7.0

The Windows interface follows Android 0.6: graphite background #0B0E13, panel #171C24, raised surface #242C38, blue #80B8FF, pale text #F0F4FA and secondary text #B1BDCE. The approved Offset symbol remains the brand mark. Graphite is the initial appearance; Light and Windows-setting options remain available. Theme resources cover light, dark and Windows high contrast.

Camera is a preview-led workspace with a compact, authoritative session label. Routine connection state lives beside the preview; service and action errors use a visible warning. Rounded 28-32px surfaces use a quiet metallic highlight. Controls use native focus, hover, pressed and toggle behavior. Page transitions are finite, respect Windows animation preferences, and do not run on every status poll.

Connect, Devices and Settings use the same card treatment, spacing and typography. Pairing reveals its white QR panel only after a code is generated. It does not generate, expose or renew a pairing secret as a side effect of viewing the page. Device cards expose only the existing removal action. Diagnostics remain behind an expander.

Camera controls remain requests to the phone. The interface reports pending changes and the applied camera selection separately; privacy state and acknowledgment still come from the media engine. Stop and Pause depend on current consent. Leaving a page never starts capture. Closing the window still quits and stops sharing.

Camera controls stack below the preview when content width is below 880px; preview actions stack independently to avoid text overlap. Content gutters narrow on small windows. The main content scrolls, while navigation and the Stop action remain available. Native title-bar colours follow the selected theme and defer to Windows in high contrast.

The UI change does not alter signaling, encryption, phone lifecycle, virtual-camera registration or media processing. Existing Android 0.6.1 is compatible with this Windows update.

## Verification

The Windows publish and required-resource packaging check pass; all 29 core checks pass. The installed app was opened and the Camera and Settings layouts inspected. Switching from Graphite to Light and back was verified in the installed application, including the native title-bar colours. Native accessibility exposes labelled navigation, fields and disabled Stop/Pause/Apply in the unpaired state. Text and primary-button contrast checks pass in both palettes (minimum tested pair 5.04:1). A startup failure from subscribing to an unavailable Windows accessibility notification was found during the installed-app check and removed; title-bar contrast now follows the existing UI refresh instead.

Small-window layout and high-contrast resources are implemented. Full Narrator, display-scaling, live phone streaming and performance certification were not re-tested for this visual update. Interactive resizing could not be completed while the shared window was in use. The adaptive layout is implemented but still needs a dedicated manual resize check. No personal camera frames were captured.
