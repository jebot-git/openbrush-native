// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "graphics.h"
#include "render.h"
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
  XrAction grip{}, trigger{}, undo{}, redo{};
  XrPath hands[2]{};
  XrSpace handSpaces[2]{};
  std::vector<Eye> eyes;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = false, quit = false;
  Document &document;
  History &history;
  Renderer renderer;
  Stroke active;
  bool drawing = false, erasing = false;
  XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  std::function<void()> changed;
  void setup(bool overlay);
  void events();
  void input(XrTime time);
  void finishStroke();
  void cleanup();

public:
  XrPainter(Document &d, History &h, std::function<void()> save,
            bool overlay = true);
  ~XrPainter();
  void run(unsigned frameLimit = 0);
  static void probe();
};
} // namespace ob
