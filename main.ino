// Evie's Arcade - menus, settings and input for the ESP32 Cheap Yellow Display.
// Each game lives in its own header under include/.
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
} chooseGameButton, squishyMenuButton, memoryMenuButton, saucerMenuButton, yodaMenuButton, driveMenuButton, zombieMenuButton, backButton;

#include "include/UiNavigation.h"

enum GameState { HOME_MENU, GAME_LIST, SQUISHY_MAKER, SAUCER_PLAYING, SAUCER_RESULTS, YODA_PLAYING, CITY_DRIVING, ZOMBIE_CRASH, BLACK_KNIGHT, SETTINGS, MEMORY_MATCH };
GameState gameState = HOME_MENU;
bool touchWasDown = false;
const TouchButton settingsButton={5,5,36,30};
const TouchButton settingsBackButton={8,6,60,25};
const TouchButton soundToggleButton={55,75,210,42};
const TouchButton volumeDownButton={45,145,62,44};
const TouchButton volumeUpButton={213,145,62,44};

void drawCentered(const char *text, int16_t y, uint8_t font, uint16_t color) {
  gameFrame.setTextFont(font);
  gameFrame.setTextColor(color, COLOR_SKY);
  gameFrame.drawCentreString(text, gameFrame.width() / 2, y, font);
}

void drawFreeCentered(const char *text, int16_t y, uint16_t color) {
  gameFrame.setTextColor(color);
  gameFrame.drawString(text, (gameFrame.width() - gameFrame.textWidth(text)) / 2, y);
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
  using GameGfx::rgb;
  const bool low=BatteryStatus::percentage>=0 && BatteryStatus::percentage<=20;
  gameFrame.fillRoundRect(244,7,70,20,10,TFT_WHITE);
  gameFrame.drawRoundRect(244,7,70,20,10,low ? rgb(255,0,85) : rgb(219,73,182));
  // Battery icon with a fill proportional to the charge.
  gameFrame.drawRect(251,12,16,10,rgb(182,36,146));
  gameFrame.fillRect(267,15,2,4,rgb(182,36,146));
  if (BatteryStatus::percentage>0)
    gameFrame.fillRect(253,14,max(1,12*BatteryStatus::percentage/100),6,low ? rgb(255,0,85) : rgb(73,219,146));
  gameFrame.setTextColor(rgb(182,36,146));
  char label[8];
  if (BatteryStatus::percentage<0) snprintf(label,sizeof(label),"--%%");
  else snprintf(label,sizeof(label),"%d%%",BatteryStatus::percentage);
  gameFrame.drawRightString(label,308,10,2);
}

uint32_t lastMenuFrame = 0;

// Solid color bands instead of a dithered gradient: RGB332 dithering reads as
// a grainy texture on large areas, while bands stay clean.
void bands(int x, int y, int w, int h, uint16_t top, uint16_t bottom, int steps) {
  for (int i=0;i<steps;++i) {
    const int y0=y+h*i/steps, y1=y+h*(i+1)/steps;
    gameFrame.fillRect(x,y0,w,y1-y0,GameGfx::mix565(top,bottom,i,steps-1));
  }
}

void drawTitle(const char *text, int16_t y, uint16_t color, uint16_t outline) {
  // A thick outline plus a soft drop shadow keeps the title readable over the sky.
  drawFreeCentered(text, y + 4, GameGfx::rgb(182,73,182));
  for (int dx=-2; dx<=2; dx+=2)
    for (int dy=-2; dy<=2; dy+=2)
      if (dx || dy) {
        gameFrame.setTextColor(outline);
        gameFrame.drawString(text, (gameFrame.width()-gameFrame.textWidth(text))/2+dx, y+dy);
      }
  drawFreeCentered(text, y, color);
}

// Text on a solid pill so it stays legible over the scenery.
void drawLabelPill(const char *text, int16_t y, uint16_t color, uint16_t border) {
  const int w=gameFrame.textWidth(text,2)+28;
  const int x=(gameFrame.width()-w)/2;
  gameFrame.fillRoundRect(x,y,w,22,11,TFT_WHITE);
  gameFrame.drawRoundRect(x,y,w,22,11,border);
  gameFrame.setTextColor(color);
  gameFrame.drawCentreString(text,160,y+3,2);
  gameFrame.drawCentreString(text,161,y+3,2); // Double strike for bolder text.
}

void drawHeartShape(int x, int y, int r, uint16_t c) {
  gameFrame.fillCircle(x-r,y,r,c);
  gameFrame.fillCircle(x+r,y,r,c);
  gameFrame.fillTriangle(x-2*r,y+1,x+2*r,y+1,x,y+2*r+2,c);
}

void drawCloud(int x, int y, int s) {
  using GameGfx::rgb;
  gameFrame.fillEllipse(x,y+s/3,s*2,s/2+2,rgb(255,219,255));
  gameFrame.fillCircle(x-s,y+s/4,s*2/3,TFT_WHITE);
  gameFrame.fillCircle(x,y,s,TFT_WHITE);
  gameFrame.fillCircle(x+s,y+s/5,s*3/4,TFT_WHITE);
}

// Pastel sky with drifting clouds, floating hearts and twinkling sparkles.
void drawPastelBackdrop(uint32_t now) {
  using GameGfx::rgb;
  bands(0,0,320,240,rgb(255,182,219),rgb(219,182,255),12);
  for (int i=0;i<3;++i) {
    const int x=((i*130+(int)(now/90))%440+440)%440-60;
    drawCloud(x,48+i*38,10+i*2);
  }
  for (int i=0;i<8;++i) {
    const int x=(i*67+23)%320;
    const int y=200-(int)((now/40+i*37)%200);
    drawHeartShape(x,y,2+i%2,i%3 ? rgb(255,146,219) : rgb(255,255,255));
  }
  for (int i=0;i<18;++i) {
    const int x=(i*97+31)%320, y=(i*53+11)%170;
    const int phase=(now/120+i*7)%24;
    const uint16_t c=i%3==0 ? rgb(255,219,85) : TFT_WHITE;
    if (phase<3) {
      gameFrame.drawFastHLine(x-3,y,7,c);
      gameFrame.drawFastVLine(x,y-3,7,c);
    } else if (phase<18) gameFrame.drawPixel(x,y,c);
  }
}

// Rolling candy hills along the bottom.
void drawCandyHills() {
  using GameGfx::rgb;
  gameFrame.fillEllipse(70,262,150,70,rgb(255,146,219));
  gameFrame.fillEllipse(260,268,150,72,rgb(219,146,255));
  gameFrame.fillEllipse(160,280,220,62,rgb(255,182,219));
  for (int i=0;i<6;++i) {
    const int x=20+i*56, y=226+(i%2)*6;
    gameFrame.fillCircle(x,y,4,TFT_WHITE);
    gameFrame.fillCircle(x,y,2,i%2 ? rgb(255,73,146) : rgb(255,219,85));
  }
}

void drawHomeScene(uint32_t now) {
  using GameGfx::rgb;
  drawPastelBackdrop(now);
  // Rainbow arc behind the title.
  const uint16_t rainbow[6]={rgb(255,109,109),rgb(255,182,85),rgb(255,255,109),
                             rgb(146,255,146),rgb(109,182,255),rgb(219,146,255)};
  for (int i=0;i<6;++i) {
    gameFrame.fillCircle(160,150,128-i*7,rainbow[i]);
  }
  gameFrame.fillCircle(160,150,85,rgb(255,219,255));
  bands(0,150,320,90,rgb(255,219,255),rgb(219,182,255),6);
  drawCloud(36,142,14);
  drawCloud(286,138,14);
  // A heart balloon bobbing beside the title.
  const int by=70+(int)(5*sinf(now/400.0f));
  gameFrame.drawLine(276,by+14,270,by+46,rgb(182,73,182));
  drawHeartShape(276,by,8,rgb(255,73,146));
  gameFrame.fillCircle(270,by-3,2,TFT_WHITE);
  drawCandyHills();
}

void drawHomeMenu(uint32_t now) {
  using GameGfx::rgb;
  gameState = HOME_MENU;
  lastMenuFrame = now;
  drawHomeScene(now);

  gameFrame.setFreeFont(FSSBO18);
  drawTitle("EVIE'S", 54, TFT_WHITE, rgb(255,73,182));
  gameFrame.setFreeFont(FSSBO24);
  drawTitle("ARCADE", 98, rgb(255,255,109), rgb(182,73,255));
  gameFrame.setFreeFont(NULL);

  drawLabelPill("READY TO PLAY?",150,(now/600)%2 ? rgb(219,36,146) : rgb(146,73,219),rgb(255,73,182));

  const TouchButton &b=chooseGameButton;
  gameFrame.fillRoundRect(b.x+2,b.y+5,b.w,b.h,20,rgb(182,73,182));
  gameFrame.fillRoundRect(b.x,b.y,b.w,b.h,20,TFT_WHITE);
  gameFrame.fillRoundRect(b.x+3,b.y+3,b.w-6,b.h-6,17,rgb(255,73,182));
  gameFrame.fillRoundRect(b.x+3,b.y+3,b.w-6,(b.h-6)/2,17,rgb(255,146,219));
  gameFrame.fillRect(b.x+3,b.y+3+(b.h-6)/2-10,b.w-6,10,rgb(255,146,219));
  gameFrame.drawFastHLine(b.x+20,b.y+6,b.w-40,TFT_WHITE);
  drawHeartShape(b.x+18,b.y+b.h/2-2,4,TFT_WHITE);
  drawHeartShape(b.x+b.w-18,b.y+b.h/2-2,4,TFT_WHITE);
  gameFrame.setTextColor(rgb(182,36,146));
  gameFrame.drawCentreString("CHOOSE GAME",b.x+b.w/2+1,b.y+14,4);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString("CHOOSE GAME",b.x+b.w/2,b.y+12,4);

  const TouchButton &g=settingsButton;
  gameFrame.fillRoundRect(g.x,g.y,g.w,g.h,9,rgb(255,73,182));
  gameFrame.drawRoundRect(g.x,g.y,g.w,g.h,9,TFT_WHITE);
  const float spin=now/1500.0f;
  for (int i=0;i<8;++i) {
    float angle=i*PI/4+spin;
    gameFrame.drawWideLine(23+cosf(angle)*8,20+sinf(angle)*8,
                           23+cosf(angle)*12,20+sinf(angle)*12,4,TFT_WHITE);
  }
  gameFrame.fillCircle(23,20,9,TFT_WHITE);
  gameFrame.fillCircle(23,20,4,rgb(255,73,182));
  drawBatteryBadge();
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void drawHomeMenu() {
  GameAudio::stop();
  drawHomeMenu(millis());
}

// Small hand-drawn icon for each game card, centered on (x, y).
void drawGameIcon(int game, int x, int y, uint32_t now) {
  using GameGfx::rgb;
  if (game==4) {
    // Little pink squishy with a happy face.
    gameFrame.fillEllipse(x,y+2,13,9,rgb(255,182,219));
    gameFrame.fillCircle(x-4,y+1,2,rgb(36,0,36));
    gameFrame.fillCircle(x+4,y+1,2,rgb(36,0,36));
    gameFrame.fillEllipse(x,y+6,2,1,rgb(182,0,73));
    gameFrame.fillCircle(x+8,y-6,1,TFT_WHITE);
  } else if (game==5) {
    // A face-down card with a heart overlapping a face-up one.
    gameFrame.fillRoundRect(x-3,y-11,14,20,3,TFT_WHITE);
    gameFrame.fillCircle(x+4,y-1,4,rgb(255,182,85));
    gameFrame.fillRoundRect(x-12,y-8,14,20,3,rgb(255,73,146));
    gameFrame.drawRoundRect(x-12,y-8,14,20,3,TFT_WHITE);
    drawHeartShape(x-5,y,2,TFT_WHITE);
  } else if (game==0) {
    gameFrame.fillEllipse(x,y-3,6,5,rgb(146,219,255));
    gameFrame.fillEllipse(x,y+2,13,4,rgb(182,182,170));
    for (int i=0;i<3;++i) gameFrame.drawPixel(x-6+i*6,y+2,(i+now/200)%3 ? rgb(255,73,85) : COLOR_GOLD);
  } else if (game==1) {
    const uint16_t green=rgb(146,182,85);
    gameFrame.fillTriangle(x-6,y-3,x-15,y-5,x-6,y+3,green);
    gameFrame.fillTriangle(x+6,y-3,x+15,y-5,x+6,y+3,green);
    gameFrame.fillCircle(x,y,8,green);
    gameFrame.fillCircle(x-3,y,2,TFT_BLACK);
    gameFrame.fillCircle(x+3,y,2,TFT_BLACK);
    gameFrame.fillRect(x-6,y+8,12,4,rgb(182,146,85));
  } else if (game==2) {
    gameFrame.fillRoundRect(x-12,y-2,24,9,3,rgb(255,73,0));
    gameFrame.fillRoundRect(x-7,y-8,14,8,3,rgb(255,73,0));
    gameFrame.fillRect(x-5,y-6,10,4,rgb(146,219,255));
    gameFrame.fillCircle(x-7,y+7,3,TFT_BLACK);
    gameFrame.fillCircle(x+7,y+7,3,TFT_BLACK);
  } else {
    const uint16_t skin=rgb(109,182,85);
    gameFrame.fillRoundRect(x-8,y-10,16,18,5,skin);
    gameFrame.fillRect(x-5,y-4,4,3,rgb(255,36,0));
    gameFrame.fillRect(x+1,y-4,4,3,rgb(255,36,0));
    gameFrame.drawFastHLine(x-4,y+3,8,TFT_BLACK);
    for (int i=-3;i<=3;i+=3) gameFrame.drawFastVLine(x+i,y+2,3,TFT_BLACK);
    gameFrame.fillRect(x-8,y-11,16,3,rgb(73,36,0));
  }
}

void drawGameCard(const TouchButton &b, int game, const char *label, uint16_t accent, uint32_t now) {
  using GameGfx::rgb;
  gameFrame.fillRoundRect(b.x+2,b.y+3,b.w,b.h,12,rgb(182,73,182));
  gameFrame.fillRoundRect(b.x,b.y,b.w,b.h,12,accent);
  gameFrame.fillRoundRect(b.x+2,b.y+2,b.w-4,b.h-4,10,rgb(255,255,255));
  // Icon tile on the left in the game's accent color.
  gameFrame.fillRoundRect(b.x+4,b.y+4,b.h-8+10,b.h-8,6,GameGfx::darker(accent));
  drawGameIcon(game,b.x+4+(b.h+2)/2,b.y+b.h/2,now);
  // Fall back to the small font if a long name would reach the chevron.
  const int textX=b.x+b.h+18, font=gameFrame.textWidth(label,4)<=b.x+b.w-24-textX ? 4 : 2;
  gameFrame.setTextColor(rgb(146,36,146));
  gameFrame.drawString(label,textX,b.y+(b.h-(font==4 ? 18 : 14))/2,font);
  // Chevron that nudges right in time with the stars.
  const int cx=b.x+b.w-16+((now/300)%2);
  gameFrame.fillTriangle(cx,b.y+b.h/2-6,cx+7,b.y+b.h/2,cx,b.y+b.h/2+6,accent);
}

void drawGameList(uint32_t now) {
  using GameGfx::rgb;
  gameState = GAME_LIST;
  lastMenuFrame = now;
  backButton = {8, 6, 60, 25};
  drawPastelBackdrop(now);
  drawCandyHills();
  drawBackButton(backButton);
  gameFrame.setFreeFont(FSSBO12);
  drawTitle("PICK A GAME", 26, TFT_WHITE, rgb(255,73,182));
  gameFrame.setFreeFont(NULL);
  drawGameCard(squishyMenuButton,4,"SQUISHY MAKER",rgb(255,73,182),now);
  drawGameCard(memoryMenuButton,5,"MEMORY MATCH",rgb(255,109,146),now);
  drawGameCard(saucerMenuButton,0,"SPACE SAUCER",COLOR_PURPLE,now);
  drawGameCard(yodaMenuButton,1,"BABY YODA",rgb(73,182,85),now);
  drawGameCard(driveMenuButton,2,"CITY DRIVE",rgb(255,146,0),now);
  drawGameCard(zombieMenuButton,3,"ZOMBIE CRASH",rgb(219,36,0),now);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}

void drawGameList() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  drawGameList(millis());
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

#include "include/SpaceSaucer.h"
#include "include/CityDrive.h"
#include "include/ZombieCrash.h"
#include "include/BabyYoda.h"
#include "include/BlackKnight.h"
#include "include/SquishyMaker.h"
#include "include/MemoryMatch.h"

void saveSoundSettings() {
  Preferences prefs;
  if (prefs.begin("game-sound",false)) {
    prefs.putBool("enabled",GameAudio::enabled);
    prefs.putUChar("volume-pct",GameAudio::percent);
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
  // Ten segments, one per 10% step.
  for (int i=0;i<10;++i) {
    const bool on=GameAudio::percent>=(i+1)*10;
    gameFrame.fillRect(116+i*9,179,8,10,on ? COLOR_MINT : 0x2945);
  }
  drawCentered("P0/P1: MOVE   P2: SELECT   P3: BACK",215,1,TFT_WHITE);
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}
void beginSettings() { GameAudio::stop(); drawSettings(); }
void updateSettings(bool tapped,uint16_t x,uint16_t y) {
  if (!tapped) return;
  if (settingsBackButton.contains(x,y)) { drawHomeMenu(); return; }
  if (soundToggleButton.contains(x,y)) GameAudio::setEnabled(!GameAudio::enabled);
  else if (volumeDownButton.contains(x,y)) GameAudio::setVolumePercent(GameAudio::percent-10);
  else if (volumeUpButton.contains(x,y)) GameAudio::setVolumePercent(GameAudio::percent+10);
  else return;
  saveSoundSettings();
  GameAudio::play(GameAudio::Click);
  drawSettings();
}

// Translate physical menu presses into the same actions as touch selection.
void navigateUi(uint8_t pressed, bool &touching, bool &tapped, uint16_t &x, uint16_t &y) {
  const TouchButton *items[16] = {};
  const TouchButton *back = nullptr;
  int count = 0, context = (int)gameState * 16;
  // On the home screen P2 selects on release, and is cancelled if other buttons
  // join in, so the four-button secret code doesn't also open a menu.
  static bool homeSelectArmed = false;
  if (gameState == HOME_MENU) {
    if (pressed & 4) { homeSelectArmed = true; pressed &= ~4; }
    if (homeSelectArmed && (ExpanderButtons::held & 0x0B)) homeSelectArmed = false;
    if (homeSelectArmed && !(ExpanderButtons::held & 4)) { homeSelectArmed = false; pressed |= 4; }
  } else homeSelectArmed = false;
  switch (gameState) {
    case HOME_MENU:
      items[count++]=&chooseGameButton; items[count++]=&settingsButton; break;
    case SETTINGS:
      items[count++]=&soundToggleButton; items[count++]=&volumeDownButton;
      items[count++]=&volumeUpButton; back=&settingsBackButton; break;
    case GAME_LIST:
      items[count++]=&squishyMenuButton; items[count++]=&memoryMenuButton; items[count++]=&saucerMenuButton; items[count++]=&yodaMenuButton;
      items[count++]=&driveMenuButton; items[count++]=&zombieMenuButton;
      back=&backButton; break;
    case SAUCER_RESULTS:
      items[count++]=&SpaceSaucer::againButton; items[count++]=&SpaceSaucer::menuButton;
      back=&SpaceSaucer::menuButton; break;
    case YODA_PLAYING:
      context+=(int)BabyYoda::mode;
      count=BabyYoda::navItems(items); back=&BabyYoda::exitButton; break;
    case SAUCER_PLAYING:
      if (pressed & 8) { drawGameList(); pressed=0; }
      break;
    case CITY_DRIVING:
      if (CityDrive::choosing) {
        context+=2;
        for (int i=0;i<CityDrive::vehicleCount;++i)
          if (CityDrive::unlocked(i)) items[count++]=&CityDrive::vehicleCards[i];
        back=&CityDrive::exitButton;
      } else if (CityDrive::finished) {
        context++; items[count++]=&CityDrive::retryButton; back=&CityDrive::exitButton;
      }
      break;
    case SQUISHY_MAKER:
      context+=(int)Squishy::stage*4+Squishy::step;
      count=Squishy::navItems(items); back=&Squishy::exitButton; break;
    case MEMORY_MATCH:
      context+=Memory::navContext();
      count=Memory::navItems(items); back=&Memory::exitButton; break;
    case BLACK_KNIGHT:
      context+=(int)BlackKnight::stage;
      if (BlackKnight::stage==BlackKnight::SELECT) {
        for (int i=0;i<3;++i) items[count++]=&BlackKnight::heroCards[i];
        back=&BlackKnight::exitButton;
      } else if (BlackKnight::stage!=BlackKnight::ACTIVE) {
        items[count++]=&BlackKnight::retryButton; items[count++]=&BlackKnight::menuButton;
        back=&BlackKnight::menuButton;
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
    else if (gameState==GAME_LIST) drawGameList(millis());
    else if (gameState==SAUCER_RESULTS) SpaceSaucer::showResults(false);
    else if (gameState==YODA_PLAYING) BabyYoda::render(millis());
    else if (gameState==CITY_DRIVING) CityDrive::render(false,0,0);
    else if (gameState==ZOMBIE_CRASH) ZombieCrash::render(false,0,0,0);
    else if (gameState==BLACK_KNIGHT) BlackKnight::render();
    else if (gameState==SQUISHY_MAKER) Squishy::render();
    else if (gameState==MEMORY_MATCH) Memory::render();
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
  SpaceSaucer::pause(pausedFor);
  BabyYoda::pause(pausedFor);
  CityDrive::lastFrame += pausedFor;
  ZombieCrash::lastFrame += pausedFor;
  BlackKnight::lastFrame += pausedFor;
  Memory::pause(pausedFor);
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
  chooseGameButton = {(int16_t)((GameDisplay::width - 190) / 2), 181, 190, 48};
  squishyMenuButton = {20, 50, 280, 28};
  memoryMenuButton = {20, 81, 280, 28};
  saucerMenuButton = {20, 112, 280, 28};
  yodaMenuButton = {20, 143, 280, 28};
  driveMenuButton = {20, 174, 280, 28};
  zombieMenuButton = {20, 205, 280, 28};
  backButton = {8, 6, 60, 25};
  Serial.println("Checking touch calibration");
  touchCalibrate();
  DamageLed::begin();
  GameAudio::begin();
  Preferences soundPrefs;
  if (soundPrefs.begin("game-sound",true)) {
    // Older firmware stored a 0-4 DAC level; convert it once to a percentage.
    const uint8_t legacy[]={0,10,30,50,100};
    const uint8_t oldLevel=min<uint8_t>(soundPrefs.getUChar("volume",4),4);
    GameAudio::setVolumePercent(soundPrefs.getUChar("volume-pct",legacy[oldLevel]));
    GameAudio::setEnabled(soundPrefs.getBool("enabled",true));
    soundPrefs.end();
  }
  BatteryStatus::begin();
  BabyYoda::load();
  Memory::loadBest();
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
  BatteryStatus::update(now);
  const bool actualTouching=touching;
  navigateUi(ExpanderButtons::pressed,touching,tapped,x,y);

  if (gameState == HOME_MENU) {
    // Twinkling stars, saucer and gear animate at ~12 fps.
    if (now - lastMenuFrame >= 80) drawHomeMenu(now);
    // Secret: hold all four buttons for 3 seconds to unlock Black Knight.
    static uint32_t secretHeldSince = 0;
    if ((ExpanderButtons::held & 0x0F) == 0x0F) {
      if (!secretHeldSince) secretHeldSince = now;
      else if (now - secretHeldSince >= 3000) { secretHeldSince = 0; beginBlackKnight(); }
    } else secretHeldSince = 0;
    if (tapped && settingsButton.contains(x,y)) beginSettings();
    else if (tapped && chooseGameButton.contains(x, y)) drawGameList();
  } else if (gameState == SETTINGS) {
    updateSettings(tapped,x,y);
  } else if (gameState == GAME_LIST) {
    if (now - lastMenuFrame >= 80) drawGameList(now);
    if (tapped && backButton.contains(x, y)) drawHomeMenu();
    else if (tapped && squishyMenuButton.contains(x, y)) beginSquishyMaker();
    else if (tapped && memoryMenuButton.contains(x, y)) beginMemoryMatch();
    else if (tapped && saucerMenuButton.contains(x, y)) beginSpaceSaucer();
    else if (tapped && yodaMenuButton.contains(x, y)) beginYodaGame();
    else if (tapped && driveMenuButton.contains(x, y)) beginCityDrive();
    else if (tapped && zombieMenuButton.contains(x, y)) beginZombieCrash();
  } else if (gameState == SAUCER_RESULTS) {
    SpaceSaucer::updateResults(touching, tapped, x, y);
  } else if (gameState == SAUCER_PLAYING) {
    updateSpaceSaucer(now, tapped, x, y);
  } else if (gameState == CITY_DRIVING) {
    updateCityDrive(now, touching, tapped, x, y, ExpanderButtons::held);
  } else if (gameState == SQUISHY_MAKER) {
    Squishy::update(now, tapped, x, y);
  } else if (gameState == MEMORY_MATCH) {
    Memory::update(now, tapped, x, y);
  } else if (gameState == BLACK_KNIGHT) {
    updateBlackKnight(now, tapped, x, y, ExpanderButtons::held, ExpanderButtons::pressed);
  } else if (gameState == ZOMBIE_CRASH) {
    updateZombieCrash(now, touching, tapped, x, y, ExpanderButtons::held, ExpanderButtons::pressed);
  } else {
    BabyYoda::update(now, touching, tapped, x, y, ExpanderButtons::held);
  }

  BabyYoda::tick(millis());
  GameAudio::engine(gameState==CITY_DRIVING && !CityDrive::finished && !CityDrive::choosing ? CityDrive::speed : 0);
  GameAudio::update(millis());
  DamageLed::update(millis());
  touchWasDown = actualTouching;
  // Yield without adding 10 ms to every completed frame.
  delay(1);
}
