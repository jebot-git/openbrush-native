// SPDX-License-Identifier: Apache-2.0
#include "document.h"
#include "mesh.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <unistd.h>
using namespace ob;
template <class F> void rejects(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  auto upstream = readTilt(argv[1]);
  assert(upstream.strokes.size() == 1);
  assert(!upstream.strokes[0].points.empty());
  auto metadata = upstream.metadata;
  auto bytes = encodeSketch(upstream, metadata);
  auto decoded = decodeSketch(bytes, metadata);
  assert(decoded.strokes[0].brush == upstream.strokes[0].brush);
  assert(decoded.strokes[0].points.size() == upstream.strokes[0].points.size());
  for (size_t i = 0; i < upstream.strokes[0].points.size(); ++i)
    assert(length(upstream.strokes[0].points[i].position -
                  decoded.strokes[0].points[i].position) < 1e-5f);
  Document d;
  History history(d);
  Stroke stroke;
  stroke.points = {{{0, 1, -1}, {}, .5f, 10},
                   {{.1f, 1, -1}, {}, 1, 20},
                   {{.2f, 1, -1}, {}, .8f, 30}};
  history.append(stroke);
  assert(history.dirty());
  history.markSaved();
  assert(!history.dirty());
  assert(history.eraseNear({.05f, 1, -1}, .01f));
  assert(d.strokes.empty());
  assert(history.undo());
  assert(!history.dirty());
  assert(history.undo());
  assert(history.dirty());
  history.append(stroke);
  assert(history.dirty());
  assert(!history.redo());
  for (auto shape : {BrushShape::Tube, BrushShape::Ribbon, BrushShape::Flat}) {
    auto mesh = strokeMesh(stroke, shape);
    assert(!mesh.vertices.empty());
    assert(!mesh.indices.empty());
    for (auto i : mesh.indices)
      assert(i < mesh.vertices.size());
    for (const auto &v : mesh.vertices)
      assert(finite(v.position) && finite(v.normal) &&
             std::abs(length(v.normal) - 1) < 1e-4f);
  }
  auto repeated = stroke;
  repeated.points[1].position = repeated.points[0].position;
  assert(!strokeMesh(repeated).indices.empty());
  stroke.extensions[8] = {1, 2, 3, 4};
  stroke.extensions[20] = {4, 3, 2, 1, 0};
  stroke.points[0].extensions[7] = 0x87654321;
  d.strokes = {stroke};
  d.sketchHeader = {10, 20, 30};
  d.metadata["NativeTestUnknown"] = {{"keep", true}};
  bytes = encodeSketch(d, metadata);
  decoded = decodeSketch(bytes, metadata);
  assert(decoded.strokes[0].extensions == stroke.extensions);
  assert(decoded.strokes[0].points[0].extensions ==
         stroke.points[0].extensions);
  assert(decoded.sketchHeader == d.sketchHeader);
  assert(decoded.metadata["NativeTestUnknown"]["keep"] == true);
  auto truncated = bytes;
  truncated.pop_back();
  rejects([&] { decodeSketch(truncated, metadata); });
  auto corrupt = bytes;
  corrupt[0] = 0;
  rejects([&] { decodeSketch(corrupt, metadata); });
  auto invalid = stroke;
  invalid.points[0].position.x = NAN;
  rejects([&] { validate(invalid); });
  d.attachments["thumbnail.png"] = {1, 2, 3};
  d.attachments["unknown.bin"] = {9, 8, 7};
  char dir[] = "/tmp/openbrush-core-XXXXXX";
  assert(mkdtemp(dir));
  auto path = std::filesystem::path(dir) / "roundtrip.tilt";
  writeTilt(d, path);
  auto roundtrip = readTilt(path);
  assert(roundtrip.attachments == d.attachments);
  assert(roundtrip.metadata["NativeTestUnknown"]["keep"] == true);
  assert(roundtrip.strokes[0].extensions == stroke.extensions);
  d.attachments["../bad"] = {0};
  rejects([&] { writeTilt(d, path); });
  assert(readTilt(path).strokes.size() == 1);
  auto original = upstream.strokes[0].points[0].position;
  writeTilt(upstream, path);
  roundtrip = readTilt(path);
  assert(length(roundtrip.strokes[0].points[0].position - original) < 1e-5f);
  std::filesystem::remove_all(dir);
  std::cout << "Real upstream .tilt import, lossless extensions, "
               "metadata/attachments, units, geometry, history and atomic-save "
               "checks passed\n";
}
