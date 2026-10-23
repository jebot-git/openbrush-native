// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "editor.h"
#include <array>
#include <functional>
#include <future>
namespace ob {
struct PaletteGlyph {
  int width = 0, height = 0, left = 0, top = 0, advance = 8;
  Bytes alpha;
};
struct PaletteTheme {
  Color background{.12f, .15f, .18f, .9f}, foreground{.95f, .95f, .95f, 1},
      button{.2f, .23f, .27f, 1}, accent{.42f, .67f, 1, 1};
  int pixels = 16, ascent = 14;
  std::string fontFile;
  std::map<uint32_t, PaletteGlyph> glyphs;
};
PaletteTheme paletteTheme();
const std::array<Color, 8> &presetColors();
size_t closestPreset(Color color);
struct PaletteImage {
  int width = 640, height = 710;
  uint64_t revision = 0;
  Bytes rgba;
};
struct PaletteButton {
  int x, y, w, h;
  std::string label, command;
  bool active = false, enabled = true;
  Color color{};
  bool swatch = false;
};
struct PaletteHit {
  float distance;
  Vec3 roomPoint;
  int button = -1;
};
class Palette {
  bool shown = true, anchored = false, latched = false, lastDown = false;
  Transform pose, viewer;
  std::vector<PaletteButton> buttons;
  PaletteTheme theme;
  PaletteImage pixels;
  std::string signature;
  int hover = -1;
  uint64_t clicks = 0;
  int armedButton = -1;
  bool armed = false, clicked = false;
  std::future<PaletteTheme> reload;
  std::filesystem::file_time_type styleTime{};
  std::chrono::steady_clock::time_point nextStyle{};
  void raster(const EditorFrame &frame);

public:
  static constexpr float width = .48f, height = .5325f;
  Palette();
  void setViewer(Transform head) { viewer = head; }
  void place(Transform head, const std::optional<Transform> &left = {});
  void ensurePlaced(Transform head, const std::optional<Transform> &left = {}) {
    if (!anchored)
      place(head, left);
  }
  void showAt(Transform head, const std::optional<Transform> &left = {}) {
    shown = true;
    place(head, left);
    cancel();
  }
  void toggle(Transform head, const std::optional<Transform> &left = {});
  bool visible() const { return anchored; }
  bool expanded() const { return shown && anchored; }
  void cancel() {
    latched = false;
    lastDown = true;
    hover = -1;
    armed = false;
    clicked = false;
  }
  const Transform &transform() const { return pose; }
  const PaletteImage &image(const EditorFrame &frame);
  const std::vector<PaletteButton> &layout(const EditorFrame &frame);
  std::optional<PaletteHit> hit(Vec3 origin, Vec3 direction) const;
  // Returns true while the trigger belongs to the panel, including after it
  // leaves the panel, so one click cannot turn into an accidental paint stroke.
  bool point(Vec3 origin, Vec3 direction, bool down, bool valid, Editor &editor,
             const std::function<void()> &save, float pressure = 1);
  bool activate(const std::string &command, Editor &editor,
                const std::function<void()> &save);
  int hovered() const { return hover; }
  uint64_t clickCount() const { return clicks; }
};
} // namespace ob
