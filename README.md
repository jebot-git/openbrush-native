# OpenBrush Native for de-xr

A native C++ OpenXR painting implementation based on Open Brush's editable
artwork format. The application uses OpenGL and FLTK directly. No Unity, Godot,
Unreal or other external game engine is built or linked. Multiplayer and cloud
functionality are outside scope. This remains an early implementation rather
than a claim of full Open Brush feature or visual parity.

## Build and run

Dependencies: C++17, CMake, Ninja, OpenXR loader/development headers, EGL/OpenGL,
libarchive, nlohmann JSON headers, Fontconfig, GIO, Wayland development headers,
`wayland-scanner`, and the de-xr patched FLTK 1.4.5. Tests also use Python 3's
standard library; the optional desktop interaction test needs Xvfb.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFLTK_DIR=/opt/fltk-vr/toolkit/share/fltk
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
build/openbrush-native --demo
build/openbrush-native artwork.tilt
build/openbrush-native --probe
build/openbrush-native --xr --open artwork.tilt --save edited.tilt
```

The default FLTK frontend explicitly selects Wayland and does not fall back to
X11. X11 is available only with `-DOPENBRUSH_ENABLE_X11=ON` and an explicit
`--x11` launch. `--version` and `--probe` do not create a desktop window.
`--demo --render preview.ppm` renders an offscreen native GL diagnostic without
starting XR or opening a display. There is no system installation target yet.

## FLTK editor and shared drawing state

The desktop window provides a GPU viewport, tools, brush selection, native color
chooser, size/opacity controls, mirrored painting, layers, visibility, selection,
duplication, recoloring and selection transforms. File dialogs open and save
editable `.tilt` artworks asynchronously. Native FLTK widgets follow DomeD's
bitmap font and appearance changes. Real commands are exported to the universal
menu; the local bar collapses only while a menu host is available and restores
on host loss. Its hidden strip is reclaimed and keyboard shortcuts remain live.

Left drag applies the selected tool on the current view plane. Select and
Recolor pick strokes along the view ray. Right drag orbits; middle/Shift drag
pans; the mouse wheel zooms in the standard desktop direction. Selection is
shown with a yellow spatial bounding box. `F` fits the drawing, `R` resets the
view, Ctrl-Z/Ctrl-Shift-Z undo/redo, Ctrl-A selects all, Ctrl-D duplicates,
Delete removes selected strokes, and Ctrl-S saves. Mirror pairs and compound
selection edits undo as single commands.

**Start XR painting** (F6) starts a native XR worker using the same document and
tool settings as the FLTK editor. Palette changes apply while XR is running.
The model uses synchronized edits and immutable scene snapshots; neither
rendering thread holds the model lock while submitting GPU work. Completed
stroke meshes stay in GPU buffers until the document changes. Active strokes
and selection bounds update separately. Layers hidden in the editor are also
hidden in XR; hidden content remains in saved artworks.

Unsaved drawings are copied to a private per-process recovery file every five
seconds, including an in-progress stroke. A subsequent desktop launch offers
drawings from dead processes for recovery. Recovery opens as unsaved artwork
and requires a real save destination. Cancel/save/discard prompts protect
changes when opening, creating or closing drawings. Runtime data is under
`$XDG_STATE_HOME/openbrush-native` (normally `~/.local/state/openbrush-native`).

## Native XR path

The XR renderer uses real stereo projection layers, controller poses/actions,
a local reference space, GPU OpenGL rendering and a windowless EGL context.
Default overlay mode requires `XR_EXTX_overlay`, blends transparently behind
the artwork, and coexists with the running compositor. `--exclusive` explicitly
allows a primary session. The current Linux graphics path additionally requires
`XR_KHR_opengl_enable` and `XR_MNDX_egl_enable`.

Right trigger uses the selected tool, left trigger erases, and right A/B undo
and redo on Touch controllers. Vive, Index and simple-controller pose/trigger
bindings are also suggested, but have not been verified on those devices.
Stopping XR or changing a drawing stops only this application's XR worker;
there is no compositor, display-manager or WiVRn restart.

`--xr` runs without the desktop editor. Without `--save`, completed edits save
to `recovery.tilt` in the state directory, restored on the next standalone
launch with no input file. Input artwork is never silently replaced unless
explicitly selected as the save destination. `--frames N` bounds a runtime test.

A headset-native palette and canvas grab/navigation controls remain outstanding.
The FLTK editor currently supplies brush/color/size/layer settings in XR.
Physical controller painting and the complete creative workflow still require
verification; short runtime tests establish stereo submission, not that coverage.

## Artwork and brush coverage

The `.tilt` reader supports packed containers and loose metadata/sketch files,
sketch versions 5/6, Unity left-handed decimeter to OpenXR right-handed meter
conversion, pressure/timestamps, brush GUIDs, stroke flags, scale, group and
layer IDs. Packed-file attachments, metadata fields and unknown binary
extensions are retained on save. Writes use a temporary file, sync and atomic
replacement. Use new output filenames when editing artwork with this prototype.

Eight native brush styles currently have distinct geometry/material behavior:
Wire, Flat, Marker, Ink, Light, Dots, Stars and Highlighter. These are native
approximations, not shader/texture parity with Unity Open Brush. Light is
unshaded; stamp brushes produce oriented dots/stars; Highlighter uses opacity.
Per-point replacement/multiply/add color extensions are interpreted. Unknown
brush GUIDs are preserved and rendered using the wire fallback. Referenced
models/images/videos, scene/layer transforms, environments and original brush
materials/textures are not yet rendered; their metadata is retained. Additional
export formats are also outstanding.

## Verification and provenance

Default CTest covers the real upstream `.tilt` fixture, an independent Python
reader, extension/metadata/attachment preservation, malformed input, atomic
save behavior, history, native colored GL output, editor transactions/layers,
concurrent snapshots and Wayland backend policy. Build with the optional X11
backend to include a private-display/private-bus FLTK interaction test covering
pointer alignment, editing, menu host gain/loss, asynchronous file operations
and recovery. A native Wayland capture has also been inspected under DomeD.
A short combined FLTK/WiVRn run on 2026-10-23 submitted 14 stereo views in 15
frames without restarting the user's session.

`third_party/upstream.json` records the Open Brush revision, format sources and
fixture provenance. Its 83-entry reference brush catalog is not a list of
implemented brushes. The ignored upstream reference checkout is not a build or
installation dependency. `third_party/ui-adapters.json` records the separately
MIT-licensed de-xr appearance, bitmap-font, GPU-widget and menu adapters.
Open Brush-derived material and native code use Apache-2.0 (`LICENSE`, `NOTICE`).
The native EGL bootstrap is MIT-licensed from Ghostty XR via FLTerm XR; its
notice is in `third_party/LICENSE.native-graphics`.
