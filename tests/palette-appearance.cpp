// SPDX-License-Identifier: Apache-2.0
#include "palette.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace ob;
int main() {
  char root[] = "/tmp/openbrush-appearance-XXXXXX";
  assert(mkdtemp(root));
  setenv("XDG_CONFIG_HOME", root, 1);
  std::filesystem::create_directories(std::filesystem::path(root) / "dome");
  auto config = std::filesystem::path(root) / "dome/appearance.json";
  {
    std::ofstream f(config);
    f << R"({"light":true,"accent":"#ff0000","font":"monospace","font_size":16,"font_scale":2,"opacity":128})";
  }
  {
    auto theme = paletteTheme();
    assert(theme.pixels >= 32);
    assert(theme.background.r > .8f);
    assert(theme.accent.r == 1 && theme.accent.g == 0);
    assert(!theme.fontFile.empty());
    Editor editor;
    Palette palette;
    palette.place({{0, 1.5f, 0}, {}, 1});
    auto image = palette.image(editor.frame());
    assert(image.width >= 1280 && image.height >= 1420);
    assert(image.rgba[0] > 200);
    for (const auto &b : palette.layout(editor.frame())) {
      int width = 0;
      for (unsigned char c : b.label)
        width += theme.glyphs.at(c).advance;
      assert(width <= b.w - 16 * (image.width / 640));
      auto local =
          Vec3{((b.x + b.w * .5f) / image.width - .5f) * Palette::width,
               (.5f - (b.y + b.h * .5f) / image.height) * Palette::height, 0};
      auto point = transform(palette.transform(), local);
      auto hit = palette.hit(point + Vec3{0, 0, .2f}, {0, 0, -1});
      assert(hit && hit->button >= 0);
    }
    auto tool = editor.tools();
    tool.tool = Tool::Erase;
    editor.configure(tool);
    auto paintSize = tool.size, eraser = tool.eraser;
    palette.activate("size:+", editor, {});
    assert(editor.tools().size == paintSize && editor.tools().eraser > eraser);
    palette.activate("brush:0", editor, {});
    assert(editor.tools().tool == Tool::Paint);
    assert(editor.tools().size == paintSize);
    palette.activate("layer:toggle", editor, {});
    assert(!editor.frame().currentLayerVisible);
    bool hidden = false;
    for (const auto &b : palette.layout(editor.frame()))
      if (b.command == "layer:toggle")
        hidden = b.label.find("Hidden") != std::string::npos;
    assert(hidden);
  }
  {
    std::ofstream f(config);
    f << R"({"light":[],"font":42,"font_size":"bad","font_scale":-1,"accent":false,"opacity":{}})";
  }
  auto fallback = paletteTheme();
  assert(fallback.background.r < .3f);
  assert(fallback.pixels > 0);
  assert(!fallback.glyphs.empty());
  std::filesystem::remove_all(root);
  std::cout << "Palette appearance: light theme, accent, 2x bitmap layout and "
               "picking, per-tool sizes, hidden layer state and malformed "
               "preferences passed\n";
}
