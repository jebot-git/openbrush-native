// SPDX-License-Identifier: Apache-2.0
#pragma once
#define GL_GLEXT_PROTOTYPES
#include "mesh.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <array>
namespace ob {
using Mat4 = std::array<float, 16>;
Mat4 identity();
Mat4 multiply(const Mat4 &a, const Mat4 &b);
Mat4 viewMatrix(Vec3 position, Quat orientation);
Mat4 projection(float left, float right, float down, float up,
                float near = .03f, float far = 100.f);
class Renderer {
  struct Batch {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei count = 0;
  };
  GLuint shader = 0;
  Batch scene, live;
  uint64_t cachedRevision = 0;
  void upload(Batch &batch, const Mesh &mesh);
  void render(const Batch &batch);
  GLint matrix = -1;

public:
  Renderer();
  ~Renderer();
  void abandon() {
    shader = 0;
    scene = {};
    live = {};
    cachedRevision = 0;
  }
  void draw(const Document &d, const std::vector<Stroke> *active,
            const Mat4 &viewProjection, int width, int height,
            uint64_t revision = 0, Color background = {.035f, .045f, .06f, 1});
};
} // namespace ob
