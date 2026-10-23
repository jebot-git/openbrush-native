// SPDX-License-Identifier: Apache-2.0
#include "interaction.h"
#include <cassert>
#include <iostream>
using namespace ob;
void close(Vec3 a, Vec3 b) { assert(length(a - b) < 1e-4f); }
void close(float a, float b) { assert(std::abs(a - b) < 1e-4f); }
Vec3 buttonPoint(Palette &p, const EditorFrame &frame,
                 const std::string &command) {
  auto image = p.image(frame);
  auto list = p.layout(frame);
  for (auto &b : list)
    if (b.command == command) {
      float x = (b.x + b.w * .5f) / image.width - .5f,
            y = .5f - (b.y + b.h * .5f) / image.height;
      return transform(p.transform(),
                       {x * Palette::width, y * Palette::height, 0});
    }
  throw std::runtime_error("Button not found");
}
int main() {
  // Rigid composition/inverse and one/two-hand navigation operate in room
  // space.
  Transform base{{.2f, .3f, -1}, {0, .2f, 0, .98f}, 2};
  auto sample = Vec3{.3f, .2f, .1f};
  close(transform(inverse(base), transform(base, sample)), sample);
  Navigation nav;
  std::array<TrackedHand, 2> hands{};
  hands[0].valid = true;
  hands[0].grip.position = {0, 1, -1};
  hands[0].squeeze = 1;
  assert(!nav.update(hands, {}));
  hands[0].grip.position.x = .2f;
  auto moved = nav.update(hands, {});
  assert(moved);
  close(moved->position, {.2f, 0, 0});
  hands[1].valid = true;
  hands[1].squeeze = 1;
  hands[1].grip.position = {.6f, 1, -1};
  assert(!nav.update(hands, *moved));
  auto pivot = (hands[0].grip.position + hands[1].grip.position) * .5f;
  hands[0].grip.position.x = 0;
  hands[1].grip.position.x = .8f;
  auto scaled = nav.update(hands, *moved);
  assert(scaled);
  close(scaled->scale, 2);
  close(transform(*scaled, transform(inverse(*moved), pivot)), pivot);
  hands[1].valid = false;
  assert(!nav.update(hands, *scaled));
  auto noJump = nav.update(hands, *scaled);
  assert(noJump);
  close(noJump->position, scaled->position);
  close(noJump->scale, scaled->scale);
  hands[0].squeeze = 0;
  assert(!nav.update(hands, *scaled));
  assert(!nav.grabbing());
  Editor editor;
  editor.navigate({{.2f, 0, 0}, {}, 2});
  auto tools = editor.tools();
  tools.mirror = false;
  editor.configure(tools);
  editor.sampleRoom(2, {{.4f, 1, -1}, {}, 1, 123});
  editor.finish(2);
  auto document = editor.copy();
  close(document.strokes[0].points[0].position, {.1f, .5f, -.5f});
  close(document.strokes[0].size, tools.size / 2);
  assert(document.strokes[0].points[0].timeMs == 123);
  auto version = editor.frame().revision;
  editor.navigate({});
  assert(editor.frame().revision == version);
  assert(editor.copy().strokes[0].points[0].position.x == .1f);
  int saves = 0;
  Palette palette;
  Transform head{{0, 1.6f, 0}, {}, 1};
  palette.place(head);
  auto target = buttonPoint(palette, editor.frame(), "brush:4");
  auto origin = target + Vec3{0, 0, .3f};
  auto hit = palette.hit(origin, {0, 0, -1});
  assert(hit && hit->button >= 0);
  close(hit->roomPoint, target);
  assert(!palette.hit(origin, {0, 0, 1}));
  assert(!palette.hit(target + Vec3{2, 0, .3f}, {0, 0, -1}));
  palette.point(origin, {0, 0, -1}, false, true, editor, [] {});
  assert(palette.point(origin, {0, 0, -1}, true, true, editor, [] {}));
  assert(editor.tools().brush == brushes()[4].guid);
  palette.point(origin + Vec3{2, 0, 0}, {0, 0, -1}, true, true, editor, [] {});
  assert(editor.copy().strokes.size() == 1);
  palette.point(origin, {0, 0, -1}, false, true, editor, [] {});
  auto image = palette.image(editor.frame());
  assert(image.rgba.size() == size_t(image.width * image.height * 4));
  auto unchanged = palette.image(editor.frame()).revision;
  assert(unchanged == image.revision);
  palette.activate("size:+", editor, [] {});
  assert(editor.tools().size > tools.size);
  palette.activate("mirror", editor, [] {});
  assert(editor.tools().mirror);
  palette.activate("save", editor, [&] { ++saves; });
  assert(saves == 1);
  palette.activate("hide", editor, {});
  assert(palette.visible() && !palette.expanded());
  assert(palette.image(editor.frame()).rgba[3] == 0);
  auto tab = buttonPoint(palette, editor.frame(), "expand");
  palette.point(tab + Vec3{0, 0, .2f}, {0, 0, -1}, false, true, editor, [] {});
  palette.point(tab + Vec3{0, 0, .2f}, {0, 0, -1}, true, true, editor, [] {});
  assert(palette.expanded());
  palette.point(tab + Vec3{0, 0, .2f}, {0, 0, -1}, false, true, editor, [] {});
  auto stored = editor.copy().strokes[0].points[0].position;
  palette.activate("fit", editor, {});
  auto fitted = editor.canvas();
  auto content = editor.copy();
  Vec3 center{};
  size_t pointCount = 0;
  for (auto &s : content.strokes)
    for (auto &p : s.points) {
      center = center + p.position;
      ++pointCount;
    }
  center = center * (1.f / pointCount);
  close(transform(fitted, center), head.position + Vec3{0, -.05f, -.9f});
  close(editor.copy().strokes[0].points[0].position, stored);
  // Exercise production interaction, including whole-gesture capture and
  // focus/tracking loss.
  Editor shared;
  int changed = 0;
  Interaction interaction(shared, [&] { ++changed; }, [&] { ++saves; });
  hands = {};
  for (auto &h : hands) {
    h.valid = h.aimValid = true;
    h.grip.position = {.4f, 1.3f, -1};
    h.aim = h.grip;
  }
  interaction.update(hands, head, {});
  auto click = buttonPoint(interaction.panel(), shared.frame(), "color:1");
  hands[1].aim.position = click + Vec3{0, 0, .3f};
  hands[1].trigger = 1;
  interaction.update(hands, head, {});
  assert(shared.tools().color.r == 1);
  hands[1].aim.position.x = 2;
  hands[1].grip.position.x = 2;
  interaction.update(hands, head, {});
  interaction.update(hands, head, {});
  assert(shared.copy(true).strokes.empty());
  hands[1].trigger = 0;
  interaction.update(hands, head, {});
  hands[1].trigger = 1;
  hands[1].timeMs = 444;
  interaction.update(hands, head, {});
  hands[1].grip.position.x += .02f;
  interaction.update(hands, head, {});
  hands[1].trigger = 0;
  interaction.update(hands, head, {});
  assert(shared.copy().strokes.size() == 1);
  assert(changed == 1);
  hands[1].trigger = 1;
  interaction.update(hands, head, {false});
  interaction.update(hands, head, {});
  assert(shared.copy(true).strokes.size() == 1);
  hands[1].trigger = 0;
  interaction.update(hands, head, {});
  hands[1].trigger = 1;
  interaction.update(hands, head, {});
  hands[1].valid = false;
  interaction.update(hands, head, {});
  assert(shared.copy().strokes.size() == 2);
  hands[1].valid = true;
  interaction.update(hands, head, {});
  assert(shared.copy(true).strokes.size() == 2);
  hands[1].trigger = 0;
  interaction.update(hands, head, {});
  auto before = shared.frame().revision;
  hands[0].squeeze = 1;
  interaction.update(hands, head, {});
  hands[0].grip.position.x += .1f;
  interaction.update(hands, head, {});
  assert(shared.frame().revision == before);
  close(shared.canvas().position.x, .1f);
  Editor simple;
  Interaction simpleInput(simple, [] {}, [] {});
  std::array<TrackedHand, 2> simpleHands{};
  for (int i = 0; i < 2; ++i) {
    simpleHands[i].valid = simpleHands[i].aimValid = true;
    simpleHands[i].grip.position = {float(i + 2), 1, -1};
    simpleHands[i].aim = simpleHands[i].grip;
  }
  simpleInput.update(simpleHands, head, {});
  simpleInput.panel().activate("tool:4", simple, {});
  simpleHands[0].trigger = 1;
  simpleInput.update(simpleHands, head, {});
  simpleHands[0].grip.position.x += .2f;
  simpleInput.update(simpleHands, head, {});
  close(simple.canvas().position.x, .2f);
  assert(simple.copy().strokes.empty());
  simpleHands[1].trigger = 1;
  simpleInput.update(simpleHands, head, {});
  simpleHands[1].grip.position.x += .3f;
  simpleInput.update(simpleHands, head, {});
  assert(simple.canvas().scale > 1);
  simpleInput.panel().activate("brush:0", simple, {});
  simpleInput.update(simpleHands, head, {});
  assert(simple.copy(true).strokes.empty());
  simpleHands[0].trigger = simpleHands[1].trigger = 0;
  simpleInput.update(simpleHands, head, {});
  simpleHands[1].trigger = 1;
  simpleInput.update(simpleHands, head, {});
  simpleHands[1].trigger = 0;
  simpleInput.update(simpleHands, head, {});
  assert(simple.copy().strokes.size() == 1);
  Editor fine;
  auto fineSettings = fine.tools();
  fineSettings.size = .001f;
  fine.configure(fineSettings);
  fine.navigate({{}, {}, 20});
  fine.sampleRoom(2, {{0, 1, -1}, {}, 1, 1});
  fine.sampleRoom(2, {{.0004f, 1, -1}, {}, 1, 2});
  fine.finish(2);
  assert(fine.copy().strokes[0].points.size() == 2);
  Editor startup;
  Interaction initial(startup, [] {}, [] {});
  auto held = simpleHands;
  held[1].trigger = 1;
  initial.update(held, head, {});
  assert(startup.copy(true).strokes.empty());
  held[1].trigger = 0;
  initial.update(held, head, {});
  held[1].trigger = 1;
  initial.update(held, head, {});
  held[1].trigger = 0;
  initial.update(held, head, {});
  assert(startup.copy().strokes.size() == 1);
  std::cout
      << "Native interaction: panel hit/projection, captured clicks, tool "
         "changes, save dispatch, tracking/focus recovery, timestamps, grip "
         "transitions, pivot scaling and room-space painting passed\n";
}
