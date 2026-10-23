// SPDX-License-Identifier: Apache-2.0
#include "graphics.h"
#include "panel-render.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace ob;
int main(int argc, char **argv) {
  NativeGraphics graphics;
  Palette palette;
  Editor editor;
  auto image = palette.image(editor.frame());
  GLuint fbo, texture;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, image.width, image.height);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  PanelRenderer renderer;
  auto projection = identity();
  projection[0] = 2 / Palette::width;
  projection[5] = 2 / Palette::height;
  renderer.draw(image, {}, projection);
  glFinish();
  Bytes output(image.width * image.height * 4);
  glReadPixels(0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE,
               output.data());
  size_t mismatches = 0;
  for (int y = 0; y < image.height; ++y)
    for (int x = 0; x < image.width; ++x) {
      auto source = (y * image.width + x) * 4,
           dest = ((image.height - 1 - y) * image.width + x) * 4;
      bool matches = true;
      for (int c = 0; c < 4; ++c) {
        int expected = c == 3
                           ? image.rgba[source + 3]
                           : int(std::lround(image.rgba[source + c] *
                                             image.rgba[source + 3] / 255.f));
        if (std::abs(expected - output[dest + c]) > 1)
          matches = false;
      }
      if (!matches)
        ++mismatches;
    }
  assert(mismatches < image.width * image.height / 100);
  size_t glyphPixels = 0;
  int unit = image.width / 640;
  for (int y = 8 * unit; y < 38 * unit; ++y)
    for (int x = 12 * unit; x < 400 * unit; ++x) {
      auto i = (y * image.width + x) * 4;
      int difference = 0;
      for (int c = 0; c < 3; ++c)
        difference += std::abs(image.rgba[i + c] - image.rgba[c]);
      if (difference > 100)
        ++glyphPixels;
    }
  assert(glyphPixels > 100);
  if (argc > 1) {
    std::ofstream f(argv[1], std::ios::binary);
    f << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (size_t i = 0; i < image.rgba.size(); i += 4)
      f.write(reinterpret_cast<const char *>(image.rgba.data() + i), 3);
  }
  glDeleteTextures(1, &texture);
  glDeleteFramebuffers(1, &fbo);
  std::cout << "Palette GPU image agrees with CPU layout/bitmap font/alpha in "
            << image.width * image.height - mismatches
            << " pixels; header glyph pixels=" << glyphPixels << '\n';
}
