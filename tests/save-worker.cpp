// SPDX-License-Identifier: Apache-2.0
#include "save-worker.h"
#include <cassert>
#include <future>
#include <iostream>
#include <unistd.h>
using namespace ob;
using namespace std::chrono_literals;
void stroke(Editor &editor, float x) {
  editor.sample(1, {{x, 1, -1}, {}, 1, 0});
  editor.finish(1);
}
int main() {
  char pattern[] = "/tmp/openbrush-save-test-XXXXXX";
  auto directory = mkdtemp(pattern);
  assert(directory);
  auto path = std::filesystem::path(directory) / "drawing.tilt";
  Editor editor;
  stroke(editor, 0);
  auto settings = editor.tools();
  settings.layer = 7;
  editor.configure(settings);
  std::mutex gate;
  std::condition_variable changed;
  int entered = 0, allowed = 0;
  std::vector<size_t> written;
  const auto caller = std::this_thread::get_id();
  {
    SaveWorker worker(editor, path,
                      [&](const Document &document, const auto &file) {
                        assert(std::this_thread::get_id() != caller);
                        std::unique_lock lock(gate);
                        int call = ++entered;
                        changed.notify_all();
                        changed.wait(lock, [&] { return allowed >= call; });
                        writeTilt(document, file);
                        written.push_back(document.strokes.size());
                      });
    worker.request();
    {
      std::unique_lock lock(gate);
      assert(changed.wait_for(lock, 2s, [&] { return entered == 1; }));
    }
    stroke(editor, .1f);
    editor.layerVisible(7, false); // Hidden content must be saved as well.
    settings.layer = 0;
    editor.configure(settings);
    auto request = std::async(std::launch::async, [&] {
      for (int i = 0; i < 1000; ++i)
        worker.request();
    });
    auto ready = request.wait_for(2s) == std::future_status::ready;
    if (!ready) {
      std::lock_guard lock(gate);
      allowed = 10000;
      changed.notify_all();
    }
    assert(ready);
    request.get();
    {
      std::unique_lock lock(gate);
      allowed = 1;
      changed.notify_all();
      assert(changed.wait_for(lock, 2s, [&] { return entered == 2; }));
    }
    // Completing the old version cannot mark the newer document as saved.
    assert(editor.frame().dirty);
    stroke(editor, .2f);
    {
      std::lock_guard lock(gate);
      allowed = 2;
      changed.notify_all();
    }
    worker.flush();
    assert(editor.frame().dirty);
    assert((written == std::vector<size_t>{1, 2}));
    auto saved = readTilt(path);
    assert(saved.strokes.size() == 2 && saved.strokes[1].layer == 7);
    {
      std::lock_guard lock(gate);
      allowed = 3;
    }
    worker.request();
    worker.flush();
    assert(!editor.frame().dirty);
    assert(written.size() == 3 && written.back() == 3);
  }
  stroke(editor, .3f);
  std::atomic_bool fail{true};
  {
    SaveWorker worker(editor, path, [&](const auto &doc, const auto &file) {
      if (fail)
        throw std::runtime_error("injected disk failure");
      writeTilt(doc, file);
    });
    worker.request();
    bool rejected = false;
    try {
      worker.flush();
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    assert(rejected && editor.frame().dirty);
    assert(readTilt(path).strokes.size() ==
           3); // Failed write preserves existing artwork.
    fail = false;
    worker.request();
    worker.flush();
    assert(!editor.frame().dirty && readTilt(path).strokes.size() == 4);
  }
  stroke(editor, .4f);
  {
    SaveWorker worker(editor, path);
    worker.request();
  } // Destruction drains pending work.
  assert(readTilt(path).strokes.size() == 5);
  editor.sample(1, {{.5f, 1, -1}, {}, 1, 1});
  auto recovery = editor.snapshot(true);
  assert(recovery.document.strokes.size() == 6);
  assert(editor.snapshot().document.strokes.size() == 5);
  assert(editor.frame().document->strokes.size() ==
         4); // Visibility is view-only.
  std::filesystem::remove_all(directory);
  std::cout
      << "Background saves: nonblocking requests, coalescing, hidden layers, "
         "revision guards, failure/retry and shutdown drain passed\n";
}
