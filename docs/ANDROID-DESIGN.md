# Android 0.3

The native Jetpack Compose app uses one primary action derived from connection
state: connect, reconnect, start, pause or resume. Stop is always available while
sharing or paused. Pairing, reconnection and forgetting move into a rounded
Options bottom sheet; forgetting requires confirmation.

Soft teal/sage light and dark themes, 24 dp phone gutters, 16/24/32 dp rounded
surfaces, 48 dp minimum touch targets and 60 dp primary actions define the system.
System typography scales with Android text settings. The column scrolls on short
screens and remains limited to 600 dp on larger displays.

Native switch/sheet/ripple transitions and finite spring camera-state feedback
provide motion without perpetual loops. Compose honors Android animation scale.
Hand reactions have an actual persisted phone-side enable switch and still require
the PC's reaction setting. Camera frames remain in the existing media pipeline.

The approved Open C logo is installed as an adaptive Android icon and themed
monochrome icon, and as the Windows executable/window icon. See design/brand/final.
