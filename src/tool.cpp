// SPDX-License-Identifier: Apache-2.0
#include "document.h"
#include "mesh.h"
#include <iostream>
int main(int argc, char **argv) try {
  if (argc < 2) {
    std::cerr << "Usage: openbrush-tool SKETCH.tilt [ROUNDTRIP.tilt]\n";
    return 2;
  }
  auto d = ob::readTilt(argv[1]);
  size_t points = 0, triangles = 0;
  for (const auto &s : d.strokes) {
    points += s.points.size();
    triangles += ob::strokeMesh(s).indices.size() / 3;
  }
  std::cout << ob::Json{{"strokes", d.strokes.size()},
                        {"points", points},
                        {"triangles", triangles},
                        {"warnings", d.warnings}}
                   .dump(2)
            << '\n';
  if (argc > 2)
    ob::writeTilt(d, argv[2]);
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
