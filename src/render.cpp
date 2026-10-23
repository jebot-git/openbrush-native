// SPDX-License-Identifier: Apache-2.0
#include "render.h"
#include <cstddef>
namespace ob {
Mat4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Mat4 multiply(const Mat4 &a, const Mat4 &b) {
  Mat4 c{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int i = 0; i < 4; ++i)
        c[col * 4 + row] += a[i * 4 + row] * b[col * 4 + i];
  return c;
}
Mat4 viewMatrix(Vec3 p, Quat q) {
  q = normalized(q);
  q = {-q.x, -q.y, -q.z, q.w};
  auto x = rotate(q, {1, 0, 0}), y = rotate(q, {0, 1, 0}),
       z = rotate(q, {0, 0, 1}), t = rotate(q, p) * -1;
  return {x.x, x.y, x.z, 0, y.x, y.y, y.z, 0,
          z.x, z.y, z.z, 0, t.x, t.y, t.z, 1};
}
Mat4 projection(float left, float right, float down, float up, float near,
                float far) {
  float l = std::tan(left), r = std::tan(right), b = std::tan(down),
        t = std::tan(up);
  return {2 / (r - l),
          0,
          0,
          0,
          0,
          2 / (t - b),
          0,
          0,
          (r + l) / (r - l),
          (t + b) / (t - b),
          -(far + near) / (far - near),
          -1,
          0,
          0,
          -2 * far * near / (far - near),
          0};
}
namespace {
GLuint compile(GLenum type, const char *source) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &source, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char error[4096];
    glGetShaderInfoLog(s, sizeof(error), nullptr, error);
    glDeleteShader(s);
    throw std::runtime_error(error);
  }
  return s;
}
} // namespace
Renderer::Renderer() {
  const char *vs = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec4 color;
uniform mat4 matrix;out vec4 tint;out vec3 n;
void main(){gl_Position=matrix*vec4(position,1);tint=color;n=normal;})";
  const char *fs = R"(#version 330 core
in vec4 tint;in vec3 n;out vec4 pixel;
void main(){float light=.45+.55*abs(dot(normalize(n),normalize(vec3(.3,.8,.6))));pixel=vec4(tint.rgb*light,tint.a);})";
  auto vert = compile(GL_VERTEX_SHADER, vs),
       frag = compile(GL_FRAGMENT_SHADER, fs);
  shader = glCreateProgram();
  glAttachShader(shader, vert);
  glAttachShader(shader, frag);
  glLinkProgram(shader);
  glDeleteShader(vert);
  glDeleteShader(frag);
  GLint ok;
  glGetProgramiv(shader, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("Brush shader link failed");
  matrix = glGetUniformLocation(shader, "matrix");
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
}
Renderer::~Renderer() {
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(shader);
}
void Renderer::draw(const Document &d, const Stroke *active, const Mat4 &vp,
                    int w, int h) {
  glViewport(0, 0, w, h);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(.035f, .045f, .06f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(shader);
  glUniformMatrix4fv(matrix, 1, GL_FALSE, vp.data());
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  for (unsigned i = 0; i < 3; ++i)
    glEnableVertexAttribArray(i);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, color)));
  auto draw = [&](const Stroke &s) {
    auto mesh = strokeMesh(s);
    if (mesh.indices.empty())
      return;
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex),
                 mesh.vertices.data(), GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(),
                 GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
  };
  for (const auto &s : d.strokes)
    draw(s);
  if (active)
    draw(*active);
  glBindVertexArray(0);
  glUseProgram(0);
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("OpenGL brush rendering failed");
}
} // namespace ob
