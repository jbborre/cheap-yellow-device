// Auggie's Game - touch splash screen for the ESP32 Cheap Yellow Display.
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Free_Fonts.h"

TFT_eSPI tft;
TFT_eSprite gameFrame(&tft);

// The CYD's XPT2046 controller is wired to a separate SPI bus from the TFT.
constexpr uint8_t TOUCH_CS = 33;
constexpr uint8_t TOUCH_IRQ = 36;
constexpr uint8_t TOUCH_MOSI = 32;
constexpr uint8_t TOUCH_MISO = 39;
constexpr uint8_t TOUCH_SCLK = 25;
constexpr uint32_t CALIBRATION_MAGIC = 0x41554734; // "AUG4"

SPIClass touchSPI(VSPI);
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
} touchCalibration;

struct TouchButton {
  int16_t x, y, w, h;

  bool contains(uint16_t tx, uint16_t ty) const {
    return tx >= x && tx < x + w && ty >= y && ty < y + h;
  }
} startButton;

enum GameState { SPLASH, PLAYING, RESULTS };
GameState gameState = SPLASH;
bool buttonWasPressed = false;
bool touchWasDown = false;

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
uint32_t lastGameFrame = 0;

void drawCentered(const char *text, int16_t y, uint8_t font, uint16_t color) {
  tft.setTextFont(font);
  tft.setTextColor(color, COLOR_SKY);
  tft.drawCentreString(text, tft.width() / 2, y, font);
}

void drawFreeCentered(const char *text, int16_t y, uint16_t color) {
  tft.setTextColor(color);
  tft.drawString(text, (tft.width() - tft.textWidth(text)) / 2, y);
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
    tft.fillCircle(star[0], star[1], star[2], star[2] == 2 ? COLOR_MINT : TFT_WHITE);
  }
}

void drawGameStars() {
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

  tft.fillRoundRect(x + 3, y + 4, startButton.w, startButton.h, 14, TFT_BLACK);
  tft.fillRoundRect(x, y, startButton.w, startButton.h, 14, edge);
  tft.fillRoundRect(x + 3, y + 3, startButton.w - 6, startButton.h - 6, 11, fill);

  tft.setTextFont(4);
  tft.setTextColor(TFT_BLACK, fill);
  tft.drawCentreString(label, x + startButton.w / 2 + 1, y + 14, 4);
  tft.setTextColor(TFT_WHITE, fill);
  tft.drawCentreString(label, x + startButton.w / 2, y + 12, 4);
}

void drawSplash() {
  gameState = SPLASH;
  buttonWasPressed = false;
  tft.fillScreen(COLOR_SKY);
  drawStars();

  // Layered, offset lettering gives the title a neon arcade look.
  tft.setFreeFont(FSSBO18);
  drawFreeCentered("AUGGIE'S", 52, TFT_BLACK);
  drawFreeCentered("AUGGIE'S", 49, COLOR_PINK);

  tft.setFreeFont(FSSBO24);
  drawFreeCentered("GAME", 96, TFT_BLACK);
  drawFreeCentered("GAME", 92, COLOR_GOLD);
  tft.setFreeFont(NULL);

  tft.drawFastHLine(75, 145, 170, COLOR_MINT);
  drawCentered("TOUCH TO PLAY", 151, 2, TFT_WHITE);
  drawStartButton(false);
}

void respawnSaucer(Saucer &saucer) {
  const bool large = random(0, 3) == 0;
  saucer.radius = large ? 23 : 14;
  saucer.hitsLeft = large ? 2 : 1;
  saucer.x = random(saucer.radius, tft.width() - saucer.radius);
  saucer.y = random(43 + saucer.radius, tft.height() - saucer.radius);
  const float speed = large ? random(65, 115) / 100.0f : random(100, 270) / 100.0f;
  saucer.vx = (random(0, 2) ? speed : -speed);
  saucer.vy = (random(0, 2) ? speed * 0.70f : -speed * 0.70f);
  saucer.exploding = false;
}

void drawSaucer(const Saucer &saucer) {
  const int16_t x = (int16_t)saucer.x;
  const int16_t y = (int16_t)saucer.y;
  const int16_t r = saucer.radius;
  if (saucer.exploding) {
    const int16_t blast = r + 8;
    gameFrame.fillCircle(x, y, blast, COLOR_GOLD);
    gameFrame.fillCircle(x, y, blast - 5, COLOR_PINK);
    gameFrame.drawFastHLine(x - blast - 4, y, blast * 2 + 8, TFT_WHITE);
    gameFrame.drawFastVLine(x, y - blast - 4, blast * 2 + 8, TFT_WHITE);
    return;
  }

  const uint16_t hull = saucer.hitsLeft == 2 ? COLOR_PINK : COLOR_MINT;
  gameFrame.fillEllipse(x, y + 3, r, r / 2, hull);
  gameFrame.drawEllipse(x, y + 3, r, r / 2, TFT_WHITE);
  gameFrame.fillCircle(x, y - r / 4, r / 2, COLOR_PURPLE);
  gameFrame.drawCircle(x, y - r / 4, r / 2, TFT_WHITE);
  gameFrame.fillCircle(x - r / 2, y + 5, 2, COLOR_GOLD);
  gameFrame.fillCircle(x + r / 2, y + 5, 2, COLOR_GOLD);
  if (saucer.hitsLeft == 2) gameFrame.fillCircle(x, y + r / 2 + 5, 3, COLOR_PINK);
}

void drawGame(uint32_t elapsed) {
  gameFrame.fillSprite(COLOR_SKY);
  drawGameStars();
  const uint8_t secondsLeft = (LEVEL_DURATION_MS - elapsed + 999) / 1000;
  gameFrame.setTextFont(2);
  gameFrame.setTextColor(TFT_WHITE, COLOR_SKY);
  gameFrame.drawString("SCORE: " + String(score), 8, 8, 2);
  gameFrame.setTextColor(COLOR_GOLD, COLOR_SKY);
  gameFrame.drawRightString("TIME: " + String(secondsLeft), tft.width() - 8, 8, 2);
  gameFrame.drawFastHLine(0, 31, tft.width(), COLOR_PURPLE);
  for (const Saucer &saucer : saucers) drawSaucer(saucer);
  gameFrame.pushSprite(0, 0);
}

void beginGame() {
  gameState = PLAYING;
  score = 0;
  levelStarted = millis();
  lastGameFrame = levelStarted;
  for (Saucer &saucer : saucers) respawnSaucer(saucer);
}

void showResults() {
  gameState = RESULTS;
  buttonWasPressed = false;
  tft.fillScreen(COLOR_SKY);
  drawStars();
  drawCentered("TIME'S UP!", 54, 4, COLOR_PINK);
  drawCentered("SAUCERS DOWN", 95, 2, TFT_WHITE);
  drawCentered(String(score).c_str(), 116, 6, COLOR_GOLD);
  drawStartButton(false, "AGAIN");
}

void updateGame(uint32_t now, bool tapped, uint16_t touchX, uint16_t touchY) {
  const uint32_t elapsed = now - levelStarted;
  if (elapsed >= LEVEL_DURATION_MS) {
    showResults();
    return;
  }

  if (tapped) {
    for (Saucer &saucer : saucers) {
      if (saucer.exploding) continue;
      const float dx = touchX - saucer.x;
      const float dy = (touchY - saucer.y) * 2.0f;
      if (dx * dx + dy * dy <= saucer.radius * saucer.radius) {
        if (--saucer.hitsLeft == 0) {
          score++;
          saucer.exploding = true;
          saucer.explosionStarted = now;
        }
        break; // One tap can damage only one saucer.
      }
    }
  }

  const float frameScale = min(now - lastGameFrame, (uint32_t)50) / 25.0f;
  lastGameFrame = now;
  for (Saucer &saucer : saucers) {
    if (saucer.exploding) {
      if (now - saucer.explosionStarted > 130) respawnSaucer(saucer);
      continue;
    }
    saucer.x += saucer.vx * frameScale;
    saucer.y += saucer.vy * frameScale;
    if (saucer.x < saucer.radius || saucer.x > tft.width() - saucer.radius) saucer.vx = -saucer.vx;
    if (saucer.y < 38 + saucer.radius || saucer.y > tft.height() - saucer.radius) saucer.vy = -saucer.vy;
  }
  drawGame(elapsed);
}

bool readTouch(uint16_t &x, uint16_t &y) {
  if (!touch.touched()) return false;

  TS_Point point = touch.getPoint();
  long screenX = map(point.x, touchCalibration.topLeftX,
                     touchCalibration.bottomRightX, 0, tft.width() - 1);
  long screenY = map(point.y, touchCalibration.topLeftY,
                     touchCalibration.bottomRightY, 0, tft.height() - 1);
  x = constrain(screenX, 0, tft.width() - 1);
  y = constrain(screenY, 0, tft.height() - 1);
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
    File file = SPIFFS.open("/AuggieTouchCal_v4", "r");
    valid = file && file.readBytes((char *)&touchCalibration, sizeof(touchCalibration)) == sizeof(touchCalibration);
    if (file) file.close();
    valid = valid && touchCalibration.magic == CALIBRATION_MAGIC &&
            abs((int)touchCalibration.topLeftX - touchCalibration.bottomRightX) > 1000 &&
            abs((int)touchCalibration.topLeftY - touchCalibration.bottomRightY) > 1000;
  }
  if (valid) return;

  tft.fillScreen(TFT_BLACK);
  drawCentered("TOUCH CALIBRATION", 68, 2, COLOR_GOLD);
  drawCentered("Touch each crosshair", 94, 2, TFT_WHITE);
  drawCalibrationTarget(18, 18);
  TS_Point topLeft = waitForCalibrationTouch();

  tft.fillScreen(TFT_BLACK);
  drawCentered("TOUCH CALIBRATION", 68, 2, COLOR_GOLD);
  drawCentered("Touch each crosshair", 94, 2, TFT_WHITE);
  drawCalibrationTarget(tft.width() - 19, tft.height() - 19);
  TS_Point bottomRight = waitForCalibrationTouch();

  touchCalibration = {CALIBRATION_MAGIC,
                      static_cast<uint16_t>(topLeft.x), static_cast<uint16_t>(topLeft.y),
                      static_cast<uint16_t>(bottomRight.x), static_cast<uint16_t>(bottomRight.y)};
  File file = SPIFFS.open("/AuggieTouchCal_v4", "w");
  if (file) {
    file.write((const uint8_t *)&touchCalibration, sizeof(touchCalibration));
    file.close();
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1); // 320 x 240 landscape
  // 8-bit color keeps the full-screen buffer below 80 KB, leaving ample RAM
  // for the ESP32 and ensuring the game level can always allocate its frame.
  gameFrame.setColorDepth(8);
  if (!gameFrame.createSprite(tft.width(), tft.height())) {
    Serial.println("Game frame allocation failed");
  }
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  randomSeed(micros());
  startButton = {(int16_t)((tft.width() - 190) / 2), 181, 190, 48};
  touchCalibrate();
  drawSplash();
}

void loop() {
  uint16_t x, y;
  const bool touching = readTouch(x, y);
  const bool tapped = touching && !touchWasDown;
  const uint32_t now = millis();

  if (gameState == SPLASH || gameState == RESULTS) {
    const char *label = gameState == SPLASH ? "START" : "AGAIN";
    if (touching && startButton.contains(x, y)) {
      if (!buttonWasPressed) {
        buttonWasPressed = true;
        drawStartButton(true, label);
      }
    } else if (!touching && buttonWasPressed) {
      // Trigger only after a tap that began inside the button is released.
      beginGame();
    } else if (touching && buttonWasPressed) {
      // Letting the finger slide away cancels the tap and restores the button.
      buttonWasPressed = false;
      drawStartButton(false, label);
    }
  } else {
    updateGame(now, tapped, x, y);
  }

  touchWasDown = touching;
  delay(10);
}
