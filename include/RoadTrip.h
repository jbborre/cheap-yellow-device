#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
#include <math.h>

// Road Trip Rally: an arcade cross-country race over five stages. Beat the
// clock to each checkpoint, dodge slow traffic and pass five rival racers.
// The road is a segment-based pseudo-3D track with curves and hills, drawn
// far to near so hills hide whatever is behind them.
namespace RoadTrip {
using GameGfx::rgb;
enum Stage { GARAGE, RACING, STAGE_DONE, TIME_UP, TRIP_DONE };
enum Style { ROADSTER, MUSCLE, OFFROAD, SEDAN, VAN, TRUCK };
enum Prop { PALM, UMBRELLA, CACTUS, ROCK, PINE, SNOWMAN, BARN, HAYBALE, STREETLIGHT, NEON };

// World units: a segment is 200 long and the road is 2000 wide.
constexpr float segLen = 200, roadW = 1000, camH = 1000, depth = 0.84f;
constexpr float playerZ = camH * depth;       // The car sits this far ahead of the camera.
constexpr float maxSpeed = segLen * 45;       // Fastest car's top speed, units per second.
constexpr int drawDist = 60, horizonY = 120;
constexpr int stageCount = 5, rivalCount = 5, trafficCount = 6, carCount = rivalCount + trafficCount;
constexpr float startTime = 25, checkpointBonus = 20;

struct Ride { const char *name; float top, accel, grip, offroad; Style style; uint16_t color; };
const Ride rides[3] = {
  {"ROADSTER",   1.00f, maxSpeed / 5.5f, 0.85f, 0.35f, ROADSTER, rgb(255,36,36)},
  {"MUSCLE CAR", 0.95f, maxSpeed / 4.0f, 0.75f, 0.35f, MUSCLE,   rgb(36,109,255)},
  {"4X4",        0.90f, maxSpeed / 5.0f, 1.00f, 0.65f, OFFROAD,  rgb(73,182,73)},
};

struct Section { uint16_t len; int8_t curve, hill; };  // hill: height change in hundreds of units.
const Section coast[] = {{60,0,0},{150,2,0},{120,0,20},{160,-3,-20},{140,0,0},{180,3,30},
                         {150,-2,-30},{120,0,0},{200,-3,0},{160,2,20},{140,0,-20},{150,0,0}};
const Section canyon[] = {{60,0,0},{140,3,0},{140,-4,20},{120,0,-10},{180,4,30},{160,-3,-40},
                          {140,0,0},{160,-4,10},{150,3,-10},{180,0,40},{140,-3,-40},{120,0,0}};
const Section peaks[] = {{60,0,0},{150,-3,40},{140,3,40},{160,0,-50},{150,4,30},{140,-4,-40},
                         {170,0,60},{150,-3,-30},{150,3,-30},{140,0,20},{160,-4,-20},{120,0,0}};
const Section farm[] = {{60,0,0},{160,0,30},{140,2,-30},{150,0,30},{140,-2,-30},{170,3,20},
                        {150,0,-20},{140,-3,30},{160,0,-30},{150,2,20},{150,0,-20},{120,0,0}};
const Section neon[] = {{60,0,0},{140,4,0},{120,-4,0},{150,0,20},{140,5,-20},{140,-5,0},
                        {160,0,30},{140,4,-30},{150,-3,0},{160,3,0},{140,-4,0},{150,0,0}};
constexpr int sectionsPerStage = 12;

struct Theme {
  const char *name; const Section *sections;
  uint16_t skyTop, skyBottom, groundA, groundB, rumbleA, rumbleB, road;
  Prop propA, propB;
};
const Theme themes[stageCount] = {
  {"SUNNY COAST",   coast,  rgb(73,146,255), rgb(182,219,255), rgb(255,219,146), rgb(255,219,109), rgb(255,36,36), TFT_WHITE, rgb(109,109,109), PALM, UMBRELLA},
  {"CACTUS CANYON", canyon, rgb(255,146,73), rgb(255,219,146), rgb(219,146,73),  rgb(219,109,73),  rgb(255,255,255), rgb(146,36,0), rgb(109,73,73), CACTUS, ROCK},
  {"PINE PEAKS",    peaks,  rgb(109,182,255), rgb(219,255,255), rgb(73,182,73),   rgb(73,146,73),   rgb(255,36,36), TFT_WHITE, rgb(109,109,109), PINE, SNOWMAN},
  {"FARM FIELDS",   farm,   rgb(255,182,109), rgb(255,255,170), rgb(146,219,73),  rgb(182,219,73),  rgb(255,255,255), rgb(219,0,0), rgb(109,109,109), BARN, HAYBALE},
  {"NEON NIGHT",    neon,   rgb(0,0,36),     rgb(73,0,109),    rgb(36,36,73),    rgb(36,36,36),    rgb(255,0,255), rgb(0,255,255), rgb(73,73,73), STREETLIGHT, NEON},
};

const TouchButton exitButton = {5, 3, 48, 20};
const TouchButton garageCards[3] = {{5,38,100,198},{110,38,100,198},{215,38,100,198}};
const TouchButton nextButton = {40, 164, 112, 34};
const TouchButton leaveButton = {168, 164, 112, 34};

struct Car { float z, x, targetX, speed; uint16_t color; uint8_t style; bool rival; };

Stage stage = GARAGE;
int ride = 0, level = 0;
int places[stageCount];
float position = 0, playerX = 0, speed = 0, steer = 0;
float timeLeft = 0, countdown = 0, banner = 0, endTimer = 0, shake = 0, bgOffset = 0;
const char *bannerText = "";
int nextCheckpoint = 1, place = rivalCount + 1;
bool finished = false, braking = false;
uint32_t lastFrame = 0;
Car cars[carCount];

// ---- Track ---------------------------------------------------------------
int sectionStart[sectionsPerStage + 1];
float sectionY[sectionsPerStage + 1];
int stageLen = 0;

void loadTrack() {
  const Section *s = themes[level].sections;
  int at = 0; float y = 0;
  for (int i = 0; i < sectionsPerStage; ++i) {
    sectionStart[i] = at; sectionY[i] = y;
    at += s[i].len; y += s[i].hill * 100.0f;
  }
  sectionStart[sectionsPerStage] = stageLen = at;
  sectionY[sectionsPerStage] = y;
}

int sectionAt(int idx) {
  static int last = 0;
  if (last >= sectionsPerStage || idx < sectionStart[last]) last = 0;
  while (last + 1 < sectionsPerStage && idx >= sectionStart[last + 1]) ++last;
  return last;
}
float sectionT(int s, int idx) { return (idx - sectionStart[s]) / (float)themes[level].sections[s].len; }

// Bends ease in and out so the road never kinks.
float curveAt(int idx) {
  if (idx < 0 || idx >= stageLen) return 0;
  const int s = sectionAt(idx);
  const float t = sectionT(s, idx);
  const float ease = t < 0.2f ? t / 0.2f : t > 0.8f ? (1 - t) / 0.2f : 1;
  return themes[level].sections[s].curve * ease;
}
float heightAt(int idx) {
  if (idx <= 0) return 0;
  if (idx >= stageLen) return sectionY[sectionsPerStage];
  const int s = sectionAt(idx);
  const float t = sectionT(s, idx);
  return sectionY[s] + (sectionY[s + 1] - sectionY[s]) * (1 - cosf(PI * t)) / 2;
}
float finishZ() { return (stageLen - 2) * segLen; }
float checkpointZ(int k) { return stageLen * segLen * k / 3; }
float playerDist() { return position + playerZ; }

uint32_t mix(uint32_t a, uint32_t b) {
  uint32_t h = a * 374761393u + b * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// ---- Drawing helpers --------------------------------------------------------
// Centered trapezoid from a far edge (top) to a near edge (bottom).
void trap(float xb, float yb, float wb, float xt, float yt, float wt, uint16_t c) {
  const int y0 = max(0, (int)ceilf(yt)), y1 = min(239, (int)yb);
  const float span = max(1.0f, yb - yt);
  for (int y = y0; y <= y1; ++y) {
    const float t = (y - yt) / span;
    const float cx = xt + (xb - xt) * t, hw = wt + (wb - wt) * t;
    const int l = (int)constrain(cx - hw, -2.0f, 322.0f), r = (int)constrain(cx + hw, -2.0f, 322.0f);
    if (r > l) gameFrame.drawFastHLine(l, y, r - l, c);
  }
}

void carBack(int x, int bottom, int w, uint16_t color, int style, bool brake, int lean = 0, bool racer = false) {
  if (w < 3) return;
  if (w < 8) { gameFrame.fillRect(x - w / 2, bottom - w / 2, w, w / 2, color); return; }
  const uint16_t shade = GameGfx::darker(color), shine = GameGfx::lighter(color);
  const int h = style == TRUCK ? w : style == VAN ? w * 4 / 5 : style == OFFROAD ? w * 7 / 10 : w * 11 / 20;
  GameGfx::darkenEllipse(x, bottom, w * 3 / 5, max(1, w / 10));
  const int tw = max(2, w / 6), th = max(2, h / 3);
  gameFrame.fillRoundRect(x - w / 2, bottom - th, tw, th, max(1, tw / 3), rgb(36,36,36));
  gameFrame.fillRoundRect(x + w / 2 - tw, bottom - th, tw, th, max(1, tw / 3), rgb(36,36,36));
  int lightY;
  if (style == VAN || style == TRUCK) {
    const int bw = style == TRUCK ? w * 11 / 10 : w;
    gameFrame.fillRoundRect(x - bw / 2, bottom - h, bw, h * 7 / 8, max(1, w / 16), color);
    gameFrame.drawFastVLine(x, bottom - h + 2, h * 7 / 8 - 4, shade);  // Rear doors.
    if (style == VAN) {
      gameFrame.fillRect(x - w / 2 + w / 10, bottom - h + h / 8, w * 3 / 10, h / 5, rgb(36,73,109));
      gameFrame.fillRect(x + w / 10, bottom - h + h / 8, w * 3 / 10, h / 5, rgb(36,73,109));
    } else {
      gameFrame.fillRect(x - bw / 2, bottom - h + h / 3, bw, max(1, h / 8), shine);  // Trailer stripe.
    }
    lightY = bottom - h / 4;
  } else {
    const bool open = style == ROADSTER;
    const int bodyTop = bottom - h * 3 / 5, cabW = style == OFFROAD ? w * 17 / 20 : w * 7 / 10;
    const int cx = x + lean * w / 18;
    if (open) {
      // Convertible: headrests and the driver's head peeking over the seat.
      gameFrame.fillRoundRect(cx - w / 4 - w / 14, bodyTop - h / 4, w / 7, h / 4 + 2, max(1, w / 20), rgb(36,36,36));
      gameFrame.fillRoundRect(cx + w / 4 - w / 14, bodyTop - h / 4, w / 7, h / 4 + 2, max(1, w / 20), rgb(36,36,36));
      gameFrame.fillCircle(cx - w / 4, bodyTop - h / 3, max(2, w / 11), rgb(146,73,0));
    } else {
      gameFrame.fillRoundRect(cx - cabW / 2, bottom - h, cabW, h / 2, max(1, w / 12), shade);
      gameFrame.fillRoundRect(cx - cabW / 2 + max(1, w / 16), bottom - h + max(1, h / 10), cabW - 2 * max(1, w / 16), h / 3, max(1, w / 16), rgb(36,73,109));
      gameFrame.drawLine(cx - cabW / 5, bottom - h + h / 3, cx - cabW / 10, bottom - h + h / 8, rgb(146,182,255));
    }
    gameFrame.fillRoundRect(x - w / 2, bodyTop, w, h * 3 / 5 - h / 8, max(1, w / 12), color);
    gameFrame.drawFastHLine(x - w / 2 + 2, bodyTop + 1, w - 4, shine);
    if (style == MUSCLE || racer) {
      // Racing stripes and a spoiler.
      gameFrame.fillRect(x - max(2, w / 12), bodyTop, max(1, w / 18), h * 2 / 5, TFT_WHITE);
      gameFrame.fillRect(x + max(2, w / 12) - max(1, w / 18), bodyTop, max(1, w / 18), h * 2 / 5, TFT_WHITE);
      gameFrame.fillRect(x - w / 2 + 2, bodyTop - max(2, h / 10), w - 4, max(2, h / 12), rgb(36,36,36));
    }
    if (style == OFFROAD) gameFrame.fillCircle(x, bodyTop + h / 5, max(2, w / 8), rgb(36,36,36));  // Spare tire.
    lightY = bodyTop + h / 8;
  }
  const int lw = max(2, w / 6), lh = max(1, h / 8);
  const uint16_t lamp = brake ? rgb(255,73,85) : rgb(182,0,0);
  gameFrame.fillRect(x - w / 2 + 2, lightY, lw, lh, lamp);
  gameFrame.fillRect(x + w / 2 - 2 - lw, lightY, lw, lh, lamp);
  if (brake && w > 30) {
    GameGfx::glow(x - w / 2 + 2 + lw / 2, lightY, lw + 3, TFT_RED);
    GameGfx::glow(x + w / 2 - 2 - lw / 2, lightY, lw + 3, TFT_RED);
  }
  gameFrame.fillRect(x - w / 2 + 1, bottom - max(2, h / 8), w - 2, max(2, h / 8), rgb(36,36,36));
  if (w >= 20) gameFrame.fillRect(x - w / 8, bottom - max(2, h / 8) - max(2, h / 9), w / 4, max(2, h / 9), TFT_WHITE);
}

// Roadside scenery, sized by u: half the road's width on screen at that spot.
void prop(Prop p, int x, int b, float u, uint32_t now) {
  const int s = (int)u;
  if (s < 2) return;
  switch (p) {
    case PALM: {
      int tx = x, ty = b;
      for (int i = 0; i < 4; ++i) {  // Curved trunk.
        const int nx = tx + s / 14, ny = ty - s * 3 / 8;
        gameFrame.drawWideLine(tx, ty, nx, ny, max(2.0f, u / 9), rgb(146,109,36));
        tx = nx; ty = ny;
      }
      const float a[5] = {-2.6f, -2.0f, -1.2f, -0.6f, -0.1f};
      for (float ang : a)
        gameFrame.drawWideLine(tx, ty, tx + (int)(cosf(ang) * s * 0.7f), ty + (int)(sinf(ang) * s * 0.35f) + s / 6, max(2.0f, u / 10), rgb(36,146,36));
      gameFrame.fillCircle(tx, ty + s / 16, max(1, s / 14), rgb(109,73,0));
      break;
    }
    case UMBRELLA:
      gameFrame.drawFastVLine(x, b - s * 3 / 5, s * 3 / 5, rgb(219,219,219));
      // Striped canopy: alternating pink and white wedges.
      for (int k = 0; k < 4; ++k)
        gameFrame.fillTriangle(x, b - s * 9 / 10, x - s / 3 + k * s / 6, b - s * 3 / 5, x - s / 3 + (k + 1) * s / 6, b - s * 3 / 5,
                               k % 2 ? TFT_WHITE : rgb(255,73,146));
      break;
    case CACTUS: {
      const uint16_t g = rgb(73,146,36);
      const int tw = max(2, s / 6);
      gameFrame.fillRoundRect(x - tw / 2, b - s, tw, s, tw / 2, g);
      gameFrame.fillRoundRect(x - tw * 2, b - s * 3 / 5, tw * 3 / 2 + 1, max(2, tw * 2 / 3), tw / 3, g);
      gameFrame.fillRoundRect(x - tw * 2, b - s * 4 / 5, max(2, tw * 2 / 3), s / 4, tw / 3, g);
      gameFrame.fillRoundRect(x + tw / 2, b - s / 2, tw * 3 / 2, max(2, tw * 2 / 3), tw / 3, g);
      gameFrame.fillRoundRect(x + tw * 3 / 2, b - s * 3 / 4, max(2, tw * 2 / 3), s / 4 + 1, tw / 3, g);
      break;
    }
    case ROCK:
      gameFrame.fillEllipse(x, b - s / 6, s / 3, s / 6 + 1, rgb(146,73,36));
      gameFrame.fillEllipse(x - s / 10, b - s / 4, s / 6, s / 12 + 1, rgb(182,109,73));
      break;
    case PINE:
      gameFrame.fillRect(x - max(1, s / 16), b - s / 5, max(2, s / 8), s / 5, rgb(109,73,0));
      for (int i = 0; i < 3; ++i) {
        const int top = b - s + i * s / 4, half = s / 5 + i * s / 9;
        gameFrame.fillTriangle(x, top, x - half, top + s * 2 / 5, x + half, top + s * 2 / 5, rgb(0,109,36));
        gameFrame.fillTriangle(x, top, x - half / 3, top + s / 8, x + half / 3, top + s / 8, TFT_WHITE);  // Snowy tip.
      }
      break;
    case SNOWMAN:
      gameFrame.fillCircle(x, b - s / 5, s / 5, TFT_WHITE);
      gameFrame.fillCircle(x, b - s / 2, s / 7, TFT_WHITE);
      gameFrame.fillRect(x - s / 9, b - s * 4 / 5, s * 2 / 9, s / 6, rgb(36,36,36));  // Top hat.
      gameFrame.fillRect(x - s / 6, b - s * 2 / 3, s / 3, max(1, s / 30), rgb(36,36,36));
      if (s > 20) {
        gameFrame.fillCircle(x - s / 20, b - s / 2 - s / 30, 1, TFT_BLACK);
        gameFrame.fillCircle(x + s / 20, b - s / 2 - s / 30, 1, TFT_BLACK);
        gameFrame.fillTriangle(x, b - s / 2, x + s / 8, b - s / 2 + s / 40, x, b - s / 2 + s / 20, rgb(255,146,0));
      }
      break;
    case BARN: {
      const int w = s, h = s * 3 / 5;
      gameFrame.fillRect(x - w / 2, b - h, w, h, rgb(182,36,36));
      gameFrame.fillTriangle(x - w / 2 - s / 12, b - h, x + w / 2 + s / 12, b - h, x, b - h - s * 2 / 5, rgb(146,0,0));
      gameFrame.fillRect(x - w / 6, b - h / 2, w / 3, h / 2, TFT_WHITE);
      gameFrame.drawLine(x - w / 6, b - h / 2, x + w / 6, b - 1, rgb(182,36,36));
      gameFrame.drawLine(x + w / 6, b - h / 2, x - w / 6, b - 1, rgb(182,36,36));
      break;
    }
    case HAYBALE:
      gameFrame.fillRoundRect(x - s / 4, b - s / 4, s / 2, s / 4, max(1, s / 14), rgb(219,182,73));
      gameFrame.drawFastHLine(x - s / 4, b - s / 6, s / 2, rgb(182,146,36));
      break;
    case STREETLIGHT: {
      gameFrame.drawWideLine(x, b, x, b - s * 6 / 5, max(1.0f, u / 20), rgb(109,109,109));
      GameGfx::glow(x, b - s * 6 / 5, max(3, s / 3), rgb(255,219,109));
      gameFrame.fillCircle(x, b - s * 6 / 5, max(1, s / 14), rgb(255,255,182));
      break;
    }
    case NEON: {
      const uint16_t glowC[3] = {rgb(255,0,255), rgb(0,255,255), rgb(255,255,0)};
      const uint16_t c = glowC[(x / 40 + now / 400) % 3];
      const int w = s * 4 / 5, h = s * 2 / 5;
      gameFrame.fillRect(x - max(1, s / 30), b - s / 2, max(2, s / 15), s / 2, rgb(73,73,73));
      gameFrame.fillRoundRect(x - w / 2, b - s / 2 - h, w, h, max(1, s / 12), rgb(36,0,73));
      gameFrame.drawRoundRect(x - w / 2, b - s / 2 - h, w, h, max(1, s / 12), c);
      if (s > 50) {
        gameFrame.setTextColor(c);
        gameFrame.drawCentreString((x / 40) % 2 ? "DINER" : "MOTEL", x, b - s / 2 - h / 2 - 7, 2);
      } else gameFrame.drawFastHLine(x - w / 3, b - s / 2 - h / 2, w * 2 / 3, c);
      break;
    }
  }
}

// Arch over the road at checkpoints, checkered at the finish.
void arch(float x, float y, float w, bool finishLine) {
  if (w < 4) return;
  const int post = max(2, (int)(w / 18)), top = (int)(y - w * 0.95f), bh = max(3, (int)(w * 0.2f));
  const int l = (int)(x - w * 1.2f), r = (int)(x + w * 1.2f);
  gameFrame.fillRect(l - post / 2, top, post, (int)y - top, rgb(219,219,219));
  gameFrame.fillRect(r - post / 2, top, post, (int)y - top, rgb(219,219,219));
  if (finishLine) {
    const int cell = max(2, bh / 2);
    for (int cx = l, i = 0; cx < r; cx += cell, ++i)
      for (int row = 0; row < 2; ++row)
        gameFrame.fillRect(cx, top + row * cell, min(cell, r - cx), cell, (i + row) % 2 ? TFT_BLACK : TFT_WHITE);
  } else {
    gameFrame.fillRect(l, top, r - l, bh, rgb(255,182,0));
    if (w > 70) {
      gameFrame.setTextColor(rgb(146,36,0));
      gameFrame.drawCentreString("CHECKPOINT", (int)x, top + bh / 2 - 7, 2);
    }
  }
}

// ---- Backgrounds ------------------------------------------------------------
void cloud(int x, int y, int s) {
  gameFrame.fillCircle(x, y, s, TFT_WHITE);
  gameFrame.fillCircle(x + s, y + s / 3, s * 2 / 3, TFT_WHITE);
  gameFrame.fillCircle(x - s, y + s / 3, s * 2 / 3, TFT_WHITE);
}
int wrap(int v, int span) { return ((v % span) + span) % span; }

void background(uint32_t now) {
  const Theme &t = themes[level];
  GameGfx::gradient(0, 0, 320, horizonY, t.skyTop, t.skyBottom);
  gameFrame.fillRect(0, horizonY, 320, 240 - horizonY, t.groundA);
  const int off = (int)bgOffset;
  if (level != 4)
    for (int i = 0; i < 3; ++i) cloud(wrap(i * 140 - off / 4, 440) - 60, 44 + i * 12, 8 + i * 2);
  switch (level) {
    case 0:  // Sun over the ocean, with little islands.
      GameGfx::glow(250 - off / 30, 62, 30, rgb(255,255,146));
      gameFrame.fillCircle(250 - off / 30, 62, 14, rgb(255,255,109));
      gameFrame.fillRect(0, horizonY - 14, 320, 14, rgb(36,109,219));
      for (int i = 0; i < 3; ++i) gameFrame.fillEllipse(wrap(i * 150 - off / 2, 450) - 60, horizonY - 14, 28, 6, rgb(73,146,73));
      for (int y = horizonY - 11; y < horizonY; y += 4)
        for (int x = -wrap(off + (int)(now / 50), 40); x < 320; x += 40) gameFrame.drawFastHLine(x + (y % 8) * 2, y, 8, TFT_WHITE);
      break;
    case 1:  // Red mesas.
      for (int i = 0; i < 5; ++i) {
        const int x = wrap(i * 90 - off / 2, 450) - 70, w = 50 + (i % 3) * 20, h = 26 + (i % 2) * 18;
        gameFrame.fillRect(x, horizonY - h, w, h, rgb(182,73,36));
        gameFrame.fillTriangle(x - 12, horizonY, x, horizonY - h, x, horizonY, rgb(182,73,36));
        gameFrame.fillTriangle(x + w + 12, horizonY, x + w, horizonY - h, x + w, horizonY, rgb(146,36,36));
        gameFrame.fillRect(x, horizonY - h, w, 3, rgb(219,109,73));
      }
      break;
    case 2:  // Snowy mountains in two layers.
      for (int layer = 0; layer < 2; ++layer)
        for (int i = 0; i < 5; ++i) {
          const int x = wrap(i * 100 - off / (3 - layer), 500) - 90, h = layer ? 40 + (i % 2) * 14 : 62 + (i % 3) * 10;
          const uint16_t c = layer ? rgb(73,109,146) : rgb(109,146,182);
          gameFrame.fillTriangle(x - 70, horizonY, x + 70, horizonY, x, horizonY - h, c);
          gameFrame.fillTriangle(x - 18, horizonY - h + h / 4, x + 18, horizonY - h + h / 4, x, horizonY - h, TFT_WHITE);
        }
      break;
    case 3:  // Rolling hills and a windmill.
      for (int i = 0; i < 4; ++i) gameFrame.fillEllipse(wrap(i * 120 - off / 2, 480) - 80, horizonY, 90, 24 + (i % 2) * 10, rgb(73,146,36));
      {
        const int wx = wrap(200 - off / 2, 480) - 80;
        gameFrame.fillTriangle(wx - 5, horizonY - 20, wx + 5, horizonY - 20, wx, horizonY - 54, rgb(219,219,219));
        const float a = now / 400.0f;
        for (int k = 0; k < 4; ++k)
          gameFrame.drawWideLine(wx, horizonY - 52, wx + (int)(cosf(a + k * PI / 2) * 16), horizonY - 52 + (int)(sinf(a + k * PI / 2) * 16), 3, TFT_WHITE);
      }
      break;
    default:  // City lights at night.
      for (int i = 0; i < 30; ++i) gameFrame.drawPixel((i * 97 + 13) % 320, (i * 37) % 90 + 4, (now / 300 + i) % 9 ? TFT_WHITE : rgb(146,146,255));
      gameFrame.fillCircle(60, 40, 10, rgb(255,255,219));
      for (int i = 0; i < 9; ++i) {
        const int x = wrap(i * 44 - off / 2, 400) - 40, h = 30 + (int)(mix(i, 7) % 50), w = 38;
        gameFrame.fillRect(x, horizonY - h, w, h, rgb(36,0,73));
        for (int wy = horizonY - h + 4; wy < horizonY - 4; wy += 7)
          for (int wx = x + 4; wx < x + w - 4; wx += 7)
            if (mix(i * 97 + wx - x, wy) % 3 == 0) gameFrame.fillRect(wx, wy, 3, 3, rgb(255,219,109));
      }
  }
}

// ---- Rendering --------------------------------------------------------------
struct Proj { float x1, y1, w1, x2, y2, w2; bool ok; } proj[drawDist];
int baseSegment = 0;

void projectRoad() {
  baseSegment = (int)(position / segLen);
  const float basePct = fmodf(position, segLen) / segLen;
  const int pseg = (int)(playerDist() / segLen);
  const float pPct = fmodf(playerDist(), segLen) / segLen;
  const float camY = camH + heightAt(pseg) + (heightAt(pseg + 1) - heightAt(pseg)) * pPct;
  const float camX = playerX * roadW;
  float x = 0, dx = -curveAt(baseSegment) * basePct;
  for (int n = 0; n < drawDist; ++n) {
    const int idx = baseSegment + n;
    const float z1 = idx * segLen - position, z2 = z1 + segLen;
    Proj &p = proj[n];
    p.ok = z1 > 20;
    if (p.ok) {
      const float s1 = depth / z1, s2 = depth / z2;
      p.x1 = 160 + s1 * (x - camX) * 160;
      p.y1 = horizonY - s1 * (heightAt(idx) - camY) * 120;
      p.w1 = s1 * roadW * 160;
      p.x2 = 160 + s2 * (x + dx - camX) * 160;
      p.y2 = horizonY - s2 * (heightAt(idx + 1) - camY) * 120;
      p.w2 = s2 * roadW * 160;
    }
    x += dx; dx += curveAt(idx);
  }
}

void drawSegment(int n, uint32_t now) {
  const Proj &p = proj[n];
  const int idx = baseSegment + n;
  const Theme &t = themes[level];
  if (p.y2 < p.y1) {
    const bool alt = (idx / 3) % 2;
    const int top = max(0, (int)p.y2), bottom = min(240, (int)p.y1 + 1);
    if (bottom > top) gameFrame.fillRect(0, top, 320, bottom - top, alt ? t.groundB : t.groundA);
    trap(p.x1, p.y1, p.w1 * 1.15f, p.x2, p.y2, p.w2 * 1.15f, alt ? t.rumbleB : t.rumbleA);
    trap(p.x1, p.y1, p.w1, p.x2, p.y2, p.w2, t.road);
    if (idx >= 6 && idx <= 7) {
      // Checkered start line.
      for (int k = -4; k < 4; ++k) {
        const float a = p.x1 + p.w1 * k / 4, b = p.x2 + p.w2 * k / 4;
        trap(a + p.w1 / 8, p.y1, p.w1 / 8, b + p.w2 / 8, p.y2, p.w2 / 8, (k + idx) % 2 ? TFT_WHITE : TFT_BLACK);
      }
    } else if (alt) {
      for (int k = -1; k <= 1; k += 2)
        trap(p.x1 + k * p.w1 / 3, p.y1, max(0.5f, p.w1 / 40), p.x2 + k * p.w2 / 3, p.y2, max(0.5f, p.w2 / 40), TFT_WHITE);
    }
  }
  // Scenery on this segment.
  if (p.w1 < 600) {
    const uint32_t h = mix(idx, level * 31 + 7);
    if (level == 4 && idx % 8 == 0) {
      prop(STREETLIGHT, (int)(p.x1 - p.w1 * 1.35f), (int)p.y1, p.w1, now);
      prop(STREETLIGHT, (int)(p.x1 + p.w1 * 1.35f), (int)p.y1, p.w1, now);
    } else if (idx % 3 == 0 && h % 4 != 0) {
      const Prop kind = (h >> 8) % 3 == 0 ? t.propB : t.propA;
      const float side = (h >> 4) & 1 ? -1 : 1;
      const float spread = kind == BARN ? 2.2f : 1.4f;
      prop(kind, (int)(p.x1 + side * p.w1 * (spread + ((h >> 12) % 60) / 100.0f)), (int)p.y1, p.w1, now);
    }
  }
  // Checkpoints and the finish arch.
  for (int k = 1; k <= 3; ++k) {
    const int at = k == 3 ? (int)(finishZ() / segLen) : (int)(checkpointZ(k) / segLen);
    if (idx == at) arch(p.x1, p.y1, p.w1, k == 3);
  }
  // Cars on this segment, positioned within it.
  for (const Car &c : cars) {
    const int seg = (int)(c.z / segLen);
    if (seg != idx) continue;
    const float f = fmodf(c.z, segLen) / segLen;
    const float w = p.w1 + (p.w2 - p.w1) * f;
    const float cx = p.x1 + (p.x2 - p.x1) * f + c.x * w;
    const float cy = p.y1 + (p.y2 - p.y1) * f;
    carBack((int)cx, (int)cy, (int)(w * 0.5f), c.color, c.style, false, 0, c.rival);
  }
}

const char *ordinal(int n) {
  static const char *const names[] = {"1ST", "2ND", "3RD", "4TH", "5TH", "6TH"};
  return names[constrain(n, 1, 6) - 1];
}

void header() {
  GameGfx::darkenRect(0, 0, 320, 26);
  GameGfx::darkenRect(0, 0, 320, 26);
  gameFrame.fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, rgb(0,0,85));
  gameFrame.drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString("BACK", exitButton.x + exitButton.w / 2, exitButton.y + 3, 2);
}

void hud(uint32_t now) {
  header();
  char text[24];
  snprintf(text, sizeof(text), "STAGE %d", level + 1);
  gameFrame.setTextColor(rgb(255,219,85));
  gameFrame.drawString(text, 58, 3, 1);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawString(themes[level].name, 58, 13, 1);
  const bool low = timeLeft < 10 && !finished;
  snprintf(text, sizeof(text), "%d", (int)ceilf(max(0.0f, timeLeft)));
  gameFrame.setTextColor(low && (now / 250) % 2 ? rgb(255,36,36) : rgb(255,255,0));
  gameFrame.drawCentreString(text, 186, 2, 4);
  gameFrame.setTextColor(rgb(182,182,182));
  gameFrame.drawRightString("TIME", 168, 9, 1);
  snprintf(text, sizeof(text), "%s", ordinal(place));
  gameFrame.setTextColor(place == 1 ? rgb(255,219,0) : TFT_WHITE);
  gameFrame.drawRightString(text, 314, 2, 4);
  gameFrame.setTextColor(rgb(182,182,182));
  gameFrame.drawRightString("POS", 314 - gameFrame.textWidth(text, 4) - 4, 9, 1);
  // Trip progress: checkpoints, rivals and you.
  const float total = finishZ();
  gameFrame.drawFastHLine(20, 30, 280, rgb(219,219,219));
  for (int k = 1; k <= 3; ++k) gameFrame.drawFastVLine(20 + 280 * k / 3, 27, 7, k == 3 ? TFT_WHITE : rgb(255,182,0));
  for (const Car &c : cars)
    if (c.rival) gameFrame.fillCircle(20 + (int)(280 * constrain(c.z / total, 0.0f, 1.0f)), 30, 2, c.color);
  gameFrame.fillCircle(20 + (int)(280 * constrain(playerDist() / total, 0.0f, 1.0f)), 30, 3, rides[ride].color);
  gameFrame.drawCircle(20 + (int)(280 * constrain(playerDist() / total, 0.0f, 1.0f)), 30, 3, TFT_WHITE);
  // Speedometer.
  gameFrame.fillRoundRect(248, 206, 68, 30, 8, rgb(0,0,36));
  gameFrame.drawRoundRect(248, 206, 68, 30, 8, rgb(73,73,146));
  snprintf(text, sizeof(text), "%d", (int)(speed / maxSpeed * 140));
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawRightString(text, 292, 210, 4);
  gameFrame.setTextColor(rgb(182,182,255));
  gameFrame.drawString("MPH", 294, 224, 1);
}

void bannerBox(const char *text, uint16_t color) {
  const int w = gameFrame.textWidth(text, 4) + 30;
  gameFrame.fillRoundRect(160 - w / 2, 58, w, 34, 12, rgb(0,0,36));
  gameFrame.drawRoundRect(160 - w / 2, 58, w, 34, 12, color);
  gameFrame.setTextColor(color);
  gameFrame.drawCentreString(text, 160, 63, 4);
}

void panelButtons(const char *a, const char *b) {
  const TouchButton *buttons[2] = {&nextButton, &leaveButton};
  const char *labels[2] = {a, b};
  for (int i = 0; i < 2; ++i) {
    const TouchButton &t = *buttons[i];
    gameFrame.fillRoundRect(t.x, t.y, t.w, t.h, 10, i ? rgb(73,0,109) : rgb(0,109,219));
    gameFrame.drawRoundRect(t.x, t.y, t.w, t.h, 10, TFT_WHITE);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(labels[i], t.x + t.w / 2, t.y + 9, 2);
  }
}

void panel() {
  GameGfx::darkenRect(0, 36, 320, 204);
  gameFrame.fillRoundRect(24, 46, 272, 162, 14, rgb(0,0,73));
  gameFrame.drawRoundRect(24, 46, 272, 162, 14, rgb(255,219,0));
  char line[48];
  if (stage == TIME_UP) {
    gameFrame.setTextColor(rgb(255,73,73));
    gameFrame.drawCentreString("OUT OF TIME!", 160, 60, 4);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString("Reach each checkpoint", 160, 100, 2);
    gameFrame.drawCentreString("before the clock runs out.", 160, 118, 2);
    panelButtons("TRY AGAIN", "EXIT");
  } else if (stage == STAGE_DONE) {
    snprintf(line, sizeof(line), "STAGE %d CLEAR!", level + 1);
    gameFrame.setTextColor(rgb(255,219,0));
    gameFrame.drawCentreString(line, 160, 58, 4);
    snprintf(line, sizeof(line), "YOU PLACED %s", ordinal(places[level]));
    gameFrame.setTextColor(places[level] == 1 ? rgb(255,219,0) : TFT_WHITE);
    gameFrame.drawCentreString(line, 160, 92, 4);
    snprintf(line, sizeof(line), "Next stop: %s", themes[level + 1].name);
    gameFrame.setTextColor(rgb(182,219,255));
    gameFrame.drawCentreString(line, 160, 132, 2);
    panelButtons("NEXT STAGE", "EXIT");
  } else {
    gameFrame.setTextColor(rgb(255,219,0));
    gameFrame.drawCentreString("TRIP COMPLETE!", 160, 56, 4);
    int sum = 0, wins = 0;
    for (int i = 0; i < stageCount; ++i) {
      sum += places[i]; wins += places[i] == 1;
      gameFrame.setTextColor(places[i] == 1 ? rgb(255,219,0) : TFT_WHITE);
      gameFrame.drawCentreString(ordinal(places[i]), 56 + i * 52, 96, 2);
      gameFrame.setTextColor(rgb(146,146,219));
      snprintf(line, sizeof(line), "STAGE %d", i + 1);
      gameFrame.drawCentreString(line, 56 + i * 52, 84, 1);
    }
    const char *verdict = wins == stageCount ? "PERFECT! CHAMPION OF THE ROAD!" : sum <= 10 ? "GREAT DRIVING!" : "THANKS FOR THE RIDE!";
    gameFrame.setTextColor(rgb(182,255,182));
    gameFrame.drawCentreString(verdict, 160, 128, 2);
    panelButtons("PLAY AGAIN", "EXIT");
  }
}

void statBar(int x, int y, const char *label, float value, uint16_t color, int w) {
  gameFrame.setTextColor(rgb(182,182,219));
  gameFrame.drawString(label, x, y, 1);
  gameFrame.drawRect(x, y + 10, w, 7, rgb(73,73,146));
  gameFrame.fillRect(x + 1, y + 11, (int)((w - 2) * constrain(value, 0.0f, 1.0f)), 5, color);
}

void renderGarage(uint32_t now) {
  GameGfx::gradient(0, 0, 320, 240, rgb(0,36,109), rgb(73,0,109));
  header();
  gameFrame.setTextColor(rgb(255,219,0));
  gameFrame.drawCentreString("ROAD TRIP RALLY - PICK A CAR", 186, 5, 2);
  for (int i = 0; i < 3; ++i) {
    const TouchButton &b = garageCards[i];
    const Ride &r = rides[i];
    const int cx = b.x + b.w / 2;
    gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 10, rgb(0,0,73));
    gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 10, r.color);
    GameGfx::glow(cx, b.y + 56, 34, GameGfx::darker(r.color));
    gameFrame.fillEllipse(cx, b.y + 88, 44, 6, rgb(36,36,73));
    carBack(cx, b.y + 88 - (int)((now / 120 + i) % 2), 76, r.color, r.style, false);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(r.name, cx, b.y + 98, 2);
    statBar(b.x + 8, b.y + 118, "TOP SPEED", (r.top - 0.8f) / 0.2f, rgb(255,73,85), b.w - 16);
    statBar(b.x + 8, b.y + 138, "PICKUP", r.accel / (maxSpeed / 4), rgb(255,182,0), b.w - 16);
    statBar(b.x + 8, b.y + 158, "GRIP", r.grip, rgb(0,219,255), b.w - 16);
    gameFrame.setTextColor(rgb(255,219,85));
    gameFrame.drawCentreString("TAP TO RACE", cx, b.y + 182, 1);
  }
}

void render() {
  const uint32_t now = millis();
  if (stage == GARAGE) renderGarage(now);
  else {
    background(now);
    projectRoad();
    for (int n = drawDist - 1; n >= 0; --n) if (proj[n].ok) drawSegment(n, now);
    // The player's car, bouncing when it runs off the road.
    const bool offroad = fabsf(playerX) > 1;
    const int bounce = (offroad && speed > 100 ? (int)((now / 40) % 2) * 2 : 0) + (shake > 0 ? random(-2, 3) : 0);
    carBack(160 + (shake > 0 ? random(-3, 4) : 0), 234 - bounce, 92, rides[ride].color, rides[ride].style, braking, (int)steer);
    if (offroad && speed > 1000)
      for (int i = 0; i < 3; ++i) gameFrame.fillCircle(118 + i * 42 + random(-3, 4), 230 - random(0, 6), 4 + random(0, 3), themes[level].groundB);
    hud(now);
    if (stage == RACING) {
      if (countdown > 0) {
        char n[4]; snprintf(n, sizeof(n), "%d", (int)ceilf(countdown));
        bannerBox(n, rgb(255,219,0));
      } else if (banner > 0) bannerBox(bannerText, rgb(0,255,146));
    } else panel();
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

// ---- Racing -------------------------------------------------------------------
int rank() {
  int ahead = 0;
  for (const Car &c : cars) if (c.rival && c.z > playerDist()) ++ahead;
  return ahead + 1;
}

const uint16_t trafficColors[4] = {rgb(219,219,219), rgb(146,146,109), rgb(73,109,146), rgb(182,146,109)};
void placeTraffic(Car &c, float z) {
  const float lanes[3] = {-0.62f, 0, 0.62f};
  const int kind = random(0, 4);
  c = {z, lanes[random(0, 3)], 0, maxSpeed * (0.32f + random(0, 20) / 100.0f),
       trafficColors[random(0, 4)], (uint8_t)(kind == 0 ? VAN : kind == 1 ? TRUCK : SEDAN), false};
  c.targetX = c.x;
}

void startStage() {
  loadTrack();
  stage = RACING;
  position = playerX = speed = steer = 0;
  timeLeft = startTime; countdown = 3; banner = endTimer = shake = bgOffset = 0;
  nextCheckpoint = 1; finished = braking = false;
  // The player starts at the back of the grid, behind five rivals.
  const uint16_t rivalColors[rivalCount] = {rgb(255,219,0), rgb(0,219,255), rgb(182,73,255), rgb(255,146,0), TFT_WHITE};
  const Style rivalStyles[rivalCount] = {ROADSTER, MUSCLE, OFFROAD, MUSCLE, ROADSTER};
  for (int i = 0; i < rivalCount; ++i) {
    const float x = i % 2 ? 0.45f : -0.45f;
    cars[i] = {playerDist() + 260 + i * 260, x, x, maxSpeed * (0.70f + i * 0.04f + level * 0.01f),
               rivalColors[i], (uint8_t)rivalStyles[i], true};
  }
  for (int i = 0; i < trafficCount; ++i) placeTraffic(cars[rivalCount + i], playerDist() + 3000 + i * 2200);
  place = rank();
  lastFrame = millis();
  GameAudio::play(GameAudio::Click);
}

void startTrip(int chosen) {
  ride = chosen; level = 0;
  for (int &p : places) p = 0;
  GameAudio::stop();
  startStage();
}

void step(float dt, uint8_t held, bool touching, uint16_t tx) {
  const Ride &r = rides[ride];
  if (stage != RACING) {
    speed = max(0.0f, speed - maxSpeed * dt);
    position += speed * dt;
    return;
  }
  banner = max(0.0f, banner - dt);
  shake = max(0.0f, shake - dt);
  if (countdown > 0) {
    const int before = (int)ceilf(countdown);
    countdown -= dt;
    if (countdown <= 0) { bannerText = "GO!"; banner = 1; GameAudio::play(GameAudio::Start); }
    else if ((int)ceilf(countdown) != before) GameAudio::play(GameAudio::Click);
    return;
  }
  // P0/P1 steer, P2 gas, P3 brake. Touch steers on the outer thirds and always drives.
  bool left = held & 1, right = held & 2, gas = held & 4;
  braking = held & 8;
  if (touching) { gas = true; left = left || tx < 107; right = right || tx > 213; }
  if (finished) { gas = false; braking = true; left = right = false; }
  const float pct = speed / maxSpeed;
  const int dir = (int)right - (int)left;
  steer += (dir - steer) * min(1.0f, dt * 8);
  playerX += dir * dt * 2.4f * r.grip * min(1.0f, pct * 2.5f);
  // Bends push the car toward the outside.
  playerX -= dt * 2 * pct * pct * curveAt((int)(playerDist() / segLen)) * 0.18f;
  playerX = constrain(playerX, -2.2f, 2.2f);
  bgOffset += curveAt((int)(playerDist() / segLen)) * pct * dt * 60;

  if (braking) speed -= maxSpeed * dt;
  else if (gas) speed += r.accel * dt * (1 - 0.4f * pct);
  else speed -= maxSpeed / 5 * dt;
  const bool offroad = fabsf(playerX) > 1;
  if (offroad && speed > r.offroad * maxSpeed) speed -= maxSpeed / 1.5f * dt;
  speed = constrain(speed, 0.0f, r.top * maxSpeed);
  position += speed * dt;

  // Cars ahead: rivals weave around traffic; anyone you rear-end slows you down.
  for (int i = 0; i < carCount; ++i) {
    Car &c = cars[i];
    if (c.rival) {
      // Rubber band keeps the pack close enough to race.
      const float gap = c.z - playerDist();
      const float base = maxSpeed * (0.70f + i * 0.04f + level * 0.01f);
      const float want = gap > 5000 ? base * 0.8f : gap < -3000 ? maxSpeed * 0.97f : base;
      c.speed += (want - c.speed) * min(1.0f, dt);
      for (int k = 0; k < carCount; ++k) {
        const Car &o = cars[k];
        if (k == i) continue;
        const float dz = o.z - c.z;
        if (dz > 0 && dz < 900 && fabsf(o.x - c.targetX) < 0.5f) { c.targetX = o.x > 0 ? -0.5f : 0.5f; break; }
      }
      const float pz = playerDist() - c.z;
      if (pz > 0 && pz < 700 && fabsf(playerX - c.targetX) < 0.5f) c.targetX = playerX > 0 ? -0.6f : 0.6f;
      c.x += constrain(c.targetX - c.x, -dt, dt);
    }
    c.z += c.speed * dt;
    const float dz = c.z - playerDist();
    if (dz > 0 && dz < 180 && fabsf(c.x - playerX) < 0.55f && speed > c.speed) {
      speed = c.speed * 0.6f;
      position = c.z - 180 - playerZ;
      shake = 0.4f;
      DamageLed::flash(millis());
      GameAudio::play(GameAudio::Crash);
    }
    // Traffic you've left behind reappears far up the road.
    if (!c.rival && dz < -1500) {
      const float z = playerDist() + drawDist * segLen + random(0, 3000);
      if (z < finishZ() - 1000) placeTraffic(c, z);
    }
  }

  if (!finished) {
    timeLeft -= dt;
    place = rank();
    if (nextCheckpoint < 3 && playerDist() >= checkpointZ(nextCheckpoint)) {
      ++nextCheckpoint;
      timeLeft += checkpointBonus;
      bannerText = "CHECKPOINT! +20"; banner = 2;
      GameAudio::play(GameAudio::Happy);
    }
    if (playerDist() >= finishZ()) {
      finished = true; endTimer = 2.5f;
      places[level] = place;
      bannerText = place == 1 ? "1ST PLACE!" : "FINISH!"; banner = 2.5f;
      GameAudio::play(place == 1 ? GameAudio::Win : GameAudio::Feed);
    } else if (timeLeft <= 0) {
      timeLeft = 0; stage = TIME_UP;
      GameAudio::play(GameAudio::Lose);
    }
  } else {
    endTimer -= dt;
    if (endTimer <= 0) stage = level + 1 < stageCount ? STAGE_DONE : TRIP_DONE;
  }
}

int navItems(const TouchButton **items) {
  if (stage == GARAGE) { for (int i = 0; i < 3; ++i) items[i] = &garageCards[i]; return 3; }
  if (stage == RACING) return 0;
  items[0] = &nextButton; items[1] = &leaveButton;
  return 2;
}

bool engineOn() { return stage == RACING && countdown <= 0; }
float engineSpeed() { return speed / maxSpeed * 50; }

void update(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y, uint8_t held) {
  if (tapped && exitButton.contains(x, y)) { drawGameList(); return; }
  if (stage == GARAGE) {
    for (int i = 0; i < 3; ++i) if (tapped && garageCards[i].contains(x, y)) { startTrip(i); render(); return; }
    if (now - lastFrame >= 120) { lastFrame = now; render(); }
    return;
  }
  if (stage != RACING && tapped) {
    if (nextButton.contains(x, y)) {
      if (stage == STAGE_DONE) ++level;
      else if (stage == TRIP_DONE) { startTrip(ride); render(); return; }
      startStage(); render(); return;
    }
    if (leaveButton.contains(x, y)) { drawGameList(); return; }
  }
  if (now - lastFrame < 33) return;
  const float dt = min(0.05f, (now - lastFrame) / 1000.0f);
  lastFrame = now;
  // Touches on the panel buttons must not also steer.
  step(dt, held, touching && stage == RACING && y > 30, x);
  render();
}
} // namespace RoadTrip

void beginRoadTrip() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  gameState = ROAD_TRIP;
  RoadTrip::stage = RoadTrip::GARAGE;
  RoadTrip::lastFrame = millis();
  RoadTrip::render();
}
