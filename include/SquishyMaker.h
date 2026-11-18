#pragma once
#include "GameAudio.h"
#include "GameGfx.h"
#include <math.h>

// Create-your-own squishy: pick a kind, then its options one step at a time,
// then admire the finished squishy up close. Tapping it squishes it.
namespace Squishy {
using GameGfx::rgb;
enum Kind { DUMPLING, NDOH, TABY };
enum Stage { PICK, ADMIRE };
struct Step { const char *prompt; const char *names[3]; };

const Step kindStep = {"PICK A SQUISHY", {"DUMPLING", "N-DOH", "TABY"}};
// Steps after the kind. The dumpling has two; the others have three.
const Step dumplingSteps[2] = {
  {"PICK A COLOR", {"PINK", "YELLOW", "GLITTER GOLD"}},
  {"PICK A FACE", {"SAD", "EXCITED", "GLASSES"}},
};
const Step ndohSteps[3] = {
  {"PICK A SHAPE", {"ICE CREAM", "POPSICLE", "ICE CUBE"}},
  {"PICK A COLOR", {"PINK", "BLUE", "PURPLE"}},
  {"PICK A STYLE", {"GLITTER", "SHINY", "RAINBOW"}},
};
const Step tabySteps[3] = {
  {"PICK A SHAPE", {"CAT", "PIZZA", "CHICK"}},
  {"PICK A COLOR", {"ORANGE", "RED", "GREEN"}},
  {"PICK A STYLE", {"STICKY", "GLITTER", "FLUFFY"}},
};
const uint16_t dumplingColors[3] = {rgb(255,146,170), rgb(255,219,85), rgb(219,182,0)};
const uint16_t ndohColors[3] = {rgb(255,146,170), rgb(109,182,255), rgb(182,109,255)};
const uint16_t tabyColors[3] = {rgb(255,146,0), rgb(255,73,85), rgb(109,219,85)};

enum Shape { DUMPLING_SHAPE, ICE_CREAM, POPSICLE, ICE_CUBE, CAT, PIZZA, CHICK };
enum Mod { NO_MOD, GLITTER, SHINY, RAINBOW, STICKY, FLUFFY };
enum Face { CUTE, SAD, EXCITED, GLASSES };
struct Look { Shape shape; uint16_t color; Mod mod; Face face; };

const TouchButton exitButton = {8, 6, 60, 25};
const TouchButton cards[3] = {{10,68,96,128},{112,68,96,128},{214,68,96,128}};
const TouchButton squishTarget = {70, 70, 180, 132};
const TouchButton againButton = {190, 206, 122, 28};

Stage stage = PICK;
int step = 0;
int choice[4] = {0, -1, -1, -1};  // [0] kind, then that kind's steps; -1 = not chosen yet
uint32_t lastFrame = 0;
float squash = 0, squashVel = 0;
struct Heart { float x, y, vy, life; } hearts[8];

int stepCount() { return choice[0] == DUMPLING ? 3 : 4; }
const Step &stepAt(int s) {
  if (s == 0) return kindStep;
  if (choice[0] == DUMPLING) return dumplingSteps[s - 1];
  return choice[0] == NDOH ? ndohSteps[s - 1] : tabySteps[s - 1];
}

// Unchosen options fall back to the first color and no style, so previews always render.
Look lookFor(const int *c) {
  Look l = {DUMPLING_SHAPE, dumplingColors[0], NO_MOD, CUTE};
  const int a = c[1] < 0 ? 0 : c[1], b = c[2] < 0 ? 0 : c[2];
  if (c[0] == DUMPLING) {
    l.color = dumplingColors[a];
    if (a == 2) l.mod = GLITTER;  // Glittery gold.
    if (c[2] >= 0) l.face = c[2] == 0 ? SAD : c[2] == 1 ? EXCITED : GLASSES;
  } else if (c[0] == NDOH) {
    l.shape = (Shape)(ICE_CREAM + a);
    l.color = ndohColors[b];
    if (c[3] >= 0) l.mod = c[3] == 0 ? GLITTER : c[3] == 1 ? SHINY : RAINBOW;
  } else {
    l.shape = (Shape)(CAT + a);
    l.color = tabyColors[b];
    if (c[3] >= 0) l.mod = c[3] == 0 ? STICKY : c[3] == 1 ? GLITTER : FLUFFY;
  }
  return l;
}

// ---- Drawing -------------------------------------------------------------
void arc(int cx, int cy, int rx, int ry, bool lower, uint16_t color, int thick = 2) {
  if (rx < 1) rx = 1;
  for (int dx = -rx; dx <= rx; ++dx) {
    const int dy = (int)(ry * sqrtf(1.0f - (float)(dx * dx) / (rx * rx)));
    gameFrame.fillRect(cx + dx, lower ? cy + dy : cy - dy, 1, thick, color);
  }
}

uint32_t hash(int x, int y) {
  uint32_t h = (uint32_t)(x * 374761393 + y * 668265263);
  h = (h ^ (h >> 13)) * 1274126177;
  return h ^ (h >> 16);
}

// Rewrites body-colored pixels: rainbow stripes or twinkling glitter.
void decorateBody(uint16_t body, Mod mod, int x0, int y0, int x1, int y1, uint32_t now) {
  uint8_t *px = GameGfx::pixels();
  if (!px) return;
  const uint8_t key = GameGfx::pack(body);
  const uint16_t rainbow[6] = {rgb(255,109,109), rgb(255,182,85), rgb(255,255,109),
                               rgb(146,255,146), rgb(109,182,255), rgb(219,146,255)};
  x0 = max(0, x0); y0 = max(0, y0); x1 = min(GameDisplay::width - 1, x1); y1 = min(GameDisplay::height - 1, y1);
  const int h = max(1, y1 - y0);
  const uint32_t twinkle = now / 180;
  for (int y = y0; y <= y1; ++y) {
    uint8_t *row = px + y * GameDisplay::width;
    for (int x = x0; x <= x1; ++x) {
      if (row[x] != key) continue;
      if (mod == RAINBOW) row[x] = GameGfx::pack(rainbow[(y - y0) * 6 / (h + 1)]);
      else if (mod == GLITTER) {
        const uint32_t v = hash(x, y);
        if (v % 23 == 0) row[x] = GameGfx::pack((v / 23 + twinkle) % 3 ? rgb(255,255,219) : rgb(255,219,0));
        else if (v % 97 == 1) row[x] = GameGfx::pack(TFT_WHITE);
      }
    }
  }
}

void drawFace(int fx, int fy, float R, float sx, Face face, bool squished, Shape shape) {
  const int ex = (int)(0.3f * R * sx), er = max(2, (int)(0.1f * R));
  const uint16_t ink = rgb(36,0,36);
  // Rosy cheeks.
  gameFrame.fillEllipse(fx - ex - er, fy + er + 1, er + 1, max(1, er / 2 + 1), rgb(255,109,170));
  gameFrame.fillEllipse(fx + ex + er, fy + er + 1, er + 1, max(1, er / 2 + 1), rgb(255,109,170));
  if (squished) {
    // Squeezed shut: > <
    gameFrame.drawLine(fx - ex - er, fy - er, fx - ex + er, fy, ink);
    gameFrame.drawLine(fx - ex + er, fy, fx - ex - er, fy + er, ink);
    gameFrame.drawLine(fx + ex + er, fy - er, fx + ex - er, fy, ink);
    gameFrame.drawLine(fx + ex - er, fy, fx + ex + er, fy + er, ink);
  } else if (face == SAD) {
    arc(fx - ex, fy, er + 1, er / 2 + 1, true, ink);
    arc(fx + ex, fy, er + 1, er / 2 + 1, true, ink);
    gameFrame.fillEllipse(fx - ex + 1, fy + er + 3, max(1, er / 2), er, rgb(109,182,255)); // Tear.
  } else {
    const int r = face == EXCITED ? er + 2 : er;
    gameFrame.fillCircle(fx - ex, fy, r, ink);
    gameFrame.fillCircle(fx + ex, fy, r, ink);
    gameFrame.fillCircle(fx - ex - r / 3, fy - r / 3, max(1, r / 3), TFT_WHITE);
    gameFrame.fillCircle(fx + ex - r / 3, fy - r / 3, max(1, r / 3), TFT_WHITE);
    if (face == EXCITED) {
      gameFrame.drawPixel(fx - ex + r / 3, fy + r / 3, TFT_WHITE);
      gameFrame.drawPixel(fx + ex + r / 3, fy + r / 3, TFT_WHITE);
    }
    if (face == GLASSES) {
      const int gr = er + 3;
      gameFrame.drawCircle(fx - ex, fy, gr, ink);
      gameFrame.drawCircle(fx - ex, fy, gr - 1, ink);
      gameFrame.drawCircle(fx + ex, fy, gr, ink);
      gameFrame.drawCircle(fx + ex, fy, gr - 1, ink);
      gameFrame.drawFastHLine(fx - ex + gr, fy - 1, 2 * ex - 2 * gr, ink);
    }
  }
  // Mouth: chicks get a beak, cats a little "w".
  const int my = fy + er + 2, mw = max(3, (int)(0.12f * R * sx));
  if (shape == CHICK) {
    gameFrame.fillTriangle(fx - mw, my - 1, fx + mw, my - 1, fx, my + mw, rgb(255,109,0));
  } else if (shape == CAT && !squished) {
    arc(fx - mw / 2, my, mw / 2 + 1, 2, true, ink, 1);
    arc(fx + mw / 2, my, mw / 2 + 1, 2, true, ink, 1);
  } else if (squished || face == EXCITED) {
    gameFrame.fillEllipse(fx, my + 1, mw, mw * 2 / 3 + 1, rgb(182,0,73));
  } else if (face == SAD) {
    arc(fx, my + 3, mw, 2, false, ink);
  } else {
    arc(fx, my - 1, mw, 2, true, ink);
  }
}

// Squishy standing on (cx, by), size R; sx/sy squash it while keeping its base planted.
void drawSquishy(const Look &l, int cx, int by, float R, float sx, float sy, uint32_t now, bool squished) {
  const float RX = R * sx, RY = R * sy;
  const uint16_t c = l.color, shade = GameGfx::darker(c), light = GameGfx::lighter(c);
  GameGfx::darkenEllipse(cx, by + 1, (int)(RX * 1.05f), max(2, (int)(R * 0.1f)));
  int fx = cx, fy = by - (int)(0.55f * RY);
  // Fluffy fur: a ring of soft tufts just outside the body.
  if (l.mod == FLUFFY) {
    const float cy = by - 0.6f * RY;
    for (int a = 0; a < 360; a += 20) {
      const float t = a * PI / 180;
      gameFrame.fillCircle(cx + (int)(cosf(t) * 0.95f * RX), (int)(cy + sinf(t) * 0.62f * RY), max(2, (int)(0.14f * R)), light);
    }
  }
  switch (l.shape) {
    case DUMPLING_SHAPE: {
      gameFrame.fillEllipse(cx, by - (int)(0.5f * RY), (int)(1.12f * RX), (int)(0.52f * RY), shade);
      gameFrame.fillEllipse(cx, by - (int)(0.56f * RY), (int)(1.08f * RX), (int)(0.52f * RY), c);
      fy = by - (int)(0.42f * RY);
      break;
    }
    case ICE_CREAM: {
      fy = by - (int)(1.02f * RY);
      gameFrame.fillEllipse(cx, by - (int)(1.0f * RY), (int)(0.78f * RX), (int)(0.58f * RY), c);
      break;
    }
    case POPSICLE: {
      gameFrame.fillRoundRect(cx - (int)(0.1f * RX), by - (int)(0.4f * RY), max(3, (int)(0.2f * RX)), (int)(0.4f * RY), 2, rgb(219,182,85));
      gameFrame.fillRoundRect(cx - (int)(0.52f * RX), by - (int)(1.5f * RY), (int)(1.04f * RX), (int)(1.16f * RY), (int)(0.45f * min(RX, RY)), c);
      fy = by - (int)(0.95f * RY);
      break;
    }
    case ICE_CUBE: {
      gameFrame.fillRoundRect(cx - (int)(0.75f * RX), by - (int)(1.3f * RY), (int)(1.5f * RX), (int)(1.3f * RY), (int)(0.25f * R), shade);
      gameFrame.fillRoundRect(cx - (int)(0.75f * RX), by - (int)(1.3f * RY), (int)(1.42f * RX), (int)(1.22f * RY), (int)(0.25f * R), c);
      fy = by - (int)(0.62f * RY);
      break;
    }
    case CAT: {
      const int top = by - (int)(1.12f * RY);
      gameFrame.fillTriangle(cx - (int)(0.85f * RX), top + (int)(0.45f * RY), cx - (int)(0.75f * RX), top - (int)(0.1f * RY), cx - (int)(0.3f * RX), top + (int)(0.2f * RY), c);
      gameFrame.fillTriangle(cx + (int)(0.85f * RX), top + (int)(0.45f * RY), cx + (int)(0.75f * RX), top - (int)(0.1f * RY), cx + (int)(0.3f * RX), top + (int)(0.2f * RY), c);
      gameFrame.fillEllipse(cx, by - (int)(0.58f * RY), (int)(0.95f * RX), (int)(0.58f * RY), c);
      fy = by - (int)(0.6f * RY);
      break;
    }
    case PIZZA: {
      gameFrame.fillTriangle(cx - (int)(0.9f * RX), by - (int)(1.2f * RY), cx + (int)(0.9f * RX), by - (int)(1.2f * RY), cx, by, c);
      fy = by - (int)(0.78f * RY);
      break;
    }
    case CHICK: {
      for (int i = -1; i <= 1; ++i)
        gameFrame.fillEllipse(cx + i * (int)(0.12f * RX), by - (int)(1.3f * RY), max(2, (int)(0.08f * RX)), max(3, (int)(0.16f * RY)), c);
      gameFrame.fillEllipse(cx, by - (int)(0.6f * RY), (int)(0.85f * RX), (int)(0.62f * RY), c);
      fy = by - (int)(0.72f * RY);
      break;
    }
  }
  // Glitter and rainbow recolor only the body pixels drawn so far.
  if (l.mod == GLITTER || l.mod == RAINBOW)
    decorateBody(c, l.mod, cx - (int)(1.3f * RX), by - (int)(1.65f * RY), cx + (int)(1.3f * RX), by, now);
  // Details drawn over the body.
  switch (l.shape) {
    case DUMPLING_SHAPE: {
      // Pinched top with pleats.
      const int ky = by - (int)(1.02f * RY);
      for (int i = -2; i <= 2; ++i)
        gameFrame.drawLine(cx + i * (int)(0.08f * RX), ky + 2, cx + i * (int)(0.32f * RX), ky + (int)(0.3f * RY), shade);
      gameFrame.fillEllipse(cx, ky, max(3, (int)(0.2f * RX)), max(2, (int)(0.12f * RY)), c);
      gameFrame.drawEllipse(cx, ky, max(3, (int)(0.2f * RX)), max(2, (int)(0.12f * RY)), shade);
      break;
    }
    case ICE_CREAM: {
      // Drips over a waffle cone.
      const int sy = by - (int)(0.62f * RY);
      for (int i = -2; i <= 2; ++i)
        gameFrame.fillEllipse(cx + i * (int)(0.28f * RX), sy + (i % 2 ? 3 : 0), max(2, (int)(0.13f * RX)), max(3, (int)(0.14f * RY)), c);
      gameFrame.fillTriangle(cx - (int)(0.55f * RX), sy + (int)(0.06f * RY), cx + (int)(0.55f * RX), sy + (int)(0.06f * RY), cx, by, rgb(219,146,85));
      for (int i = -1; i <= 1; ++i) {
        gameFrame.drawLine(cx + i * (int)(0.3f * RX) - (int)(0.2f * RX), sy + 3, cx + i * (int)(0.3f * RX) + (int)(0.15f * RX), by - (int)(0.2f * RY), rgb(182,109,36));
        gameFrame.drawLine(cx + i * (int)(0.3f * RX) + (int)(0.2f * RX), sy + 3, cx + i * (int)(0.3f * RX) - (int)(0.15f * RX), by - (int)(0.2f * RY), rgb(182,109,36));
      }
      break;
    }
    case POPSICLE:
      gameFrame.drawFastVLine(cx - (int)(0.28f * RX), by - (int)(1.38f * RY), (int)(0.9f * RY), light);
      break;
    case ICE_CUBE:
      // Frosty edges like a real ice cube.
      gameFrame.drawFastHLine(cx - (int)(0.6f * RX), by - (int)(1.2f * RY), (int)(0.9f * RX), TFT_WHITE);
      gameFrame.drawFastVLine(cx - (int)(0.66f * RX), by - (int)(1.12f * RY), (int)(0.7f * RY), TFT_WHITE);
      break;
    case CAT: {
      const int top = by - (int)(1.12f * RY);
      gameFrame.fillTriangle(cx - (int)(0.72f * RX), top + (int)(0.36f * RY), cx - (int)(0.68f * RX), top + (int)(0.06f * RY), cx - (int)(0.42f * RX), top + (int)(0.24f * RY), rgb(255,182,170));
      gameFrame.fillTriangle(cx + (int)(0.72f * RX), top + (int)(0.36f * RY), cx + (int)(0.68f * RX), top + (int)(0.06f * RY), cx + (int)(0.42f * RX), top + (int)(0.24f * RY), rgb(255,182,170));
      for (int i = -1; i <= 1; i += 2) {
        gameFrame.drawLine(cx + i * (int)(0.5f * RX), fy + (int)(0.12f * R), cx + i * (int)(1.05f * RX), fy + (int)(0.05f * R), shade);
        gameFrame.drawLine(cx + i * (int)(0.5f * RX), fy + (int)(0.2f * R), cx + i * (int)(1.0f * RX), fy + (int)(0.26f * R), shade);
      }
      break;
    }
    case PIZZA: {
      // Crust along the top and pepperoni slices.
      gameFrame.fillRoundRect(cx - (int)(0.98f * RX), by - (int)(1.38f * RY), (int)(1.96f * RX), max(4, (int)(0.24f * RY)), max(2, (int)(0.12f * RY)), rgb(219,146,85));
      const uint16_t pep = l.color == tabyColors[1] ? rgb(146,0,0) : rgb(219,36,36);
      const float spots[3][2] = {{-0.45f, -0.98f}, {0.42f, -1.0f}, {0.05f, -0.32f}};
      for (const auto &p : spots)
        gameFrame.fillCircle(cx + (int)(p[0] * RX), by + (int)(p[1] * RY), max(2, (int)(0.12f * R)), pep);
      break;
    }
    case CHICK:
      gameFrame.fillEllipse(cx + (int)(0.62f * RX), by - (int)(0.5f * RY), max(3, (int)(0.2f * RX)), max(3, (int)(0.26f * RY)), shade);
      for (int i = -1; i <= 1; i += 2)
        gameFrame.drawFastHLine(cx + i * (int)(0.3f * RX) - 3, by - 1, 7, rgb(255,109,0));
      break;
  }
  drawFace(fx, fy, R, sx, l.face, squished, l.shape);
  // Shine and goo overlays.
  if (l.mod == SHINY || l.mod == STICKY) {
    const int hx = cx - (int)(0.42f * RX), hy = fy - (int)(0.38f * R);
    gameFrame.fillEllipse(hx, hy, max(3, (int)(0.2f * RX)), max(2, (int)(0.09f * RY)), TFT_WHITE);
    gameFrame.fillCircle(hx + (int)(0.3f * RX), hy - 1, max(1, (int)(0.05f * R)), TFT_WHITE);
  }
  if (l.mod == STICKY) {
    // Gooey drips sliding off the bottom edge.
    for (int i = -1; i <= 1; ++i) {
      const int dx = cx + i * (int)(0.42f * RX), len = (int)((0.16f + 0.06f * ((now / 400 + i + 1) % 3)) * R);
      gameFrame.fillRoundRect(dx - max(2, (int)(0.06f * R)), by - (int)(0.32f * RY), max(4, (int)(0.12f * R)), (int)(0.2f * RY) + len, max(2, (int)(0.06f * R)), c);
      gameFrame.fillCircle(dx, by - (int)(0.12f * RY) + len, max(2, (int)(0.08f * R)), c);
      gameFrame.drawPixel(dx - 1, by - (int)(0.2f * RY), TFT_WHITE);
    }
  }
}

// ---- Screens --------------------------------------------------------------
void pastelBackdrop(uint32_t now) {
  for (int i = 0; i < 10; ++i)
    gameFrame.fillRect(0, i * 24, 320, 24, GameGfx::mix565(rgb(255,219,255), rgb(219,182,255), i, 9));
  for (int i = 0; i < 14; ++i) {
    const int x = (i * 83 + 17) % 320, y = 40 + (i * 47) % 190;
    if ((now / 150 + i * 5) % 16 < 3) {
      gameFrame.drawFastHLine(x - 3, y, 7, TFT_WHITE);
      gameFrame.drawFastVLine(x, y - 3, 7, TFT_WHITE);
    } else gameFrame.drawPixel(x, y, TFT_WHITE);
  }
}

void header(const char *title) {
  gameFrame.fillRect(0, 0, 320, 34, rgb(255,146,219));
  gameFrame.drawFastHLine(0, 34, 320, rgb(219,73,182));
  gameFrame.fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 10, TFT_WHITE);
  gameFrame.drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 10, rgb(219,73,182));
  gameFrame.setTextColor(rgb(182,36,146));
  gameFrame.drawCentreString("BACK", exitButton.x + exitButton.w / 2, exitButton.y + 5, 2);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString(title, 190, 9, 2);
  gameFrame.drawCentreString(title, 191, 9, 2);
}

void pill(const char *text, int y) {
  const int w = gameFrame.textWidth(text, 2) + 28, x = (320 - w) / 2;
  gameFrame.fillRoundRect(x, y, w, 22, 11, TFT_WHITE);
  gameFrame.drawRoundRect(x, y, w, 22, 11, rgb(219,73,182));
  gameFrame.setTextColor(rgb(182,36,146));
  gameFrame.drawCentreString(text, 160, y + 3, 2);
}

void renderPick(uint32_t now) {
  pastelBackdrop(now);
  header("SQUISHY MAKER");
  const Step &s = stepAt(step);
  pill(s.prompt, 40);
  for (int i = 0; i < 3; ++i) {
    const TouchButton &b = cards[i];
    gameFrame.fillRoundRect(b.x + 2, b.y + 3, b.w, b.h, 14, rgb(219,146,219));
    gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 14, TFT_WHITE);
    gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 14, rgb(255,146,219));
    int preview[4] = {choice[0], choice[1], choice[2], choice[3]};
    preview[step] = i;
    for (int k = step + 1; k < 4; ++k) preview[k] = -1;
    drawSquishy(lookFor(preview), b.x + b.w / 2, b.y + 88, 26, 1, 1, now, false);
    gameFrame.setTextColor(rgb(146,36,146));
    gameFrame.drawCentreString(s.names[i], b.x + b.w / 2, b.y + 100, 2);
  }
  // Progress dots: one per step.
  const int n = stepCount(), x0 = 160 - (n - 1) * 9;
  for (int i = 0; i < n; ++i)
    gameFrame.fillCircle(x0 + i * 18, 214, i == step ? 5 : 3, i <= step ? rgb(219,73,182) : TFT_WHITE);
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

void drawHeart(int x, int y, int r, uint16_t c) {
  gameFrame.fillCircle(x - r, y, r, c);
  gameFrame.fillCircle(x + r, y, r, c);
  gameFrame.fillTriangle(x - 2 * r, y + 1, x + 2 * r, y + 1, x, y + 2 * r + 2, c);
}

const char *fullName(char *out, size_t size) {
  const int k = choice[0];
  if (k == DUMPLING)
    snprintf(out, size, "%s %s DUMPLING", dumplingSteps[1].names[choice[2]], dumplingSteps[0].names[choice[1]]);
  else {
    const Step *steps = k == NDOH ? ndohSteps : tabySteps;
    snprintf(out, size, "%s %s %s %s", steps[2].names[choice[3]], steps[1].names[choice[2]],
             steps[0].names[choice[1]], kindStep.names[k]);
  }
  return out;
}

void renderAdmire(uint32_t now) {
  pastelBackdrop(now);
  header("MY SQUISHY");
  char name[48];
  pill(fullName(name, sizeof(name)), 40);
  // Soft spotlight and a cushion to sit on.
  gameFrame.fillEllipse(160, 150, 96, 64, rgb(255,255,255));
  gameFrame.fillEllipse(160, 204, 92, 14, rgb(255,146,219));
  gameFrame.fillEllipse(160, 200, 86, 11, rgb(255,182,219));
  const float sx = 1 + 0.6f * squash, sy = 1 - squash;
  drawSquishy(lookFor(choice), 160, 200, 54, sx, sy, now, squash > 0.12f);
  for (const Heart &h : hearts)
    if (h.life > 0) drawHeart((int)h.x, (int)h.y, 3 + (int)(h.life * 2), h.life > 0.4f ? rgb(255,73,146) : rgb(255,146,219));
  gameFrame.setTextColor(rgb(182,36,146));
  gameFrame.drawString("TAP TO SQUISH!", 10, 214, 2);
  const TouchButton &b = againButton;
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 14, rgb(219,73,182));
  gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 14, TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString("MAKE ANOTHER", b.x + b.w / 2, b.y + 6, 2);
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

void render() {
  const uint32_t now = millis();
  lastFrame = now;
  if (stage == PICK) renderPick(now);
  else renderAdmire(now);
}

int navItems(const TouchButton **items) {
  if (stage == PICK) { for (int i = 0; i < 3; ++i) items[i] = &cards[i]; return 3; }
  items[0] = &squishTarget; items[1] = &againButton;
  return 2;
}

void restart() {
  stage = PICK; step = 0;
  choice[0] = 0; choice[1] = choice[2] = choice[3] = -1;
}

void squish() {
  squashVel = 7.5f;
  GameAudio::play(GameAudio::Pop);
  for (int i = 0; i < 3; ++i)
    for (Heart &h : hearts)
      if (h.life <= 0) { h = {120.0f + random(0, 80), 140.0f, -(float)random(40, 80), 1.0f}; break; }
}

void choose(int i) {
  choice[step] = i;
  for (int k = step + 1; k < 4; ++k) choice[k] = -1;
  if (step + 1 < stepCount()) { ++step; GameAudio::play(GameAudio::Click); }
  else {
    stage = ADMIRE; squash = squashVel = 0;
    for (Heart &h : hearts) h.life = 0;
    GameAudio::play(GameAudio::Happy);
  }
}

void update(uint32_t now, bool tapped, uint16_t x, uint16_t y) {
  if (tapped && exitButton.contains(x, y)) {
    GameAudio::play(GameAudio::Click);
    if (stage == ADMIRE) stage = PICK;
    else if (step > 0) --step;
    else { drawGameList(); return; }
    render();
    return;
  }
  if (tapped) {
    if (stage == PICK) {
      for (int i = 0; i < 3; ++i) if (cards[i].contains(x, y)) { choose(i); render(); return; }
    } else if (againButton.contains(x, y)) { restart(); GameAudio::play(GameAudio::Click); render(); return; }
    else if (squishTarget.contains(x, y)) squish();
  }
  if (now - lastFrame < 50) return;
  const float dt = min(0.08f, (now - lastFrame) / 1000.0f);
  // Damped spring: squishes flat, bounces back and wobbles to rest.
  squashVel += (-170 * squash - 7 * squashVel) * dt;
  squash = constrain(squash + squashVel * dt, -0.3f, 0.55f);
  for (Heart &h : hearts) if (h.life > 0) { h.y += h.vy * dt; h.life -= dt * 0.9f; }
  render();
}
} // namespace Squishy

void beginSquishyMaker() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  gameState = SQUISHY_MAKER;
  Squishy::restart();
  Squishy::render();
}
