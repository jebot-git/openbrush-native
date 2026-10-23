// SPDX-License-Identifier: Apache-2.0
#include "save-worker.h"
#include <iostream>
namespace ob {
SaveWorker::SaveWorker(Editor &editor, std::filesystem::path path,
                       Writer writer)
    : editor(editor), path(std::move(path)), writer(std::move(writer)),
      worker([this] { run(); }) {}
SaveWorker::~SaveWorker() {
  {
    std::lock_guard lock(mutex);
    stopping = true;
  }
  wake.notify_all();
  worker.join();
}
void SaveWorker::request() {
  {
    std::lock_guard lock(mutex);
    if (stopping)
      throw std::logic_error("Save worker has stopped");
    ++requested;
  }
  wake.notify_all();
}
void SaveWorker::flush() {
  std::unique_lock lock(mutex);
  wake.wait(lock, [this] { return completed == requested; });
  if (error)
    std::rethrow_exception(error);
}
void SaveWorker::run() {
  std::unique_lock lock(mutex);
  for (;;) {
    wake.wait(lock, [this] { return stopping || completed != requested; });
    if (completed == requested && stopping)
      return;
    const auto serial = requested;
    lock.unlock();
    std::exception_ptr failure;
    try {
      auto snapshot = editor.snapshot();
      writer(snapshot.document, path);
      editor.saved(snapshot.revision);
    } catch (...) {
      failure = std::current_exception();
      try {
        std::rethrow_exception(failure);
      } catch (const std::exception &e) {
        std::cerr << "OpenBrush Native: save failed: " << e.what() << '\n';
      } catch (...) {
        std::cerr << "OpenBrush Native: save failed\n";
      }
    }
    lock.lock();
    error = failure;
    completed = serial;
    wake.notify_all();
  }
}
} // namespace ob
