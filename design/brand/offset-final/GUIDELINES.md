# Camber / Offset

Approved direction: C - Offset. A rounded diamond opens into a C-shaped field. Its shifted counter is the distinguishing feature. Keep the opening horizontal; the diamond geometry is already baked into the supplied paths.

## Files and use
- camber-symbol.svg: primary blue master, 256-unit canvas.
- camber-symbol-small.svg: wider opening for 16-24px uses.
- camber-symbol-black.svg and camber-symbol-white.svg: one-colour masters. The white drawing has a slightly wider opening to compensate for light shapes appearing heavier.
- camber-horizontal.svg: blue symbol with dark wordmark on light surfaces.
- camber-horizontal-dark.svg: blue symbol with pale wordmark on graphite.
- camber-stacked.svg and camber-wordmark.svg: alternate arrangements.
- exports/: PNG and SVG colour variants, app tile, favicon, web manifest and web icons.
- camber-windows.ico: 16/24/32/48/64/128/256px Windows icon.
- native-android/: adaptive foreground, in-app vector and adaptive-icon declarations. Copy into matching Android res directories.
- presentation.html: local, self-contained presentation with application-context mockups.

## Palette
Offset blue: #80B8FF, RGB 128/184/255, approximate process CMYK 50/28/0/0.
Graphite: #0B0E13, RGB 11/14/19, approximate process CMYK 42/26/0/93.
Pale ink: #F0F4FA, RGB 240/244/250.
Dark ink: #111820. Black and white are supplied for single-colour use.
CMYK values are starting points; print output requires a profile and proof. No Pantone match is assigned.

## Spacing and minimum sizes
Keep external clear space of at least 28/256 of the symbol canvas, approximately one opening height. The canvas already carries internal breathing room; retain it for alignment. Minimum symbol height 16px using the small cut; primary symbol at 32px and above. Minimum horizontal lockup width 160px. Below that, use the symbol alone.

The horizontal lockup uses a fixed symbol-to-wordmark gap and custom lowercase letterforms. Do not retype the wordmark. It is original filled vector artwork with no external font dependency. Supporting application text keeps native platform typography.

Use the blue symbol on graphite and the supplied dark lockup on pale surfaces. Use white on dark photography only with enough quiet space. For Windows use the multi-resolution ICO; for Android use the supplied adaptive foreground and graphite background. The Android foreground stays inside the standard 66dp safe zone on a 108dp layer and supports themed monochrome icons.

Never stretch, rotate, close the opening, change the counter position, add outlines, bevels, shadows or a metallic gradient to the mark. A metallic material may belong to an interface surface around it.

## Verification and limitations
Filled vector paths, real negative space, no text, images, masks or filters in logo masters. Inspect 16/32px, black, white, mirrored, rotated and app-tile contexts in the supplied review sheet. Revised small and reversed mouth edges are horizontal. Windows and Android assets use the same geometry.

The Open C artwork is archived as the earlier direction. This kit supersedes it for new app builds. Mockups illustrate placement and are not photographs or released product screenshots. No trademark clearance has been performed; a professional search is recommended before a public brand launch.
