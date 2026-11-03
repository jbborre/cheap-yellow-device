// Auggie's Game - game menu and Space Saucer game for the ESP32 Cheap Yellow Display.
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Free_Fonts.h"
#include "include/GameAudio.h"
#include "include/ExpanderButtons.h"
#include "include/BatteryStatus.h"
#include "include/GameDisplay.h"

TFT_eSPI tft;
GameDisplay::Frame gameFrame(&tft);
#include "include/GameGfx.h"

// LCDWIKI ST7796 boards share display SPI; the original CYD uses separate SPI.
constexpr uint8_t TOUCH_CS = 33;
constexpr uint8_t TOUCH_IRQ = 36;
#if !GAME_TOUCH_SHARED_SPI
constexpr uint8_t TOUCH_MOSI = 32;
constexpr uint8_t TOUCH_MISO = 39;
constexpr uint8_t TOUCH_SCLK = 25;
#endif
// Bump this when a fresh saved touch calibration is needed on the device.
constexpr uint32_t CALIBRATION_MAGIC = 0x41554736; // "AUG6": includes physical panel dimensions
constexpr int16_t CALIBRATION_INSET = 18;

#if !GAME_TOUCH_SHARED_SPI
SPIClass touchSPI(VSPI);
#endif
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

const uint16_t COLOR_SKY = 0x0015;
const uint16_t COLOR_PURPLE = 0xA8FF;
const uint16_t COLOR_PINK = 0xF9B5;
const uint16_t COLOR_GOLD = 0xFEA0;
const uint16_t COLOR_MINT = 0x5FFC;
struct TouchCalibration {
  uint32_t magic;
  uint16_t topLeftX;
  uint16_t topLeftY;
  uint16_t bottomRightX;
  uint16_t bottomRightY;
  uint16_t panelWidth;
  uint16_t panelHeight;
} touchCalibration;

struct TouchButton {
  int16_t x, y, w, h;

  bool contains(uint16_t tx, uint16_t ty) const {
    return tx >= x && tx < x + w && ty >= y && ty < y + h;
  }
} startButton, chooseGameButton, saucerMenuButton, yodaMenuButton, driveMenuButton, zombieMenuButton, backButton;

#include "include/UiNavigation.h"

enum GameState { HOME_MENU, GAME_LIST, PLAYING, RESULTS, YODA_PLAYING, CITY_DRIVING, ZOMBIE_CRASH, SETTINGS };
GameState gameState = HOME_MENU;
bool buttonWasPressed = false;
bool touchWasDown = false;
const TouchButton settingsButton={5,5,36,30};
const TouchButton settingsBackButton={8,6,60,25};
const TouchButton soundToggleButton={55,75,210,42};
const TouchButton volumeDownButton={45,145,62,44};
const TouchButton volumeUpButton={213,145,62,44};

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

// Baby Yoda's needs gradually drop while the pet screen is open.
uint8_t yodaHunger = 78;
uint8_t yodaHappiness = 78;
uint8_t yodaEnergy = 78;
uint32_t lastYodaNeedUpdate = 0;
uint32_t yodaBlinkStarted = 0;
uint32_t nextYodaBlink = 0;
bool yodaBlinking = false;
TouchButton feedButton, playButton, sleepButton;

void drawCentered(const char *text, int16_t y, uint8_t font, uint16_t color) {
  gameFrame.setTextFont(font);
  gameFrame.setTextColor(color, COLOR_SKY);
  gameFrame.drawCentreString(text, gameFrame.width() / 2, y, font);
}

void drawFreeCentered(const char *text, int16_t y, uint16_t color) {
  gameFrame.setTextColor(color);
  gameFrame.drawString(text, (gameFrame.width() - gameFrame.textWidth(text)) / 2, y);
}

void drawStars() {
  // A fixed pattern keeps the title screen attractive without flickering on redraw.
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

void drawStartButton(bool pressed, const char *label = "START") {
  const uint16_t fill = pressed ? COLOR_MINT : COLOR_PURPLE;
  const uint16_t edge = pressed ? TFT_WHITE : COLOR_PINK;
  const int16_t x = startButton.x;
  const int16_t y = startButton.y;

  gameFrame.fillRoundRect(x + 3, y + 4, startButton.w, startButton.h, 14, TFT_BLACK);
  gameFrame.fillRoundRect(x, y, startButton.w, startButton.h, 14, edge);
  gameFrame.fillRoundRect(x + 3, y + 3, startButton.w - 6, startButton.h - 6, 11, fill);

  gameFrame.setTextFont(4);
  gameFrame.setTextColor(TFT_BLACK, fill);
  gameFrame.drawCentreString(label, x + startButton.w / 2 + 1, y + 14, 4);
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.drawCentreString(label, x + startButton.w / 2, y + 12, 4);
}

void drawMenuButton(const TouchButton &button, const char *label, uint16_t fill) {
  gameFrame.fillRoundRect(button.x + 3, button.y + 4, button.w, button.h, 12, TFT_BLACK);
  gameFrame.fillRoundRect(button.x, button.y, button.w, button.h, 12, COLOR_PINK);
  gameFrame.fillRoundRect(button.x + 3, button.y + 3, button.w - 6, button.h - 6, 9, fill);
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.setTextFont(4);
  gameFrame.drawCentreString(label, button.x + button.w / 2, button.y + 10, 4);
}

void drawBackButton(const TouchButton &button, uint8_t font = 2) {
  gameFrame.fillRoundRect(button.x, button.y, button.w, button.h, 8, COLOR_PURPLE);
  gameFrame.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
  gameFrame.setTextFont(font);
  gameFrame.setTextColor(TFT_WHITE, COLOR_PURPLE);
  gameFrame.drawCentreString("BACK", button.x + button.w / 2,
                       button.y + (font == 4 ? 12 : 7), font);
}

void drawBatteryBadge() {
  gameFrame.fillRect(218,5,97,26,COLOR_SKY);
  gameFrame.setTextColor(BatteryStatus::percentage>=0 && BatteryStatus::percentage<=20 ? COLOR_PINK : COLOR_MINT,COLOR_SKY);
  char label[20];
  if (BatteryStatus::percentage<0) snprintf(label,sizeof(label),"BAT --%%");
  else snprintf(label,sizeof(label),"BAT %d%%",BatteryStatus::percentage);
  gameFrame.drawRightString(label,314,10,2);
}

void drawHomeMenu() {
  GameAudio::stop();
  gameState = HOME_MENU;
  buttonWasPressed = false;
  gameFrame.fillSprite(COLOR_SKY);
  drawStars();

  // Layered, offset lettering gives the title a neon arcade look.
  gameFrame.setFreeFont(FSSBO18);
  drawFreeCentered("AUGGIE'S", 52, TFT_BLACK);
  drawFreeCentered("AUGGIE'S", 49, COLOR_PINK);

  gameFrame.setFreeFont(FSSBO24);
  drawFreeCentered("GAME", 96, TFT_BLACK);
  drawFreeCentered("GAME", 92, COLOR_GOLD);
  gameFrame.setFreeFont(NULL);

  gameFrame.drawFastHLine(75, 145, 170, COLOR_MINT);
  drawCentered("READY TO PLAY?", 151, 2, TFT_WHITE);
  drawMenuButton(chooseGameButton, "CHOOSE GAME", COLOR_PURPLE);
  gameFrame.fillRoundRect(settingsButton.x,settingsButton.y,settingsButton.w,settingsButton.h,8,COLOR_PURPLE);
  for (int i=0;i<8;++i) {
    float angle=i*PI/4;
    gameFrame.drawWideLine(23+cosf(angle)*8,20+sinf(angle)*8,
                           23+cosf(angle)*12,20+sinf(angle)*12,4,TFT_WHITE);
  }
  gameFrame.fillCircle(23,20,9,TFT_WHITE);
  gameFrame.fillCircle(23,20,4,COLOR_PURPLE);
  drawBatteryBadge();
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void drawGameList() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  gameState = GAME_LIST;
  buttonWasPressed = false;
  backButton = {8, 6, 60, 25};
  gameFrame.fillSprite(COLOR_SKY);
  drawStars();
  drawBackButton(backButton);
  drawCentered("GAME LIST", 34, 4, COLOR_GOLD);
  drawCentered("PICK ONE TO PLAY", 61, 2, TFT_WHITE);
  drawMenuButton(saucerMenuButton, "SPACE SAUCER", COLOR_PURPLE);
  drawMenuButton(yodaMenuButton, "BABY YODA", 0x2585);
  drawMenuButton(driveMenuButton, "CITY DRIVE", 0x0357);
  drawMenuButton(zombieMenuButton, "ZOMBIE CRASH", 0x8208);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
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

using GameGfx::rgb;

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

void beginGame() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  gameState = PLAYING;
  score = 0;
  levelStarted = millis();
  lastGameFrame = levelStarted;
  for (Saucer &saucer : saucers) respawnSaucer(saucer);
}

void showResults(bool announce=true) {
  if (announce) GameAudio::play(GameAudio::Win);
  gameState = RESULTS;
  buttonWasPressed = false;
  startButton = {170, 181, 140, 48};
  backButton = {10, 181, 140, 48};
  gameFrame.fillSprite(COLOR_SKY);
  drawStars();
  drawCentered("TIME'S UP!", 54, 4, COLOR_PINK);
  drawCentered("SAUCERS DOWN", 95, 2, TFT_WHITE);
  drawCentered(String(score).c_str(), 116, 6, COLOR_GOLD);
  drawStartButton(false, "AGAIN");
  drawBackButton(backButton, 4);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void updateGame(uint32_t now, bool tapped, uint16_t touchX, uint16_t touchY) {
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

uint8_t increaseNeed(uint8_t value, uint8_t amount) {
  return min(100, value + amount);
}

uint8_t decreaseNeed(uint8_t value, uint32_t amount) {
  return amount >= value ? 0 : value - amount;
}

void drawNeedBar(const char *label, int16_t y, uint8_t value, uint16_t color) {
  gameFrame.setTextFont(2);
  gameFrame.setTextColor(TFT_WHITE, COLOR_SKY);
  gameFrame.drawString(label, 10, y, 2);
  gameFrame.drawRoundRect(82, y + 1, 225, 13, 6, TFT_WHITE);
  gameFrame.fillRoundRect(85, y + 4, (217 * value) / 100, 7, 3, color);
}

void drawYoda(bool blinking) {
  const int16_t x = gameFrame.width() / 2;
  const int16_t y = 132;
  const uint16_t green = 0x7E8B;
  const uint16_t greenShadow = 0x34C5;
  const uint16_t earInner = 0xE3B4;
  const uint16_t robe = 0xC618;
  const uint16_t robeShadow = 0x9CD3;

  // Layered ears, eyes, and robe give the small pet more depth at CYD resolution.
  gameFrame.fillTriangle(x - 33, y - 20, x - 82, y - 5, x - 38, y + 7, greenShadow);
  gameFrame.fillTriangle(x + 33, y - 20, x + 82, y - 5, x + 38, y + 7, greenShadow);
  gameFrame.fillTriangle(x - 37, y - 15, x - 70, y - 5, x - 38, y + 2, earInner);
  gameFrame.fillTriangle(x + 37, y - 15, x + 70, y - 5, x + 38, y + 2, earInner);
  gameFrame.fillEllipse(x, y, 45, 39, green);
  gameFrame.drawEllipse(x, y, 45, 39, TFT_WHITE);
  gameFrame.fillEllipse(x, y + 30, 33, 10, greenShadow);

  // Forehead creases and a tiny nose make the face feel less like a simple icon.
  gameFrame.drawLine(x - 18, y - 22, x - 10, y - 16, greenShadow);
  gameFrame.drawLine(x + 18, y - 22, x + 10, y - 16, greenShadow);
  gameFrame.fillCircle(x, y + 8, 3, greenShadow);
  gameFrame.drawLine(x - 7, y + 17, x, y + 19, TFT_BLACK);
  gameFrame.drawLine(x, y + 19, x + 7, y + 17, TFT_BLACK);

  if (blinking) {
    gameFrame.drawFastHLine(x - 23, y - 4, 15, TFT_BLACK);
    gameFrame.drawFastHLine(x + 8, y - 4, 15, TFT_BLACK);
  } else {
    gameFrame.fillEllipse(x - 16, y - 4, 8, 11, TFT_BLACK);
    gameFrame.fillEllipse(x + 16, y - 4, 8, 11, TFT_BLACK);
    gameFrame.fillCircle(x - 14, y - 8, 2, TFT_WHITE);
    gameFrame.fillCircle(x + 18, y - 8, 2, TFT_WHITE);
  }

  gameFrame.fillRoundRect(x - 45, y + 32, 91, 32, 13, robeShadow);
  gameFrame.fillRoundRect(x - 39, y + 34, 79, 29, 12, robe);
  gameFrame.drawRoundRect(x - 39, y + 34, 79, 29, 12, TFT_WHITE);
  gameFrame.fillTriangle(x - 22, y + 34, x, y + 55, x - 2, y + 34, TFT_WHITE);
  gameFrame.fillCircle(x - 39, y + 52, 6, green);
  gameFrame.fillCircle(x + 39, y + 52, 6, green);
}

void drawPetButton(const TouchButton &button, const char *label, uint16_t fill) {
  gameFrame.fillRoundRect(button.x, button.y, button.w, button.h, 8, fill);
  gameFrame.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
  gameFrame.setTextFont(2);
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.drawCentreString(label, button.x + button.w / 2, button.y + 10, 2);
}

void drawYodaGame() {
  gameFrame.fillSprite(COLOR_SKY);
  gameFrame.fillRoundRect(backButton.x,backButton.y,backButton.w,backButton.h,8,COLOR_PURPLE);
  gameFrame.drawRoundRect(backButton.x,backButton.y,backButton.w,backButton.h,8,TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE,COLOR_PURPLE);
  gameFrame.drawCentreString("BACK",backButton.x+backButton.w/2,backButton.y+7,2);
  gameFrame.setTextColor(COLOR_GOLD,COLOR_SKY);
  gameFrame.drawCentreString("RAISE BABY YODA",194,9,2);
  drawNeedBar("FOOD", 37, yodaHunger, COLOR_GOLD);
  drawNeedBar("FUN", 55, yodaHappiness, COLOR_PINK);
  drawNeedBar("REST", 73, yodaEnergy, COLOR_MINT);
  drawYoda(yodaBlinking);
  drawPetButton(feedButton, "FEED", COLOR_PURPLE);
  drawPetButton(playButton, "PLAY", COLOR_PINK);
  drawPetButton(sleepButton, "SLEEP", 0x2585);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void beginYodaGame() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  gameState = YODA_PLAYING;
  backButton = {8, 6, 60, 25};
  yodaHunger = 78;
  yodaHappiness = 78;
  yodaEnergy = 78;
  lastYodaNeedUpdate = millis();
  yodaBlinking = false;
  nextYodaBlink = lastYodaNeedUpdate + random(1800, 4200);
  drawYodaGame();
}

void updateYodaGame(uint32_t now, bool tapped, uint16_t touchX, uint16_t touchY) {
  if (tapped && backButton.contains(touchX, touchY)) {
    drawGameList();
    return;
  }

  bool redraw = false;
  if (!yodaBlinking && (int32_t)(now - nextYodaBlink) >= 0) {
    yodaBlinking = true;
    yodaBlinkStarted = now;
    redraw = true;
  } else if (yodaBlinking && now - yodaBlinkStarted >= 150) {
    yodaBlinking = false;
    nextYodaBlink = now + random(1800, 4200);
    redraw = true;
  }

  if (now - lastYodaNeedUpdate >= 5000) {
    const uint32_t intervals = (now - lastYodaNeedUpdate) / 5000;
    yodaHunger = decreaseNeed(yodaHunger, 4UL * intervals);
    yodaHappiness = decreaseNeed(yodaHappiness, 3UL * intervals);
    yodaEnergy = decreaseNeed(yodaEnergy, 2UL * intervals);
    lastYodaNeedUpdate += intervals * 5000;
    redraw=true;
  }

  if (!tapped) {
    if (redraw) drawYodaGame();
    return;
  }
  if (feedButton.contains(touchX, touchY)) {
    yodaHunger = increaseNeed(yodaHunger, 18);
    GameAudio::play(GameAudio::Feed);
  }
  else if (playButton.contains(touchX, touchY)) {
    GameAudio::play(GameAudio::Happy);
    yodaHappiness = increaseNeed(yodaHappiness, 18);
    yodaEnergy = yodaEnergy > 7 ? yodaEnergy - 7 : 0;
  } else if (sleepButton.contains(touchX, touchY)) {
    yodaEnergy = increaseNeed(yodaEnergy, 22);
    GameAudio::play(GameAudio::Sleep);
  }
  else {
    if (redraw) drawYodaGame();
    return;
  }
  drawYodaGame();
}

bool readTouch(uint16_t &x, uint16_t &y) {
  if (!touch.touched()) return false;

  TS_Point point = touch.getPoint();
  // The calibration touches are inset from the bezel, so preserve that inset
  // in the mapping. Mapping them to (0, 0) shifts upper-left taps outward.
  long screenX = map(point.x, touchCalibration.topLeftX,
                     touchCalibration.bottomRightX, CALIBRATION_INSET,
                     tft.width() - 1 - CALIBRATION_INSET);
  long screenY = map(point.y, touchCalibration.topLeftY,
                     touchCalibration.bottomRightY, CALIBRATION_INSET,
                     tft.height() - 1 - CALIBRATION_INSET);
  // Saved calibration describes landscape rotation 1; display now uses 3.
  screenX = tft.width() - 1 - screenX;
  screenY = tft.height() - 1 - screenY;
  x = GameDisplay::logicalCoordinate(screenX,tft.width(),GameDisplay::width);
  y = GameDisplay::logicalCoordinate(screenY,tft.height(),GameDisplay::height);
  return true;
}

void drawCalibrationTarget(int16_t x, int16_t y) {
  tft.drawFastHLine(x - 12, y, 25, COLOR_PINK);
  tft.drawFastVLine(x, y - 12, 25, COLOR_PINK);
  tft.drawCircle(x, y, 7, TFT_WHITE);
}

TS_Point waitForCalibrationTouch() {
  while (!touch.touched()) delay(10);
  delay(30);
  TS_Point point = touch.getPoint();
  while (touch.touched()) delay(10);
  delay(150);
  return point;
}

void touchCalibrate() {
  bool valid = false;
  if (SPIFFS.begin(true)) {
    File file = SPIFFS.open("/AuggieTouchCal_v6", "r");
    valid = file && file.readBytes((char *)&touchCalibration, sizeof(touchCalibration)) == sizeof(touchCalibration);
    if (file) file.close();
    valid = valid && touchCalibration.magic == CALIBRATION_MAGIC &&
            touchCalibration.panelWidth == tft.width() && touchCalibration.panelHeight == tft.height() &&
            abs((int)touchCalibration.topLeftX - touchCalibration.bottomRightX) > 1000 &&
            abs((int)touchCalibration.topLeftY - touchCalibration.bottomRightY) > 1000;
  }
  if (valid) return;

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(COLOR_GOLD,TFT_BLACK);
  tft.drawCentreString("TOUCH CALIBRATION",tft.width()/2,68,2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawCentreString("Touch each crosshair",tft.width()/2,94,2);
  drawCalibrationTarget(CALIBRATION_INSET, CALIBRATION_INSET);
  TS_Point topLeft = waitForCalibrationTouch();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(COLOR_GOLD,TFT_BLACK);
  tft.drawCentreString("TOUCH CALIBRATION",tft.width()/2,68,2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawCentreString("Touch each crosshair",tft.width()/2,94,2);
  drawCalibrationTarget(tft.width() - 1 - CALIBRATION_INSET,
                        tft.height() - 1 - CALIBRATION_INSET);
  TS_Point bottomRight = waitForCalibrationTouch();

  touchCalibration = {CALIBRATION_MAGIC,
                      static_cast<uint16_t>(bottomRight.x), static_cast<uint16_t>(bottomRight.y),
                      static_cast<uint16_t>(topLeft.x), static_cast<uint16_t>(topLeft.y),
                      static_cast<uint16_t>(tft.width()), static_cast<uint16_t>(tft.height())};
  File file = SPIFFS.open("/AuggieTouchCal_v6", "w");
  if (file) {
    file.write((const uint8_t *)&touchCalibration, sizeof(touchCalibration));
    file.close();
  }
}

#include "include/CityDrive.h"
#include "include/ZombieCrash.h"

void saveSoundSettings() {
  Preferences prefs;
  if (prefs.begin("game-sound",false)) {
    prefs.putBool("enabled",GameAudio::enabled);
    prefs.putUChar("volume",GameAudio::volume);
    prefs.end();
  }
}
void drawSettings() {
  gameState=SETTINGS;
  gameFrame.fillSprite(COLOR_SKY);
  drawBackButton(settingsBackButton);
  drawCentered("SETTINGS",35,4,COLOR_GOLD);
  drawMenuButton(soundToggleButton,GameAudio::enabled ? "SOUND: ON" : "SOUND: OFF",COLOR_PURPLE);
  drawMenuButton(volumeDownButton,"-",0x0357);
  drawMenuButton(volumeUpButton,"+",0x0357);
  char label[20];
  snprintf(label,sizeof(label),"%d%%",GameAudio::volumePercent());
  drawCentered(label,151,4,COLOR_MINT);
  drawCentered("VOLUME",124,2,TFT_WHITE);
  drawCentered("P0/P1: MOVE   P2: SELECT   P3: BACK",215,1,TFT_WHITE);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}
void beginSettings() { GameAudio::stop(); drawSettings(); }
void updateSettings(bool tapped,uint16_t x,uint16_t y) {
  if (!tapped) return;
  if (settingsBackButton.contains(x,y)) { drawHomeMenu(); return; }
  if (soundToggleButton.contains(x,y)) GameAudio::setEnabled(!GameAudio::enabled);
  else if (volumeDownButton.contains(x,y)) GameAudio::setVolume(GameAudio::volume-1);
  else if (volumeUpButton.contains(x,y)) GameAudio::setVolume(GameAudio::volume+1);
  else return;
  saveSoundSettings();
  GameAudio::play(GameAudio::Click);
  drawSettings();
}

// Translate physical menu presses into the same actions as touch selection.
void navigateUi(uint8_t pressed, bool &touching, bool &tapped, uint16_t &x, uint16_t &y) {
  const TouchButton *items[5] = {};
  const TouchButton *back = nullptr;
  int count = 0, context = (int)gameState * 16;
  switch (gameState) {
    case HOME_MENU:
      items[count++]=&chooseGameButton; items[count++]=&settingsButton; break;
    case SETTINGS:
      items[count++]=&soundToggleButton; items[count++]=&volumeDownButton;
      items[count++]=&volumeUpButton; back=&settingsBackButton; break;
    case GAME_LIST:
      items[count++]=&saucerMenuButton; items[count++]=&yodaMenuButton;
      items[count++]=&driveMenuButton; items[count++]=&zombieMenuButton;
      back=&backButton; break;
    case RESULTS:
      items[count++]=&startButton; items[count++]=&backButton; back=&backButton; break;
    case YODA_PLAYING:
      items[count++]=&feedButton; items[count++]=&playButton; items[count++]=&sleepButton;
      back=&backButton; break;
    case PLAYING:
      if (pressed & 8) { drawGameList(); pressed=0; }
      break;
    case CITY_DRIVING:
      if (CityDrive::finished) {
        context++; items[count++]=&CityDrive::retryButton; back=&CityDrive::exitButton;
      }
      break;
    case ZOMBIE_CRASH:
      context+=(int)ZombieCrash::stage;
      if (ZombieCrash::stage==ZombieCrash::CHARACTER_SELECT || ZombieCrash::stage==ZombieCrash::WEAPON_SELECT) {
        items[count++]=&ZombieCrash::firstChoice; items[count++]=&ZombieCrash::secondChoice;
        if (ZombieCrash::stage==ZombieCrash::CHARACTER_SELECT && ZombieCrash::crownUnlocked)
          items[count++]=&ZombieCrash::outfitButton;
      } else if (ZombieCrash::stage==ZombieCrash::ARMORY) {
        for (int i=0;i<3;++i) items[count++]=&ZombieCrash::armoryChoices[i];
      } else if (ZombieCrash::stage!=ZombieCrash::ACTIVE_LEVEL) {
        items[count++]=&ZombieCrash::retryButton; items[count++]=&ZombieCrash::menuButton;
      }
      if (count) back=&ZombieCrash::exitButton;
      break;
  }
  bool redraw=UiNavigation::update(context,items,count,pressed);
  if (redraw) {
    if (gameState==HOME_MENU) drawHomeMenu();
    else if (gameState==SETTINGS) drawSettings();
    else if (gameState==GAME_LIST) drawGameList();
    else if (gameState==RESULTS) showResults(false);
    else if (gameState==YODA_PLAYING) drawYodaGame();
    else if (gameState==CITY_DRIVING) CityDrive::render(false,0,0);
    else if (gameState==ZOMBIE_CRASH) ZombieCrash::render(false,0,0,0);
  }
  const TouchButton *activate=(pressed & 8) ? back : (pressed & 4) ? UiNavigation::target : nullptr;
  if (activate) {
    UiNavigation::visible=false;
    x=activate->x+activate->w/2; y=activate->y+activate->h/2;
    touching=tapped=true;
  }
}

// BOOT is active-low GPIO0. Light sleep retains RAM and the current game.
constexpr uint8_t BOOT_BUTTON_PIN = 0;

void waitForBootRelease() {
  uint32_t releasedAt = millis();
  while (millis() - releasedAt < 40) {
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) releasedAt = millis();
    delay(10);
  }
}

void sleepFromBootButton() {
  const uint32_t pausedAt = millis();
  const bool soundEnabled = GameAudio::enabled;
  GameAudio::setEnabled(false);
  digitalWrite(TFT_BL, LOW);
  tft.writecommand(0x10); // Display sleep-in; display RAM is retained.
  delay(120);
  // Do not let the press that requested sleep immediately wake the ESP32.
  waitForBootRelease();
  esp_err_t error = gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
  if (error == ESP_OK) error = esp_sleep_enable_gpio_wakeup();
  if (error == ESP_OK) {
    Serial.flush();
    error = esp_light_sleep_start();
  }
  gpio_wakeup_disable(GPIO_NUM_0);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  // Consume the waking press as well, including switch bounce.
  waitForBootRelease();
  tft.writecommand(0x11); // Display sleep-out.
  delay(120);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  GameAudio::setEnabled(soundEnabled);

  const uint32_t pausedFor = millis() - pausedAt;
  levelStarted += pausedFor;
  lastGameFrame += pausedFor;
  for (auto &saucer : saucers) saucer.explosionStarted += pausedFor;
  lastYodaNeedUpdate += pausedFor;
  yodaBlinkStarted += pausedFor;
  nextYodaBlink += pausedFor;
  CityDrive::lastFrame += pausedFor;
  ZombieCrash::lastFrame += pausedFor;
  // A finger left on the screen must not become a new tap on waking.
  touchWasDown = touch.touched();
  if (error != ESP_OK) Serial.printf("Sleep failed: %s\n", esp_err_to_name(error));
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  // Deselect touch before initializing the shared display bus.
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  Serial.printf("Display init: BL=%d, CS=%d, DC=%d, SCLK=%d, MOSI=%d, MISO=%d\n",
                TFT_BL,TFT_CS,TFT_DC,TFT_SCLK,TFT_MOSI,TFT_MISO);
  tft.init();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.setRotation(3); // Opposite landscape orientation (180 degrees from rotation 1).
  // 8-bit color keeps the full-screen buffer below 80 KB, leaving ample RAM
  // for the ESP32 and ensuring the game level can always allocate its frame.
  gameFrame.setColorDepth(8);
  if (!gameFrame.createSprite(GameDisplay::width, GameDisplay::height)) {
    Serial.println("Game frame allocation failed");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED,TFT_BLACK);
    tft.drawString("Game frame allocation failed",10,10,2);
    while (true) delay(1000);
  }
#if GAME_TOUCH_SHARED_SPI
  touch.begin(tft.getSPIinstance());
  Serial.println("Touch: shared display SPI");
#else
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  Serial.println("Touch: separate SPI");
#endif
  // Keep the raw touch coordinate system used by saved calibration.
  touch.setRotation(1);
  ExpanderButtons::begin();
  randomSeed(micros());
  startButton = {(int16_t)((GameDisplay::width - 190) / 2), 181, 190, 48};
  chooseGameButton = {(int16_t)((GameDisplay::width - 190) / 2), 181, 190, 48};
  saucerMenuButton = {45, 81, 230, 33};
  yodaMenuButton = {45, 120, 230, 33};
  driveMenuButton = {45, 159, 230, 33};
  zombieMenuButton = {45, 198, 230, 33};
  backButton = {8, 6, 60, 25};
  feedButton = {10, 197, 92, 34};
  playButton = {114, 197, 92, 34};
  sleepButton = {218, 197, 92, 34};
  Serial.println("Checking touch calibration");
  touchCalibrate();
  DamageLed::begin();
  GameAudio::begin();
  Preferences soundPrefs;
  if (soundPrefs.begin("game-sound",true)) {
    GameAudio::setVolume(soundPrefs.getUChar("volume",4));
    GameAudio::setEnabled(soundPrefs.getBool("enabled",true));
    soundPrefs.end();
  }
  BatteryStatus::begin();
  drawHomeMenu();
  Serial.println("Home menu ready");
}

void loop() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    delay(30);
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      sleepFromBootButton();
      return;
    }
  }
  uint16_t x = 0, y = 0;
  bool touching = readTouch(x, y);
  bool tapped = touching && !touchWasDown;
  const uint32_t now = millis();
  ExpanderButtons::update(now);
  if (BatteryStatus::update(now) && gameState==HOME_MENU) {
    drawBatteryBadge();
    gameFrame.pushSprite(0,0);
  }
  const bool actualTouching=touching;
  navigateUi(ExpanderButtons::pressed,touching,tapped,x,y);

  if (gameState == HOME_MENU) {
    if (tapped && settingsButton.contains(x,y)) beginSettings();
    else if (tapped && chooseGameButton.contains(x, y)) drawGameList();
  } else if (gameState == SETTINGS) {
    updateSettings(tapped,x,y);
  } else if (gameState == GAME_LIST) {
    if (tapped && backButton.contains(x, y)) drawHomeMenu();
    else if (tapped && saucerMenuButton.contains(x, y)) beginGame();
    else if (tapped && yodaMenuButton.contains(x, y)) beginYodaGame();
    else if (tapped && driveMenuButton.contains(x, y)) beginCityDrive();
    else if (tapped && zombieMenuButton.contains(x, y)) beginZombieCrash();
  } else if (gameState == RESULTS) {
    const char *label = "AGAIN";
    if (tapped && backButton.contains(x, y)) {
      drawGameList();
    } else if (touching && startButton.contains(x, y)) {
      if (!buttonWasPressed) {
        buttonWasPressed = true;
        drawStartButton(true, label);
        UiNavigation::draw();
  gameFrame.pushSprite(0,0);
      }
    } else if (!touching && buttonWasPressed) {
      // Trigger only after a tap that began inside the button is released.
      beginGame();
    } else if (touching && buttonWasPressed) {
      // Letting the finger slide away cancels the tap and restores the button.
      buttonWasPressed = false;
      drawStartButton(false, label);
      UiNavigation::draw();
  gameFrame.pushSprite(0,0);
    }
  } else if (gameState == PLAYING) {
    updateGame(now, tapped, x, y);
  } else if (gameState == CITY_DRIVING) {
    updateCityDrive(now, touching, tapped, x, y, ExpanderButtons::held);
  } else if (gameState == ZOMBIE_CRASH) {
    updateZombieCrash(now, touching, tapped, x, y, ExpanderButtons::held, ExpanderButtons::pressed);
  } else {
    updateYodaGame(now, tapped, x, y);
  }

  GameAudio::engine(gameState==CITY_DRIVING && !CityDrive::finished ? CityDrive::speed : 0);
  GameAudio::update(millis());
  DamageLed::update(millis());
  touchWasDown = actualTouching;
  // Yield without adding 10 ms to every completed frame.
  delay(1);
}
