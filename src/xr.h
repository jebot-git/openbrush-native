// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controller-profiles.h"
#include "editor.h"
#include "graphics.h"
#include "interaction.h"
#include "panel-render.h"
#include "render.h"
#include <atomic>
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#include <functional>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
namespace ob {
class XrPainter {
  struct Eye {
    XrSwapchain swapchain{};
    int width = 0, height = 0;
    GLuint fbo = 0, depth = 0;
    std::vector<XrSwapchainImageOpenGLKHR> images;
  };
  NativeGraphics graphics;
  XrInstance instance{};
  XrSystemId system{};
  XrSession session{};
  XrSpace reference{};
  XrActionSet actionSet{};
  XrAction grip{}, aim{}, trigger{}, squeeze{}, menuAction{}, undo{}, redo{},
      eraserAction{}, leftStick{}, rightStick{}, mirrorAction{},
      recenterAction{}, haptic{};
  XrPath hands[2]{};
  XrSpace handSpaces[2]{}, aimSpaces[2]{}, headSpace{};
  std::vector<Eye> eyes;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = false, quit = false, overlayMode = true;
  Editor &editor;
  std::atomic_bool *stop = nullptr;
  Renderer renderer;
  Interaction interaction;
  PanelRenderer panelRenderer;
  uint64_t trackedFrames[2]{};
  bool touchHands[2]{};
  XrTime lastInputTime = 0;

  XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  void setup(bool overlay);
  void events();
  void input(XrTime time);
  void finishStroke();
  void cleanup();

public:
  XrPainter(Editor &editor, std::function<void()> save, bool overlay = true,
            std::atomic_bool *stop = nullptr,
            std::function<void()> saveRequested = {});
  ~XrPainter();
  void run(unsigned frameLimit = 0);
  static void probe();
};
} // namespace ob
