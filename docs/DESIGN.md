# Interface specification

Taste read: a calm native Windows camera utility for everyday meetings and
remote work. Design variance 4/10, motion 2/10, density 4/10. Use official WinUI 3
Fluent controls and system theme resources. This is product UI, not a landing page.

Desktop: title bar, compact left navigation, content region, status/footer.
Camera: large 16:9 preview, with 288 px controls to the right. Controls move below
the preview below 780 px available content width. Dark preview background is a
media surface; surrounding application follows system appearance.

Typography: Segoe UI Variable through system styles. Title 28 px, section title
20 px, body 14 px; OS text scaling takes precedence. Spacing 4/8 px multiples,
24 px content insets, 8 px preview corners. Use a single blue interaction accent;
warnings and errors use platform semantic colors. Native focus and high contrast
resources remain intact.

Persistent Stop in the header; Pause beside preview and in the tray. Settings
separate appearance, privacy, diagnostics, and Quit. Connect explains camera-only
behavior and local networking. Device list has explicit revoke actions. The
preview never shows a stock face, fake camera image, or invented performance data.

The development application accurately labels missing native components and
uncertified performance. A polished public release must replace engineering
instructions with installer repair and consumer troubleshooting once packaging
and native registration are verified.

Accessibility: native labeled controls, visible keyboard focus, semantic status
bar, text alternatives for QR, reduced motion through platform defaults, system
theme/high contrast. Physical screen-reader and 100-250% display scaling tests
remain required. Android uses Material 3/Compose; iPhone uses UIKit Dynamic Type
and accessible controls. No microphone permission is present on either phone.
