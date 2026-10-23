// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "navigation.h"
#include "palette.h"
namespace ob {
struct HapticPulse {
  int hand;
  float amplitude;
  int milliseconds;
};
struct SessionControls {
  bool focused = true, palette = false, undo = false, redo = false;
  bool touch = false, eraser = false, mirror = false, recenter = false;
  float leftX = 0, leftY = 0, rightX = 0, rightY = 0, seconds = 1.f / 90;
};
class Interaction {
  Editor &editor;
  std::function<void()> changed, save;
  Navigation navigation;
  Palette palette;
  bool suppressRight = true, suppressLeft = true, moveRightCaptured = false;
  std::array<TrackedHand, 2> hands{};
  struct StickRepeat {
    int direction = 0;
    float remaining = 0;
    bool blocked = false;
    int step(float x, float y, float seconds);
    void reset() {
      direction = 0;
      remaining = 0;
    }
    void block() {
      reset();
      blocked = true;
    }
  };
  StickRepeat brushRepeat, colorRepeat;
  std::vector<HapticPulse> haptics;
  Tool previousTool = Tool::Paint;
  void shortcuts(const SessionControls &controls, bool right);
  void pulse(int hand, float amplitude = .18f, int milliseconds = 12) {
    haptics.push_back({hand, amplitude, milliseconds});
  }

public:
  Interaction(Editor &editor, std::function<void()> changed,
              std::function<void()> save);
  void update(const std::array<TrackedHand, 2> &hands, Transform head,
              SessionControls controls);
  void finish();
  Palette &panel() { return palette; }
  std::vector<Stroke> visuals() const;
  std::vector<HapticPulse> takeHaptics() {
    auto result = std::move(haptics);
    haptics.clear();
    return result;
  }
};
} // namespace ob
