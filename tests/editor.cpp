// SPDX-License-Identifier: Apache-2.0
#include "editor.h"
#include "mesh.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace ob;
int main() {
  Editor editor;
  auto settings = editor.tools();
  settings.mirror = true;
  settings.layer = 3;
  editor.configure(settings);
  editor.sample(1, {{.1f, 1, -1}, {}, .5f, 10});
  editor.sample(1, {{.2f, 1, -1}, {}, 1, 20});
  assert(!editor.sample(2, {{.5f, 1, -1}, {}, 1, 30}));
  assert(editor.frame().active.size() == 2);
  assert(editor.copy().strokes.empty());
  assert(editor.copy(true).strokes.size() == 2);
  editor.finish(2);
  assert(editor.copy().strokes.empty());
  editor.finish(1);
  auto data = editor.copy();
  assert(data.strokes.size() == 2);
  assert(data.strokes[0].layer == 3);
  assert(data.strokes[1].points[0].position.x == -.1f);
  assert(editor.undo());
  assert(editor.copy().strokes.empty());
  assert(editor.redo());
  assert(editor.copy().strokes.size() == 2);
  auto frame = editor.frame();
  editor.saved(frame.revision);
  assert(!editor.frame().dirty);
  assert(editor.selectRay({.15f, 1, 0}, {0, 0, -1}, .01f));
  assert(editor.frame().selected.size() == 1);
  editor.selectAll();
  editor.transformSelection({0, .2f, 0}, {}, 2);
  assert(editor.frame().dirty);
  assert(editor.copy().strokes[0].size == settings.size * 2);
  assert(editor.undo());
  assert(!editor.frame().dirty);
  editor.selectAll();
  editor.duplicateSelection();
  assert(editor.copy().strokes.size() == 4);
  assert(editor.undo());
  assert(editor.copy().strokes.size() == 2);
  editor.selectAll();
  editor.recolorSelection({1, 0, 0, 1});
  assert(editor.copy().strokes[0].color.r == 1);
  assert(editor.undo());
  editor.selectAll();
  editor.layerVisible(3, false);
  assert(editor.frame().selected.empty());
  editor.deleteSelection();
  assert(editor.frame().document->strokes.empty());
  assert(editor.copy().strokes.size() == 2);
  assert(!editor.sample(1, {{.3f, 1, -1}, {}, 1, 40}));
  editor.layerVisible(3, true);
  assert(editor.erase({.15f, 1, -1}, .01f));
  assert(editor.copy().strokes.size() == 1);
  assert(editor.undo());
  auto old = editor.frame().revision;
  editor.selectAll();
  editor.deleteSelection();
  editor.saved(old);
  assert(editor.frame().dirty);
  assert(editor.undo());
  assert(editor.copy().strokes.size() == 2);
  // Snapshot ownership stays valid while another thread replaces/edits the
  // model.
  auto snapshot = editor.frame().document;
  auto worker = std::async(std::launch::async, [&] {
    for (int i = 0; i < 30; ++i) {
      editor.selectAll();
      editor.transformSelection({.001f, 0, 0}, {}, 1);
    }
  });
  for (int i = 0; i < 100; ++i)
    assert(editor.frame().document->strokes.size() == 2);
  worker.get();
  assert(snapshot->strokes[0].points[0].position.x == .1f);
  for (const auto &b : brushes()) {
    Stroke s;
    s.brush = b.guid;
    s.points = {{{0, 0, -1}, {}, .5f, 10}, {{.1f, 0, -1}, {}, 1, 20}};
    auto mesh = brushMesh(s);
    assert(!mesh.indices.empty());
    for (auto &v : mesh.vertices)
      assert(finite(v.position) && finite(v.normal));
  }
  // Per-control-point replacement colors are interpreted without changing
  // bytes.
  Stroke gradient;
  gradient.brush = brushes()[0].guid;
  gradient.points = {{{0, 0, -1}, {}, 1, 0}, {{.1f, 0, -1}, {}, 1, 10}};
  gradient.extensions[16] = {1,   0, 0, 0,   2, 0, 0,   0,  3,
                             255, 0, 0, 255, 0, 0, 255, 255};
  auto mesh = brushMesh(gradient);
  assert(mesh.vertices.front().color.r == 1 &&
         mesh.vertices.front().color.b == 0);
  assert(mesh.vertices[8].color.b == 1);
  std::cout << "Shared editor: mirrored transactions, source ownership, "
               "selection, transforms, duplicate, recolor, hidden layers, "
               "immutable concurrent snapshots and brush colors passed\n";
}
