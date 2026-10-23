// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "palette.h"
#include "render.h"
namespace ob {
class PanelRenderer {
  GLuint program = 0, vao = 0, vbo = 0, texture = 0;
  uint64_t revision = 0;
  int width = 0, height = 0;

public:
  PanelRenderer();
  ~PanelRenderer();
  void draw(const PaletteImage &image, Transform pose,
            const Mat4 &viewProjection);
};
} // namespace ob
