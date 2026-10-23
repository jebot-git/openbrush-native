# OpenBrush Native for de-xr

Work in progress toward a native C++ OpenXR painting application based on the
Open Brush artwork format. No Unity, Godot, Unreal, external game engine, cloud
services or multiplayer components are built or linked. This is an early native
implementation, not a claim of Open Brush feature or visual parity.

The current executable paints pressure-sensitive 3D tube strokes, erases whole
strokes by proximity, supports stroke undo/redo, and saves editable `.tilt`
artworks atomically. OpenXR uses stereo projection layers, tracked controller
poses and actions, a local reference space, GPU OpenGL rendering, and a
windowless EGL context. Default overlay mode requires `XR_EXTX_overlay` and
coexists with the running compositor. `--exclusive` explicitly permits a primary
XR session. The Linux EGL binding currently requires `XR_MNDX_egl_enable` in
addition to `XR_KHR_opengl_enable`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
build/openbrush-native --probe
build/openbrush-native --demo --render build/demo.ppm
build/openbrush-native --open artwork.tilt --save edited.tilt
```

Build dependencies: C++17, CMake, Ninja, OpenXR loader/development headers,
EGL/OpenGL development libraries, libarchive, nlohmann JSON headers. Tests use
Python 3's standard library. No graphical toolkit is required for this initial
XR executable. A native FLTK frontend conforming to DomeD is still outstanding;
there is currently no desktop editor or headset tool palette.

Right trigger draws, left trigger erases. Right A/B undo/redo on Touch controllers.
Vive, Index and simple-controller pose/trigger bindings are also suggested, but
have not been verified on those devices. Without `--save`, edits autosave to
`$XDG_STATE_HOME/openbrush-native/recovery.tilt` (normally under `~/.local/state`),
which is restored at startup when no input file is supplied. Input artwork is
never silently replaced unless explicitly selected as the save destination.
`--frames N` exits after N frames for runtime checks. `--demo` starts with three
colored sample strokes.

The `.tilt` reader supports packed containers and loose metadata/sketch files,
sketch versions 5 and 6, coordinate/size conversion from Unity's left-handed
decimeter space to OpenXR's right-handed meter space, pressure/timestamps,
brush GUIDs, stroke flags, scale, group and layer IDs. Packed-file attachments,
metadata fields and unknown binary extensions are retained on save. Unknown
brush GUIDs are retained; the initial renderer renders all strokes as tubes.
Per-point color extensions are retained but not yet rendered. Referenced media,
scene/layer transforms, environment settings and imported model/image content
are not yet implemented in the native renderer. Their metadata is preserved.
Saving modifications from this prototype should use a new filename.

The geometry core also implements ribbon/flat meshes. Brush selection,
material/texture fidelity, configurable tools, layers, selection/transforms,
spatial palette interaction, FLTK dialogs/universal menu/appearance integration,
additional export formats and packaging remain incomplete. Multiplayer and
cloud features are outside this project.

Tests cover a real upstream one-stroke fixture, an independent Python `.tilt`
reader, round-trip extension/metadata/attachment preservation, malformed data,
atomic-save behavior, history, geometry and a nonblank colored native GL render.
A short live WiVRn check on 2026-10-23 submitted 14 stereo frames in 15 OpenXR
frames. This establishes the runtime path, not physical controller drawing,
complete workflow coverage or brush parity. The prototype is not installed as a
system application yet.

## Provenance and licensing

Open Brush reference revision and format/fixture provenance are recorded in
`third_party/upstream.json`. `third_party/brush-catalog.json` records 83 upstream
brush descriptors from the checked-out Resources/Brushes tree; this is reference
data, not a claim that all those brushes are implemented. The reference checkout
is ignored and is not a build dependency. Open Brush-derived format information,
reference catalog and test fixture are Apache-2.0; see `LICENSE` and `NOTICE`.
The native EGL bootstrap is reused under MIT from Ghostty XR via FLTerm XR; its
notice is in `third_party/LICENSE.native-graphics`.
