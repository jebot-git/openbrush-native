// SPDX-License-Identifier: Apache-2.0
#include "controller-profiles.h"
#include "interaction.h"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace ob;
int main() {
  auto base = controllerProfiles({});
  assert(base.size() == 4 && base.front().touch);
  auto extended = controllerProfiles(
      {"XR_FB_touch_controller_pro", "XR_META_touch_controller_plus"});
  assert(extended.size() == 6 && extended[1].touch && extended[2].touch);
  for (auto &profile : extended) {
    auto bindings = controllerBindings(profile);
    auto binding = [&](std::string action, std::string path) {
      return std::any_of(bindings.begin(), bindings.end(), [&](const auto &b) {
        return b.action == action && b.path == path;
      });
    };
    assert(binding("haptic", "/user/hand/right/output/haptic"));
    for (auto &b : bindings)
      assert(b.path.find("system") == std::string::npos);
    if (profile.touch) {
      assert(binding("menu", "/user/hand/left/input/x/click"));
      assert(binding("eraser", "/user/hand/left/input/y/click"));
      assert(binding("undo", "/user/hand/right/input/a/click"));
      assert(binding("redo", "/user/hand/right/input/b/click"));
      assert(binding("left_stick", "/user/hand/left/input/thumbstick"));
      assert(binding("right_stick", "/user/hand/right/input/thumbstick"));
    }
    if (profile.path.find("simple") != std::string::npos) {
      assert(binding("trigger", "/user/hand/right/input/select/click"));
      for (auto &b : bindings)
        assert(b.action != "squeeze");
    }
  }
  Editor editor;
  Interaction input(editor, [] {}, [] {});
  std::array<TrackedHand, 2> hands{};
  for (auto &hand : hands) {
    hand.valid = true;
    hand.grip.position = {.7f, 1, -1};
  }
  Transform head{{0, 1.6f, 0}, {}, 1};
  SessionControls c;
  c.touch = true;
  c.seconds = .05f;
  input.update(hands, head, c);
  auto brush = editor.tools().brush;
  c.leftX = .3f;
  input.update(hands, head, c);
  assert(editor.tools().brush == brush);
  c.leftX = 1;
  input.update(hands, head, c);
  auto next = editor.tools().brush;
  assert(next != brush);
  assert(input.takeHaptics().size() == 1);
  for (int i = 0; i < 4; ++i)
    input.update(hands, head, c);
  assert(editor.tools().brush == next); // initial repeat delay
  for (int i = 0; i < 6; ++i)
    input.update(hands, head, c);
  assert(editor.tools().brush != next);
  c.leftX = 0;
  input.update(hands, head, c);
  auto size = editor.tools().size;
  c.leftY = 1;
  for (int i = 0; i < 20; ++i)
    input.update(hands, head, c);
  assert(std::abs(editor.tools().size - size * 2) < .00001f);
  c.leftY = 0;
  c.rightY = -1;
  auto alpha = editor.tools().color.a;
  for (int i = 0; i < 10; ++i)
    input.update(hands, head, c);
  assert(editor.tools().color.a < alpha);
  c.rightY = 0;
  c.rightX = 1;
  input.update(hands, head, c);
  assert(std::abs(editor.tools().color.a - (alpha - .25f)) < .00001f);
  auto color = editor.tools().color;
  auto currentBrush = editor.tools().brush;
  // Sticks held during a stroke must return to neutral before changing
  // settings.
  hands[1].trigger = 1;
  c.leftX = 1;
  input.update(hands, head, c);
  hands[1].grip.position.x += .1f;
  input.update(hands, head, c);
  hands[1].trigger = 0;
  for (int i = 0; i < 15; ++i)
    input.update(hands, head, c);
  assert(editor.tools().brush == currentBrush);
  assert(closestPreset(editor.tools().color) == closestPreset(color));
  assert(editor.copy().strokes.size() == 1);
  c.leftX = c.rightX = 0;
  input.update(hands, head, c);
  c.undo = true;
  input.update(hands, head, c);
  assert(editor.copy().strokes.empty());
  c.undo = false;
  c.redo = true;
  input.update(hands, head, c);
  assert(editor.copy().strokes.size() == 1);
  c.redo = false;
  c.eraser = true;
  input.update(hands, head, c);
  assert(editor.tools().tool == Tool::Erase);
  c.eraser = false;
  input.update(hands, head, c);
  c.eraser = true;
  input.update(hands, head, c);
  assert(editor.tools().tool == Tool::Paint);
  c.eraser = false;
  c.mirror = true;
  input.update(hands, head, c);
  assert(editor.tools().mirror);
  c.mirror = false;
  c.recenter = true;
  input.update(hands, head, c);
  assert(input.panel().expanded());
  c.recenter = false;
  c.palette = true;
  input.update(hands, head, c);
  assert(!input.panel().expanded());
  c.palette = false;
  c.recenter = true;
  input.update(hands, head, c);
  assert(input.panel().expanded());
  // A light touch captures the panel gesture, but activates only at a firm
  // pull.
  auto &panel = input.panel();
  auto frame = editor.frame();
  auto image = panel.image(frame);
  auto buttons = panel.layout(frame);
  auto button = *std::find_if(buttons.begin(), buttons.end(),
                              [](auto &b) { return b.command == "mirror"; });
  Vec3 target = transform(
      panel.transform(),
      {((button.x + button.w * .5f) / image.width - .5f) * Palette::width,
       (.5f - (button.y + button.h * .5f) / image.height) * Palette::height,
       0});
  auto normal = rotate(panel.transform().orientation, {0, 0, 1});
  auto origin = target + normal * .4f;
  panel.point(origin, normal * -1, false, true, editor, [] {}, 0);
  auto mirror = editor.tools().mirror;
  assert(panel.point(origin, normal * -1, true, true, editor, [] {}, .1f));
  assert(editor.tools().mirror == mirror);
  panel.point(origin, normal * -1, true, true, editor, [] {}, .7f);
  assert(editor.tools().mirror != mirror);
  panel.point(origin, normal * -1, true, true, editor, [] {}, 1);
  assert(editor.tools().mirror != mirror);
  std::cout
      << "Touch/Quest bindings, extension gating, shortcuts, repeat/deadzones, "
         "stroke guards, haptics and firm palette activation passed\n";
}
