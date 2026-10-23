// SPDX-License-Identifier: Apache-2.0
#include "document.h"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ob {
void validate(const Stroke &s) {
  if (s.brush.empty() || s.brush.size() > 128 || !std::isfinite(s.size) ||
      s.size <= 0 || s.size > 1000 || !std::isfinite(s.scale) || s.scale <= 0 ||
      s.scale > 10000)
    throw std::runtime_error("Invalid brush size, scale or identity");
  for (float v : {s.color.r, s.color.g, s.color.b, s.color.a})
    if (!std::isfinite(v))
      throw std::runtime_error("Invalid brush color");
  if (s.points.size() > 2000000)
    throw std::runtime_error("Stroke has too many control points");
  for (const auto &p : s.points)
    if (!finite(p.position) || !finite(p.orientation) ||
        !std::isfinite(p.pressure) || p.pressure < 0 || p.pressure > 100)
      throw std::runtime_error("Invalid stroke control point");
}
void History::apply(const Edit &e, bool forward) {
  bool insert = forward == e.inserted;
  if (insert)
    document.strokes.insert(document.strokes.begin() + e.index, e.stroke);
  else
    document.strokes.erase(document.strokes.begin() + e.index);
}
void History::append(Stroke s) {
  validate(s);
  if (saved > past.size())
    savedReachable = false;
  future.clear();
  Edit e{document.strokes.size(), std::move(s), true};
  apply(e, true);
  past.push_back(std::move(e));
}
void History::erase(size_t index) {
  if (index >= document.strokes.size())
    throw std::out_of_range("Stroke index");
  if (saved > past.size())
    savedReachable = false;
  future.clear();
  Edit e{index, document.strokes[index], false};
  apply(e, true);
  past.push_back(std::move(e));
}
bool History::eraseNear(Vec3 p, float radius) {
  for (size_t i = document.strokes.size(); i > 0; --i) {
    const auto &s = document.strokes[i - 1];
    for (size_t j = 0; j < s.points.size(); ++j) {
      auto a = s.points[j].position;
      auto b = s.points[j ? j - 1 : j].position;
      if (segmentDistance(p, a, b) <= radius + s.size * s.scale * .5f) {
        erase(i - 1);
        return true;
      }
    }
  }
  return false;
}
bool History::undo() {
  if (past.empty())
    return false;
  auto e = std::move(past.back());
  past.pop_back();
  apply(e, false);
  future.push_back(std::move(e));
  return true;
}
bool History::redo() {
  if (future.empty())
    return false;
  auto e = std::move(future.back());
  future.pop_back();
  apply(e, true);
  past.push_back(std::move(e));
  return true;
}
void atomicWrite(const std::filesystem::path &path, const Bytes &data) {
  auto target = std::filesystem::absolute(path);
  if (std::filesystem::is_symlink(target))
    target = std::filesystem::canonical(target);
  std::string temp = target.string() + ".XXXXXX";
  std::vector<char> name(temp.begin(), temp.end());
  name.push_back(0);
  int fd = mkstemp(name.data());
  if (fd < 0)
    throw std::runtime_error("Cannot create temporary sketch file");
  try {
    struct stat st{};
    if (stat(target.c_str(), &st) == 0 && fchmod(fd, st.st_mode & 0777))
      throw std::runtime_error("Cannot preserve sketch permissions");
    size_t written = 0;
    while (written < data.size()) {
      ssize_t n = write(fd, data.data() + written, data.size() - written);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        throw std::runtime_error("Cannot write sketch");
      written += size_t(n);
    }
    if (fsync(fd))
      throw std::runtime_error("Cannot sync sketch");
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error("Cannot close sketch");
    }
    fd = -1;
    if (rename(name.data(), target.c_str()))
      throw std::runtime_error("Cannot replace sketch");
    int directory = open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (directory >= 0) {
      fsync(directory);
      close(directory);
    }
  } catch (...) {
    if (fd >= 0)
      close(fd);
    unlink(name.data());
    throw;
  }
}
} // namespace ob
