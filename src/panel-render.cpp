// SPDX-License-Identifier: Apache-2.0
#include "panel-render.h"
namespace ob {
PanelRenderer::PanelRenderer() {
  const char *vertex = R"(#version 330 core
layout(location=0)in vec2 position;layout(location=1)in vec2 uv;uniform mat4 matrix;out vec2 coord;
void main(){coord=uv;gl_Position=matrix*vec4(position,0,1);})";
  const char *fragment = R"(#version 330 core
in vec2 coord;uniform sampler2D pixels;out vec4 color;void main(){color=texture(pixels,coord);})";
  auto compile = [](GLenum type, const char *s) {
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &s, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      glDeleteShader(shader);
      throw std::runtime_error("Palette shader failed");
    }
    return shader;
  };
  auto vs = compile(GL_VERTEX_SHADER, vertex),
       fs = compile(GL_FRAGMENT_SHADER, fragment);
  program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("Palette program failed");
  const float w = Palette::width / 2, h = Palette::height / 2;
  float vertices[] = {-w, -h, 0, 1, w, -h, 1, 1, -w, h, 0, 0,
                      -w, h,  0, 0, w, -h, 1, 1, w,  h, 1, 0};
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void *>(2 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  glGenTextures(1, &texture);
}
PanelRenderer::~PanelRenderer() {
  glDeleteTextures(1, &texture);
  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(program);
}
void PanelRenderer::draw(const PaletteImage &image, Transform pose,
                         const Mat4 &vp) {
  glUseProgram(program);
  auto matrix = multiply(vp, modelMatrix(pose));
  glUniformMatrix4fv(glGetUniformLocation(program, "matrix"), 1, GL_FALSE,
                     matrix.data());
  glUniform1i(glGetUniformLocation(program, "pixels"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  if (image.revision != revision) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (width != image.width || height != image.height) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
      width = image.width;
      height = image.height;
    } else
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width, image.height,
                      GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    revision = image.revision;
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_CULL_FACE);
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("Palette GPU rendering failed");
}
} // namespace ob
