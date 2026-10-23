// SPDX-License-Identifier: Apache-2.0
#include "mesh.h"
namespace ob {
Mesh strokeMesh(const Stroke &s, BrushShape shape, unsigned sides) {
  validate(s);
  if (sides < 3 || sides > 32)
    throw std::runtime_error("Invalid brush tube resolution");
  Mesh m;
  if (s.points.empty())
    return m;
  std::vector<Point> points;
  for (const auto &p : s.points)
    if (points.empty() || length(p.position - points.back().position) > 1e-6f)
      points.push_back(p);
  if (points.size() == 1) {
    auto p = points.front();
    p.position = p.position +
                 rotate(p.orientation, {0, 0, -std::max(s.size * .01f, 1e-5f)});
    points.push_back(p);
  }
  const unsigned ring = shape == BrushShape::Tube ? sides : 2;
  Vec3 previous{};
  for (size_t i = 0; i < points.size(); ++i) {
    const auto &p = points[i];
    auto tangent =
        normalized(points[i + 1 < points.size() ? i + 1 : i].position -
                       points[i ? i - 1 : i].position,
                   {0, 0, -1});
    auto right = rotate(p.orientation, {1, 0, 0});
    right = right - tangent * dot(tangent, right);
    if (length(right) < 1e-5f)
      right = cross(tangent,
                    std::abs(tangent.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0});
    right = normalized(right);
    // Parallel transport avoids arbitrary rotations as tube tangents cross
    // axes.
    if (shape == BrushShape::Tube && i) {
      auto carried = previous - tangent * dot(previous, tangent);
      if (length(carried) > 1e-5f)
        right = normalized(carried);
    }
    previous = right;
    auto up = normalized(cross(tangent, right));
    float radius = s.size * s.scale * .5f * std::clamp(p.pressure, .02f, 1.f);
    for (unsigned j = 0; j < ring; ++j) {
      Vec3 normal, offset;
      if (shape == BrushShape::Tube) {
        float angle = 6.283185307f * j / ring;
        normal = right * std::cos(angle) + up * std::sin(angle);
        offset = normal * radius;
      } else {
        normal = up;
        offset = right * (j ? radius : -radius);
        if (shape == BrushShape::Flat)
          offset = offset + up * (s.size * .04f);
      }
      m.vertices.push_back({p.position + offset, normal, s.color});
    }
    if (i)
      for (unsigned j = 0; j < (shape == BrushShape::Tube ? ring : 1); ++j) {
        uint32_t a = (i - 1) * ring + j, b = (i - 1) * ring + (j + 1) % ring,
                 c = i * ring + j, d = i * ring + (j + 1) % ring;
        m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
      }
  }
  if (shape == BrushShape::Tube) {
    for (size_t end : {size_t(0), points.size() - 1}) {
      Vec3 direction = normalized(
          end ? points.back().position - points[points.size() - 2].position
              : points[0].position - points[1].position);
      uint32_t center = m.vertices.size();
      m.vertices.push_back({points[end].position, direction, s.color});
      for (unsigned j = 0; j < ring; ++j) {
        uint32_t a = end * ring + j, b = end * ring + (j + 1) % ring;
        if (end)
          m.indices.insert(m.indices.end(), {center, a, b});
        else
          m.indices.insert(m.indices.end(), {center, b, a});
      }
    }
  }
  return m;
}
} // namespace ob
