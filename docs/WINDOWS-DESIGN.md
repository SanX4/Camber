# Windows 0.4 interface

The native WinUI app shares Android's calm teal/sage identity. Light, dark and
high-contrast semantic resources cover the background, panels and text. Controls
retain Fluent focus, keyboard, pointer and switch behavior, with rounded 16–24 px
corners, 44 px actions, consistent 20–32 px spacing and no decorative shadows.

Labeled top navigation exposes Camera, Connect, Devices and Settings. The preview
is the primary surface. A separate rounded panel progressively reveals Camera
setup, Mirroring and Reactions. Controls move beneath the preview at narrow widths.
Stop remains in the persistent header, independent of the current page.

Page changes use a compositor opacity transition with a soft deceleration curve.
Windows' disabled animation setting skips the transition. Native switch, button
and expander animations remain platform-controlled. No perpetual decorative
animation runs, and video frames stay outside the managed UI.

The design-system search's marketing layout recommendations were unsuitable for
this native utility; the existing product identity and native WinUI guidance
determine layout and colors. No media, protocol, pairing or privacy boundary was
changed for this interface update.
