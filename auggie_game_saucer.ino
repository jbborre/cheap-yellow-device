// Auggie's Game - game menu and Space Saucer game for the ESP32 Cheap Yellow Display.
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
// Bump this when a fresh saved touch calibration is needed on the device.
constexpr uint32_t CALIBRATION_MAGIC = 0x41554735; // "AUG5"
constexpr int16_t CALIBRATION_INSET = 18;

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
} startButton, chooseGameButton, saucerMenuButton, yodaMenuButton, driveMenuButton, backButton;

enum GameState { HOME_MENU, GAME_LIST, PLAYING, RESULTS, YODA_PLAYING, CITY_DRIVING };
GameState gameState = HOME_MENU;
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

void drawMenuButton(const TouchButton &button, const char *label, uint16_t fill) {
  tft.fillRoundRect(button.x + 3, button.y + 4, button.w, button.h, 12, TFT_BLACK);
  tft.fillRoundRect(button.x, button.y, button.w, button.h, 12, COLOR_PINK);
  tft.fillRoundRect(button.x + 3, button.y + 3, button.w - 6, button.h - 6, 9, fill);
  tft.setTextColor(TFT_WHITE, fill);
  tft.setTextFont(4);
  tft.drawCentreString(label, button.x + button.w / 2, button.y + 10, 4);
}

void drawBackButton(const TouchButton &button, uint8_t font = 2) {
  tft.fillRoundRect(button.x, button.y, button.w, button.h, 8, COLOR_PURPLE);
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
  tft.setTextFont(font);
  tft.setTextColor(TFT_WHITE, COLOR_PURPLE);
  tft.drawCentreString("BACK", button.x + button.w / 2,
                       button.y + (font == 4 ? 12 : 7), font);
}

void drawHomeMenu() {
  gameState = HOME_MENU;
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
  drawCentered("READY TO PLAY?", 151, 2, TFT_WHITE);
  drawMenuButton(chooseGameButton, "CHOOSE GAME", COLOR_PURPLE);
}

void drawGameList() {
  gameState = GAME_LIST;
  buttonWasPressed = false;
  backButton = {8, 6, 60, 25};
  tft.fillScreen(COLOR_SKY);
  drawStars();
  drawBackButton(backButton);
  drawCentered("GAME LIST", 43, 4, COLOR_GOLD);
  drawCentered("PICK ONE TO PLAY", 78, 2, TFT_WHITE);
  drawMenuButton(saucerMenuButton, "SPACE SAUCER", COLOR_PURPLE);
  drawMenuButton(yodaMenuButton, "BABY YODA", 0x2585);
  drawMenuButton(driveMenuButton, "CITY DRIVE", 0x0357);
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
  startButton = {170, 181, 140, 48};
  backButton = {10, 181, 140, 48};
  tft.fillScreen(COLOR_SKY);
  drawStars();
  drawCentered("TIME'S UP!", 54, 4, COLOR_PINK);
  drawCentered("SAUCERS DOWN", 95, 2, TFT_WHITE);
  drawCentered(String(score).c_str(), 116, 6, COLOR_GOLD);
  drawStartButton(false, "AGAIN");
  drawBackButton(backButton, 4);
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

uint8_t increaseNeed(uint8_t value, uint8_t amount) {
  return min(100, value + amount);
}

uint8_t decreaseNeed(uint8_t value, uint32_t amount) {
  return amount >= value ? 0 : value - amount;
}

void drawNeedBar(const char *label, int16_t y, uint8_t value, uint16_t color) {
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, COLOR_SKY);
  tft.drawString(label, 10, y, 2);
  tft.drawRoundRect(82, y + 1, 225, 13, 6, TFT_WHITE);
  tft.fillRoundRect(85, y + 4, (217 * value) / 100, 7, 3, color);
}

void drawYoda(bool blinking) {
  const int16_t x = tft.width() / 2;
  const int16_t y = 132;
  const uint16_t green = 0x7E8B;
  const uint16_t greenShadow = 0x34C5;
  const uint16_t earInner = 0xE3B4;
  const uint16_t robe = 0xC618;
  const uint16_t robeShadow = 0x9CD3;

  // Layered ears, eyes, and robe give the small pet more depth at CYD resolution.
  tft.fillTriangle(x - 33, y - 20, x - 82, y - 5, x - 38, y + 7, greenShadow);
  tft.fillTriangle(x + 33, y - 20, x + 82, y - 5, x + 38, y + 7, greenShadow);
  tft.fillTriangle(x - 37, y - 15, x - 70, y - 5, x - 38, y + 2, earInner);
  tft.fillTriangle(x + 37, y - 15, x + 70, y - 5, x + 38, y + 2, earInner);
  tft.fillEllipse(x, y, 45, 39, green);
  tft.drawEllipse(x, y, 45, 39, TFT_WHITE);
  tft.fillEllipse(x, y + 30, 33, 10, greenShadow);

  // Forehead creases and a tiny nose make the face feel less like a simple icon.
  tft.drawLine(x - 18, y - 22, x - 10, y - 16, greenShadow);
  tft.drawLine(x + 18, y - 22, x + 10, y - 16, greenShadow);
  tft.fillCircle(x, y + 8, 3, greenShadow);
  tft.drawLine(x - 7, y + 17, x, y + 19, TFT_BLACK);
  tft.drawLine(x, y + 19, x + 7, y + 17, TFT_BLACK);

  if (blinking) {
    tft.drawFastHLine(x - 23, y - 4, 15, TFT_BLACK);
    tft.drawFastHLine(x + 8, y - 4, 15, TFT_BLACK);
  } else {
    tft.fillEllipse(x - 16, y - 4, 8, 11, TFT_BLACK);
    tft.fillEllipse(x + 16, y - 4, 8, 11, TFT_BLACK);
    tft.fillCircle(x - 14, y - 8, 2, TFT_WHITE);
    tft.fillCircle(x + 18, y - 8, 2, TFT_WHITE);
  }

  tft.fillRoundRect(x - 45, y + 32, 91, 32, 13, robeShadow);
  tft.fillRoundRect(x - 39, y + 34, 79, 29, 12, robe);
  tft.drawRoundRect(x - 39, y + 34, 79, 29, 12, TFT_WHITE);
  tft.fillTriangle(x - 22, y + 34, x, y + 55, x - 2, y + 34, TFT_WHITE);
  tft.fillCircle(x - 39, y + 52, 6, green);
  tft.fillCircle(x + 39, y + 52, 6, green);
}

void drawPetButton(const TouchButton &button, const char *label, uint16_t fill) {
  tft.fillRoundRect(button.x, button.y, button.w, button.h, 8, fill);
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, fill);
  tft.drawCentreString(label, button.x + button.w / 2, button.y + 10, 2);
}

void drawYodaGame() {
  tft.fillScreen(COLOR_SKY);
  drawBackButton(backButton);
  drawCentered("RAISE BABY YODA", 6, 4, COLOR_GOLD);
  drawNeedBar("FOOD", 37, yodaHunger, COLOR_GOLD);
  drawNeedBar("FUN", 55, yodaHappiness, COLOR_PINK);
  drawNeedBar("REST", 73, yodaEnergy, COLOR_MINT);
  drawYoda(yodaBlinking);
  drawPetButton(feedButton, "FEED", COLOR_PURPLE);
  drawPetButton(playButton, "PLAY", COLOR_PINK);
  drawPetButton(sleepButton, "SLEEP", 0x2585);
}

void beginYodaGame() {
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

  bool redrawForBlink = false;
  if (!yodaBlinking && (int32_t)(now - nextYodaBlink) >= 0) {
    yodaBlinking = true;
    yodaBlinkStarted = now;
    redrawForBlink = true;
  } else if (yodaBlinking && now - yodaBlinkStarted >= 150) {
    yodaBlinking = false;
    nextYodaBlink = now + random(1800, 4200);
    redrawForBlink = true;
  }
  if (redrawForBlink) drawYodaGame();

  if (now - lastYodaNeedUpdate >= 5000) {
    const uint32_t intervals = (now - lastYodaNeedUpdate) / 5000;
    yodaHunger = decreaseNeed(yodaHunger, 4UL * intervals);
    yodaHappiness = decreaseNeed(yodaHappiness, 3UL * intervals);
    yodaEnergy = decreaseNeed(yodaEnergy, 2UL * intervals);
    lastYodaNeedUpdate += intervals * 5000;
    drawYodaGame();
  }

  if (!tapped) return;
  if (feedButton.contains(touchX, touchY)) yodaHunger = increaseNeed(yodaHunger, 18);
  else if (playButton.contains(touchX, touchY)) {
    yodaHappiness = increaseNeed(yodaHappiness, 18);
    yodaEnergy = yodaEnergy > 7 ? yodaEnergy - 7 : 0;
  } else if (sleepButton.contains(touchX, touchY)) yodaEnergy = increaseNeed(yodaEnergy, 22);
  else return;
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
    File file = SPIFFS.open("/AuggieTouchCal_v5", "r");
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
  drawCalibrationTarget(CALIBRATION_INSET, CALIBRATION_INSET);
  TS_Point topLeft = waitForCalibrationTouch();

  tft.fillScreen(TFT_BLACK);
  drawCentered("TOUCH CALIBRATION", 68, 2, COLOR_GOLD);
  drawCentered("Touch each crosshair", 94, 2, TFT_WHITE);
  drawCalibrationTarget(tft.width() - 1 - CALIBRATION_INSET,
                        tft.height() - 1 - CALIBRATION_INSET);
  TS_Point bottomRight = waitForCalibrationTouch();

  touchCalibration = {CALIBRATION_MAGIC,
                      static_cast<uint16_t>(topLeft.x), static_cast<uint16_t>(topLeft.y),
                      static_cast<uint16_t>(bottomRight.x), static_cast<uint16_t>(bottomRight.y)};
  File file = SPIFFS.open("/AuggieTouchCal_v5", "w");
  if (file) {
    file.write((const uint8_t *)&touchCalibration, sizeof(touchCalibration));
    file.close();
  }
}

#include "include/CityDrive.h"

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
  chooseGameButton = {(int16_t)((tft.width() - 190) / 2), 181, 190, 48};
  saucerMenuButton = {(int16_t)((tft.width() - 230) / 2), 102, 230, 36};
  yodaMenuButton = {(int16_t)((tft.width() - 230) / 2), 145, 230, 36};
  driveMenuButton = {45, 188, 230, 36};
  backButton = {8, 6, 60, 25};
  feedButton = {10, 197, 92, 34};
  playButton = {114, 197, 92, 34};
  sleepButton = {218, 197, 92, 34};
  touchCalibrate();
  drawHomeMenu();
}

void loop() {
  uint16_t x = 0, y = 0;
  const bool touching = readTouch(x, y);
  const bool tapped = touching && !touchWasDown;
  const uint32_t now = millis();

  if (gameState == HOME_MENU) {
    if (tapped && chooseGameButton.contains(x, y)) drawGameList();
  } else if (gameState == GAME_LIST) {
    if (tapped && backButton.contains(x, y)) drawHomeMenu();
    else if (tapped && saucerMenuButton.contains(x, y)) beginGame();
    else if (tapped && yodaMenuButton.contains(x, y)) beginYodaGame();
    else if (tapped && driveMenuButton.contains(x, y)) beginCityDrive();
  } else if (gameState == RESULTS) {
    const char *label = "AGAIN";
    if (tapped && backButton.contains(x, y)) {
      drawGameList();
    } else if (touching && startButton.contains(x, y)) {
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
  } else if (gameState == PLAYING) {
    updateGame(now, tapped, x, y);
  } else if (gameState == CITY_DRIVING) {
    updateCityDrive(now, touching, tapped, x, y);
  } else {
    updateYodaGame(now, tapped, x, y);
  }

  touchWasDown = touching;
  delay(10);
}
