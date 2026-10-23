// SPDX-License-Identifier: Apache-2.0
#include "xr.h"
#include <fstream>
#include <iostream>
namespace {
ob::Document demo() {
  ob::Document d;
  for (int j = 0; j < 3; ++j) {
    ob::Stroke s;
    s.size = .018f;
    s.color = {j == 0 ? 1.f : .1f, j == 1 ? 1.f : .2f, j == 2 ? 1.f : .25f, 1};
    for (int i = 0; i < 150; ++i) {
      float t = i * .05f;
      s.points.push_back({{std::cos(t + j * 2.1f) * .22f, 1.1f + i * .003f,
                           std::sin(t + j * 2.1f) * .22f - 1.2f},
                          {},
                          .6f + .4f * std::sin(t) * std::sin(t),
                          uint32_t(i * 10)});
    }
    d.strokes.push_back(s);
  }
  return d;
}
void renderImage(const ob::Document &d, const std::string &file) {
  NativeGraphics graphics;
  ob::Renderer renderer;
  GLuint fbo, texture, depth;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 640, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 640, 640);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Offscreen framebuffer failed");
  renderer.draw(d, nullptr,
                ob::multiply(ob::projection(-.6f, .6f, -.6f, .6f),
                             ob::viewMatrix({0, 1.3f, 0}, {})),
                640, 640);
  std::vector<unsigned char> pixels(640 * 640 * 3);
  glReadPixels(0, 0, 640, 640, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  std::ofstream out(file, std::ios::binary);
  out << "P6\n640 640\n255\n";
  for (int y = 639; y >= 0; --y)
    out.write(reinterpret_cast<const char *>(pixels.data() + y * 640 * 3),
              640 * 3);
  if (!out)
    throw std::runtime_error("Cannot write render capture");
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &texture);
  glDeleteRenderbuffers(1, &depth);
}
} // namespace
int main(int argc, char **argv) try {
  std::string input, output, capture;
  unsigned frames = 0;
  bool example = false, overlay = true;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--probe") {
      ob::XrPainter::probe();
      return 0;
    } else if (arg == "--version") {
      std::cout
          << "OpenBrush Native 0.1.0 (C++/OpenXR/OpenGL; no external engine)\n";
      return 0;
    } else if (arg == "--demo")
      example = true;
    else if (arg == "--exclusive")
      overlay = false;
    else if (arg == "--open" && i + 1 < argc)
      input = argv[++i];
    else if (arg == "--save" && i + 1 < argc)
      output = argv[++i];
    else if (arg == "--render" && i + 1 < argc)
      capture = argv[++i];
    else if (arg == "--frames" && i + 1 < argc)
      frames = std::stoul(argv[++i]);
    else if (arg == "--help") {
      std::cout
          << "openbrush-native [--open artwork.tilt] [--save artwork.tilt] "
             "[--demo]\n  --probe          Inspect runtime without starting an "
             "XR session\n  --render out.ppm Render a diagnostic image without "
             "an XR runtime\n  --frames N       Exit after N XR frames\n  "
             "--exclusive      Explicitly allow a primary session without "
             "overlay support\nRight trigger: paint; left trigger: erase; "
             "right A/B: undo/redo.\n";
      return 0;
    } else
      throw std::runtime_error("Unknown or incomplete option: " + arg);
  }
  ob::Document document = example ? demo() : ob::Document{};
  if (!input.empty())
    document = ob::readTilt(input);
  if (!capture.empty()) {
    renderImage(document, capture);
    return 0;
  }
  if (output.empty()) {
    const char *state = getenv("XDG_STATE_HOME");
    auto dir = (state ? std::filesystem::path(state)
                      : std::filesystem::path(getenv("HOME") ? getenv("HOME")
                                                             : "/tmp") /
                            ".local/state") /
               "openbrush-native";
    std::filesystem::create_directories(dir);
    output = (dir / "recovery.tilt").string();
    if (input.empty() && !example && std::filesystem::exists(output))
      document = ob::readTilt(output);
  }
  ob::History history(document);
  auto save = [&] {
    ob::writeTilt(document, output);
    history.markSaved();
  };
  ob::XrPainter painter(document, history, save, overlay);
  painter.run(frames);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "OpenBrush Native: " << e.what() << '\n';
  return 1;
}
