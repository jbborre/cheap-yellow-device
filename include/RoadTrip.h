#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
#include "art/RoadTripArt.h"
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

// Cars and scenery are pre-rendered images from include/art/RoadTripArt.h (made
// by tools/art). Car bodies and neon tubes are painted in marker colors that a
// remap table turns into each car's own color.
using GameGfx::Image;
namespace Art = RoadTripArt;
struct CarArt { const Image *big, *mid, *small, *left, *right; };
const CarArt carArt[6] = {
  {&Art::carRoadster92, &Art::carRoadster46, &Art::carRoadster22, &Art::carRoadster92L, &Art::carRoadster92R},
  {&Art::carMuscle92, &Art::carMuscle46, &Art::carMuscle22, &Art::carMuscle92L, &Art::carMuscle92R},
  {&Art::carOffroad92, &Art::carOffroad46, &Art::carOffroad22, &Art::carOffroad92L, &Art::carOffroad92R},
  {&Art::carSedan92, &Art::carSedan46, &Art::carSedan22},
  {&Art::carVan92, &Art::carVan46, &Art::carVan22},
  {&Art::carTruck92, &Art::carTruck46, &Art::carTruck22},
};

// A car seen from behind, w pixels wide with its bottom center on (x, bottom).
void carBack(int x, int bottom, int w, uint16_t color, int style, bool brake, int lean = 0) {
  if (w < 3) return;
  const CarArt &a = carArt[style];
  const Image *img = w <= 26 ? a.small : w <= 52 ? a.mid : a.big;
  if (img == a.big && lean < 0 && a.left) img = a.left;
  else if (img == a.big && lean > 0 && a.right) img = a.right;
  GameGfx::darkenEllipse(x, bottom, w * 3 / 5, max(1, w / 10));
  const uint8_t *remap = GameGfx::bodyRemap(color, brake ? rgb(255,36,36) : rgb(146,0,0), brake ? rgb(255,182,182) : rgb(219,36,36));
  // Within a few pixels of the drawn size, skip scaling so details stay crisp.
  const float scale = abs(w - img->w) <= 3 ? 1.0f : w / (float)img->w;
  GameGfx::drawImageScaled(*img, x, bottom, scale, false, remap);
}

// Roadside scenery, sized by u: half the road's width on screen at that spot.
struct PropArt { const Image *big, *small; float height; };
const PropArt propArt[] = {
  {&Art::propPalmBig, &Art::propPalmSmall, 1.5f},
  {&Art::propUmbrellaBig, &Art::propUmbrellaSmall, 0.9f},
  {&Art::propCactusBig, &Art::propCactusSmall, 1.05f},
  {&Art::propRockBig, &Art::propRockSmall, 0.45f},
  {&Art::propPineBig, &Art::propPineSmall, 1.35f},
  {&Art::propSnowmanBig, &Art::propSnowmanSmall, 0.8f},
  {&Art::propBarnBig, &Art::propBarnSmall, 1.15f},
  {&Art::propHaybaleBig, &Art::propHaybaleSmall, 0.35f},
  {&Art::propStreetlightBig, &Art::propStreetlightSmall, 1.35f},
  {&Art::propDinerBig, &Art::propDinerSmall, 1.0f},
};

void prop(Prop p, int x, int b, float u, uint32_t now, int seed = 0, bool flip = false) {
  PropArt art = propArt[p];
  if (p == NEON && seed % 2) art = {&Art::propMotelBig, &Art::propMotelSmall, art.height};
  const float h = art.height * u;
  if (h < 3) return;
  const Image &img = h <= art.small->h * 1.2f ? *art.small : *art.big;
  const uint8_t *remap = nullptr;
  if (p == NEON) {
    const uint16_t glowC[3] = {rgb(255,0,255), rgb(0,255,255), rgb(255,255,0)};
    remap = GameGfx::bodyRemap(glowC[(seed / 2 + now / 400) % 3]);
  } else if (p == STREETLIGHT) GameGfx::glow(x + (flip ? -1 : 1) * (int)(h * 0.19f), b - (int)(h * 0.9f), max(3, (int)(u / 3)), rgb(255,219,109));
  GameGfx::drawImageScaled(img, x, b, h / img.h, flip, remap);
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
const Image *const skies[stageCount] = {&Art::skyCoast, &Art::skyCanyon, &Art::skyPeaks, &Art::skyFarm, &Art::skyNight};

void background(uint32_t now) {
  // The panorama wraps around, drifting as the road bends.
  GameGfx::drawStrip(*skies[level], 0, (int)bgOffset / 2);
  gameFrame.fillRect(0, horizonY, 320, 240 - horizonY, themes[level].groundA);
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
      prop(STREETLIGHT, (int)(p.x1 - p.w1 * 1.35f), (int)p.y1, p.w1, now, idx);
      prop(STREETLIGHT, (int)(p.x1 + p.w1 * 1.35f), (int)p.y1, p.w1, now, idx, true);
    } else if (idx % 3 == 0 && h % 4 != 0) {
      const Prop kind = (h >> 8) % 3 == 0 ? t.propB : t.propA;
      const float side = (h >> 4) & 1 ? -1 : 1;
      const float spread = kind == BARN ? 2.2f : 1.4f;
      prop(kind, (int)(p.x1 + side * p.w1 * (spread + ((h >> 12) % 60) / 100.0f)), (int)p.y1, p.w1, now, idx / 3);
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
    carBack((int)cx, (int)cy, (int)(w * 0.5f), c.color, c.style, false);
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
    carBack(160 + (shake > 0 ? random(-3, 4) : 0), 234 - bounce, 92, rides[ride].color, rides[ride].style, braking, steer > 0.4f ? 1 : steer < -0.4f ? -1 : 0);
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
