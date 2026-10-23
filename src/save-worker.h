// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "editor.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <thread>
namespace ob {
// Requests are cheap; document copying, compression and disk IO run on worker.
// At most one write and one coalesced pending request exist at a time.
class SaveWorker {
public:
  using Writer =
      std::function<void(const Document &, const std::filesystem::path &)>;
  SaveWorker(Editor &editor, std::filesystem::path path,
             Writer writer = writeTilt);
  ~SaveWorker();
  SaveWorker(const SaveWorker &) = delete;
  SaveWorker &operator=(const SaveWorker &) = delete;
  void request();
  // Wait for all requested saves and propagate the last unresolved failure.
  void flush();

private:
  Editor &editor;
  std::filesystem::path path;
  Writer writer;
  std::mutex mutex;
  std::condition_variable wake;
  uint64_t requested = 0, completed = 0;
  bool stopping = false;
  std::exception_ptr error;
  void run();
  std::thread worker;
};
} // namespace ob
