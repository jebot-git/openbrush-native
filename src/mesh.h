// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "document.h"
namespace ob {
enum class BrushShape { Tube, Ribbon, Flat };
struct Vertex {
  Vec3 position, normal;
  Color color;
};
struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};
Mesh strokeMesh(const Stroke &stroke, BrushShape shape = BrushShape::Tube,
                unsigned sides = 8);
} // namespace ob
