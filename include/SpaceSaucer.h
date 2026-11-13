#pragma once
#include "GameAudio.h"
#include "GameGfx.h"
#include <math.h>

void beginSpaceSaucer();

// Space Saucer: tap the flying saucers before the 30-second timer runs out.
// Large saucers take two hits.
namespace SpaceSaucer {
using GameGfx::rgb;
constexpr uint8_t SAUCER_COUNT = 5;
constexpr uint32_t LEVEL_DURATION_MS = 30000;
struct Saucer {
  float x, y, vx, vy;
  uint8_t radius;
  uint8_t hitsLeft;
  bool exploding;
  uint32_t explosionStarted;
} saucers[SAUCER_COUNT];

uint16_t score = 0;
uint32_t levelStarted = 0;
// Last tap, drawn as a brief expanding reticle.
uint32_t shotAt = 0;
int16_t shotX = -100, shotY = -100;
uint32_t lastGameFrame = 0;
const TouchButton againButton = {170, 181, 140, 48};
const TouchButton menuButton = {10, 181, 140, 48};
// Set while a finger that started on AGAIN is still down.
bool againPressed = false;

void drawStars() {
  // A fixed pattern keeps the results screen attractive without flickering on redraw.
  // X coordinates can be up to 319 on the 320-pixel-wide display.
  const uint16_t stars[][3] = {
    {18, 18, 1}, {46, 43, 2}, {76, 15, 1}, {111, 33, 1}, {142, 14, 2},
    {178, 38, 1}, {218, 17, 2}, {259, 42, 1}, {293, 19, 1}, {307, 64, 2},
    {22, 126, 1}, {51, 151, 1}, {281, 130, 1}, {304, 154, 1}, {13, 205, 2}
  };
  for (const auto &star : stars) {
    gameFrame.fillCircle(star[0], star[1], star[2], star[2] == 2 ? COLOR_MINT : TFT_WHITE);
  }
}

void drawAgainButton(bool pressed) {
  const TouchButton &b = againButton;
  const uint16_t fill = pressed ? COLOR_MINT : COLOR_PURPLE;
  const uint16_t edge = pressed ? TFT_WHITE : COLOR_PINK;

  gameFrame.fillRoundRect(b.x + 3, b.y + 4, b.w, b.h, 14, TFT_BLACK);
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 14, edge);
  gameFrame.fillRoundRect(b.x + 3, b.y + 3, b.w - 6, b.h - 6, 11, fill);

  gameFrame.setTextFont(4);
  gameFrame.setTextColor(TFT_BLACK, fill);
  gameFrame.drawCentreString("AGAIN", b.x + b.w / 2 + 1, b.y + 14, 4);
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.drawCentreString("AGAIN", b.x + b.w / 2, b.y + 12, 4);
}

void respawnSaucer(Saucer &saucer) {
  const bool large = random(0, 3) == 0;
  saucer.radius = large ? 23 : 14;
  saucer.hitsLeft = large ? 2 : 1;
  saucer.x = random(saucer.radius, gameFrame.width() - saucer.radius);
  saucer.y = random(43 + saucer.radius, gameFrame.height() - saucer.radius);
  const float speed = large ? random(65, 115) / 100.0f : random(100, 270) / 100.0f;
  saucer.vx = (random(0, 2) ? speed : -speed);
  saucer.vy = (random(0, 2) ? speed * 0.70f : -speed * 0.70f);
  saucer.exploding = false;
}

// Static deep-space backdrop; only the twinkling stars change between frames.
void drawSpace(uint32_t now) {
  GameGfx::gradient(0, 0, 320, 240, rgb(0,0,36), rgb(36,0,85));
  GameGfx::glow(235, 150, 46, rgb(73,0,109));
  GameGfx::glow(90, 90, 34, rgb(0,36,109));
  GameGfx::glow(275, 105, 30, rgb(73,0,109));
  GameGfx::glow(118, 70, 22, rgb(0,36,109));
  GameGfx::glow(250, 140, 20, rgb(109,36,170));
  const uint16_t stars[][3] = {
    {18, 48, 1}, {46, 73, 2}, {76, 45, 1}, {111, 63, 1}, {142, 44, 2},
    {178, 68, 1}, {218, 47, 2}, {259, 72, 1}, {293, 49, 1}, {307, 94, 2},
    {22, 126, 1}, {51, 151, 1}, {281, 130, 1}, {304, 154, 1}, {13, 205, 2},
    {130, 120, 1}, {160, 190, 1}, {200, 222, 2}, {240, 180, 1}, {95, 222, 1},
    {175, 100, 1}, {60, 110, 1}, {270, 210, 1}, {120, 165, 2}, {300, 230, 1}
  };
  int i = 0;
  for (const auto &star : stars) {
    bool bright = ((now / 260) + i * 7) % 11 == 0;
    uint16_t color = star[2] == 2 ? COLOR_MINT : (i % 3 ? TFT_WHITE : rgb(182,182,255));
    if (bright) {
      gameFrame.drawFastHLine(star[0] - 3, star[1], 7, color);
      gameFrame.drawFastVLine(star[0], star[1] - 3, 7, color);
    }
    gameFrame.fillRect(star[0], star[1], star[2], star[2], color);
    ++i;
  }
  // Ringed planet: banded surface, dithered terminator, ring behind and in front.
  const int px = 36, py = 206, pr = 30;
  auto ring = [&](bool front) {
    for (int a = front ? 0 : 180; a < (front ? 180 : 360); a += 2) {
      float t = a * 0.01745f;
      gameFrame.drawPixel(px + (int)(56 * cosf(t)), py + (int)(12 * sinf(t)), rgb(255,219,170));
      gameFrame.drawPixel(px + (int)(52 * cosf(t)), py + (int)(11 * sinf(t)), rgb(219,146,85));
      gameFrame.drawPixel(px + (int)(48 * cosf(t)), py + (int)(10 * sinf(t)), rgb(146,109,85));
    }
  };
  ring(false);
  const uint8_t bands[2] = {GameGfx::pack(rgb(219,109,85)), GameGfx::pack(rgb(182,73,85))};
  for (int dy = -pr; dy <= pr; ++dy) {
    int y = py + dy;
    if (y < 0 || y >= 240) continue;
    int half = (int)sqrtf((float)(pr * pr - dy * dy));
    uint8_t *row = GameGfx::pixels() + y * 320;
    for (int dx = -half; dx <= half; ++dx) {
      int x = px + dx;
      if (x < 0 || x >= 320) continue;
      uint8_t c = bands[((dy + pr + dx / 6) / 7) % 2];
      int lit = (dx + 12) * (dx + 12) + (dy + 12) * (dy + 12);
      // Past the terminator, dither then fully shade toward the night side.
      if (lit > 1300 || (lit > 1000 && GameGfx::bayer[y & 3][x & 3] < (lit - 1000) / 19)) c = GameGfx::half(c);
      row[x] = c;
    }
  }
  GameGfx::glow(px - 12, py - 12, 9, rgb(255,182,170));
  ring(true);
}

void drawSaucer(const Saucer &saucer, uint32_t now) {
  const int16_t x = (int16_t)saucer.x;
  const int16_t y = (int16_t)saucer.y;
  const int16_t r = saucer.radius;
  if (saucer.exploding) {
    // Flash, shock ring and flying shards over the 130 ms explosion.
    const int16_t step = min(now - saucer.explosionStarted, (uint32_t)130) * 10 / 130;
    const int16_t blast = r + 4 + step;
    GameGfx::glow(x, y, blast + 6, COLOR_GOLD);
    gameFrame.drawCircle(x, y, blast + 2, TFT_WHITE);
    gameFrame.fillCircle(x, y, max(3, blast - 6 - step), step < 5 ? TFT_WHITE : COLOR_GOLD);
    gameFrame.fillCircle(x, y, max(1, blast / 2 - step), COLOR_PINK);
    for (int i = 0; i < 8; ++i) {
      float a = i * PI / 4 + 0.3f;
      int d = r / 2 + step * 2 + 6;
      gameFrame.drawWideLine(x + cosf(a) * d, y + sinf(a) * d * 0.7f,
                             x + cosf(a) * (d + 5), y + sinf(a) * (d + 5) * 0.7f, 2,
                             i % 2 ? COLOR_MINT : COLOR_GOLD);
    }
    return;
  }

  const bool large = r > 20;
  const uint16_t hull = saucer.hitsLeft == 2 ? COLOR_PINK : COLOR_MINT;
  const uint16_t hullDark = GameGfx::darker(hull);
  // Thruster glow and a faint tractor beam below the hull.
  GameGfx::glow(x, y + r / 2 + 3, r / 3 + 2, rgb(146,255,255));
  if (large) {
    for (int dy = 0; dy < 14; ++dy)
      GameGfx::screenSpan(x - 4 - dy / 2, x + 5 + dy / 2, y + r / 2 + 6 + dy, GameGfx::pack(rgb(146,255,170)));
  }
  gameFrame.fillEllipse(x, y + 5, r - 2, r / 3, hullDark);
  gameFrame.fillEllipse(x, y + 3, r, r / 2 - 1, hull);
  gameFrame.fillEllipse(x, y + 1, r - 4, r / 5, GameGfx::lighter(hull));
  gameFrame.drawFastHLine(x - r + 2, y + 4, 2 * r - 4, hullDark);
  // Glass dome with a little pilot and a reflection.
  const int16_t dome = r / 2;
  gameFrame.fillCircle(x, y - r / 4, dome, rgb(73,73,170));
  gameFrame.fillCircle(x, y - r / 4 + 1, dome / 2 + 1, rgb(109,219,0));
  gameFrame.fillRect(x - dome / 3, y - r / 4, 2, 2, TFT_BLACK);
  gameFrame.fillRect(x + dome / 3 - 1, y - r / 4, 2, 2, TFT_BLACK);
  gameFrame.drawCircle(x, y - r / 4, dome, rgb(182,219,255));
  gameFrame.fillRect(x - dome / 2, y - r / 4 - dome / 2, 2, 2, TFT_WHITE);
  gameFrame.fillRect(x - dome, y - r / 4 + 1, 2 * dome + 1, 2, hull);
  // Chase lights run around the rim.
  const int lights = large ? 7 : 5;
  for (int i = 0; i < lights; ++i) {
    int lx = x - r + 4 + i * (2 * r - 8) / (lights - 1);
    bool on = ((now / 90) + i) % 3 == 0;
    gameFrame.fillRect(lx - 1, y + 5, 3, 2, on ? TFT_WHITE : COLOR_GOLD);
  }
  // A damaged large saucer smokes and sparks until the second hit.
  if (large && saucer.hitsLeft == 1) {
    int puff = (now / 120) % 3;
    gameFrame.fillCircle(x - r / 2 - puff * 3, y - 4 - puff * 2, 4 + puff, rgb(109,109,170));
    gameFrame.fillCircle(x - r / 2 - 4 - puff * 3, y - 8 - puff * 2, 3, rgb(73,73,109));
    gameFrame.drawLine(x + 4, y + 1, x + 9, y + 5, TFT_BLACK);
    gameFrame.drawLine(x + 9, y + 5, x + 6, y + 8, TFT_BLACK);
    if (puff == 0) gameFrame.fillRect(x + 8, y + 3, 2, 2, TFT_YELLOW);
  }
}

void drawGame(uint32_t elapsed) {
  const uint32_t now = levelStarted + elapsed;
  drawSpace(now);
  for (const Saucer &saucer : saucers) drawSaucer(saucer, now);
  if (now - shotAt < 160) {
    int ring = 4 + (now - shotAt) / 16;
    gameFrame.drawCircle(shotX, shotY, ring, TFT_WHITE);
    gameFrame.drawCircle(shotX, shotY, ring + 2, COLOR_PINK);
    gameFrame.drawFastHLine(shotX - ring - 5, shotY, 4, TFT_WHITE);
    gameFrame.drawFastHLine(shotX + ring + 2, shotY, 4, TFT_WHITE);
    gameFrame.drawFastVLine(shotX, shotY - ring - 5, 4, TFT_WHITE);
    gameFrame.drawFastVLine(shotX, shotY + ring + 2, 4, TFT_WHITE);
  }
  const uint8_t secondsLeft = (LEVEL_DURATION_MS - elapsed + 999) / 1000;
  GameGfx::gradient(0, 0, 320, 31, rgb(73,0,109), rgb(0,0,36));
  gameFrame.setTextFont(2);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawString("SCORE: " + String(score), 8, 6, 2);
  gameFrame.setTextColor(secondsLeft <= 5 ? COLOR_PINK : COLOR_GOLD);
  gameFrame.drawRightString("TIME: " + String(secondsLeft), gameFrame.width() - 8, 6, 2);
  // Countdown bar drains and turns pink in the final seconds.
  const int remaining = (LEVEL_DURATION_MS - elapsed) * 320 / LEVEL_DURATION_MS;
  gameFrame.drawFastHLine(0, 29, 320, rgb(36,0,85));
  gameFrame.fillRect(0, 28, remaining, 3, secondsLeft <= 5 ? COLOR_PINK : COLOR_MINT);
  gameFrame.pushSprite(0, 0);
}

void showResults(bool announce=true) {
  if (announce) GameAudio::play(GameAudio::Win);
  gameState = SAUCER_RESULTS;
  againPressed = false;
  gameFrame.fillSprite(COLOR_SKY);
  drawStars();
  drawCentered("TIME'S UP!", 54, 4, COLOR_PINK);
  drawCentered("SAUCERS DOWN", 95, 2, TFT_WHITE);
  drawCentered(String(score).c_str(), 116, 6, COLOR_GOLD);
  drawAgainButton(false);
  drawBackButton(menuButton, 4);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void pause(uint32_t pausedFor) {
  levelStarted += pausedFor;
  lastGameFrame += pausedFor;
  for (auto &saucer : saucers) saucer.explosionStarted += pausedFor;
}

void updateResults(bool touching, bool tapped, uint16_t x, uint16_t y) {
  if (tapped && menuButton.contains(x, y)) {
    drawGameList();
  } else if (touching && againButton.contains(x, y)) {
    if (!againPressed) {
      againPressed = true;
      drawAgainButton(true);
      UiNavigation::draw();
      gameFrame.pushSprite(0,0);
    }
  } else if (!touching && againPressed) {
    // Trigger only after a tap that began inside the button is released.
    beginSpaceSaucer();
  } else if (touching && againPressed) {
    // Letting the finger slide away cancels the tap and restores the button.
    againPressed = false;
    drawAgainButton(false);
    UiNavigation::draw();
    gameFrame.pushSprite(0,0);
  }
}
} // namespace SpaceSaucer

void beginSpaceSaucer() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  using namespace SpaceSaucer;
  gameState = SAUCER_PLAYING;
  score = 0;
  levelStarted = millis();
  lastGameFrame = levelStarted;
  for (Saucer &saucer : saucers) respawnSaucer(saucer);
}

void updateSpaceSaucer(uint32_t now, bool tapped, uint16_t touchX, uint16_t touchY) {
  using namespace SpaceSaucer;
  const uint32_t elapsed = now - levelStarted;
  if (elapsed >= LEVEL_DURATION_MS) {
    showResults();
    return;
  }

  if (tapped) {
    GameAudio::play(GameAudio::Shot);
    shotAt = now; shotX = touchX; shotY = touchY;
    for (Saucer &saucer : saucers) {
      if (saucer.exploding) continue;
      const float dx = touchX - saucer.x;
      const float dy = (touchY - saucer.y) * 2.0f;
      if (dx * dx + dy * dy <= saucer.radius * saucer.radius) {
        GameAudio::play(saucer.hitsLeft==1 ? GameAudio::Pop : GameAudio::Hit);
        if (--saucer.hitsLeft == 0) {
          score++;
          saucer.exploding = true;
          saucer.explosionStarted = now;
        }
        break; // One tap can damage only one saucer.
      }
    }
  }

  if (now-lastGameFrame<33) return;
  const float frameScale = min(now - lastGameFrame, (uint32_t)100) / 25.0f;
  lastGameFrame = now;
  for (Saucer &saucer : saucers) {
    if (saucer.exploding) {
      if (now - saucer.explosionStarted > 130) respawnSaucer(saucer);
      continue;
    }
    saucer.x += saucer.vx * frameScale;
    saucer.y += saucer.vy * frameScale;
    if (saucer.x < saucer.radius || saucer.x > gameFrame.width() - saucer.radius) saucer.vx = -saucer.vx;
    if (saucer.y < 38 + saucer.radius || saucer.y > gameFrame.height() - saucer.radius) saucer.vy = -saucer.vy;
  }
  drawGame(elapsed);
}
