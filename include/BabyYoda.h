#pragma once
#include "GameAudio.h"
#include "GameGfx.h"
#include <math.h>
#include <Preferences.h>

// Virtual pet in the style of a pocket egg toy. Needs keep draining whenever
// the device is on, even from other games, and are saved to NVS so the pet
// survives leaving the screen and power cycles.
namespace BabyYoda {
using GameGfx::rgb;
enum Mode { IDLE, FOOD_MENU, EATING, CATCH_GAME, CATCH_RESULT, SLEEPING };
enum Food { FROG, SOUP, COOKIE };
enum Face { NORMAL, BLINK, HAPPY, SAD, CHEW, ASLEEP, OUCH };

const TouchButton exitButton = {8, 6, 60, 25};
const TouchButton feedButton = {10, 197, 92, 34};
const TouchButton playButton = {114, 197, 92, 34};
const TouchButton restButton = {218, 197, 92, 34};
const TouchButton foodCards[3] = {{22,74,86,112},{117,74,86,112},{212,74,86,112}};
const TouchButton continueButton = {70, 80, 180, 96};
const TouchButton poopSpots[3] = {{48,162,30,26},{244,166,30,26},{84,174,30,22}};
const char *const foodNames[3] = {"FROG", "BROTH", "COOKIE"};
// Hunger and fun restored by each food.
const uint8_t foodHunger[3] = {30, 45, 15};
const uint8_t foodFun[3] = {6, 0, 15};

const uint16_t green = rgb(146,182,85), greenLight = rgb(182,219,85);
const uint16_t greenDark = rgb(73,109,0), earInner = rgb(219,146,170);
const uint16_t robe = rgb(182,146,85), robeShade = rgb(146,109,85);
const uint16_t collar = rgb(219,182,170), eyeColor = rgb(36,36,0);
const uint16_t poopColor = rgb(109,73,0);

// Saved fields first; layout changes need a new version number.
struct Saved {
  uint8_t version, hunger, fun, energy, poops;
  uint16_t best;
  uint32_t ageMinutes, sleepLeftMs;
} pet = {1, 80, 80, 80, 0, 0, 0, 0};

Mode mode = IDLE;
Food eating = FROG;
uint32_t lastTick = 0, modeStarted = 0, lastFrame = 0, lastSave = 0;
uint32_t decayMs = 0, restMs = 0, ageMs = 0, poopInMs = 0;
uint32_t messageUntil = 0, nextBlink = 0;
const char *message = nullptr;
bool dirty = false, loaded = false, sulking = false;

// Catch mini-game: snacks fall from the sky, rocks must be dodged.
struct Drop { float x, y, vy; uint8_t kind; bool active; } drops[6];
float catcherX = 160;
int catchScore = 0, flinchFrames = 0;
uint32_t nextDrop = 0, catchEnds = 0;
constexpr uint32_t catchLength = 20000;

void load() {
  Preferences prefs;
  if (prefs.begin("baby-yoda", true)) {
    Saved saved;
    if (prefs.getBytes("pet", &saved, sizeof(saved)) == sizeof(saved) && saved.version == pet.version)
      pet = saved;
    prefs.end();
  }
  if (pet.sleepLeftMs) mode = SLEEPING;
  lastTick = lastSave = millis();
  loaded = true;
}
void save() {
  Preferences prefs;
  if (prefs.begin("baby-yoda", false)) {
    prefs.putBytes("pet", &pet, sizeof(pet));
    prefs.end();
  }
  dirty = false;
  lastSave = millis();
}
void say(const char *text, uint32_t now, uint32_t ms = 2200, bool refusal = false) {
  message = text; messageUntil = now + ms; sulking = refusal;
}
uint8_t raise(uint8_t value, int amount) { return (uint8_t)constrain(value + amount, 0, 100); }

void startSleep(uint32_t now) {
  mode = SLEEPING;
  // Sleep long enough to refill energy, but never a pointlessly short nap.
  pet.sleepLeftMs = max(20000UL, 600UL * (100 - pet.energy));
  restMs = 0;
  dirty = true;
  GameAudio::play(GameAudio::Sleep);
  save();
}
void wake(uint32_t now) {
  pet.sleepLeftMs = 0;
  mode = IDLE;
  modeStarted = now;
  say(pet.energy >= 90 ? "Good nap!" : "Still tired...", now);
  GameAudio::play(GameAudio::Happy);
  save();
}

// Runs every loop so the pet keeps living while other screens are open.
void tick(uint32_t now) {
  if (!loaded) load();
  uint32_t dt = now - lastTick;
  lastTick = now;
  if (dt > 1000) dt = 1000;
  if (mode == CATCH_GAME || mode == CATCH_RESULT) dt /= 2; // Playing slows the drain.
  ageMs += dt;
  if (ageMs >= 60000) { ageMs -= 60000; ++pet.ageMinutes; dirty = true; }
  if (mode == SLEEPING) {
    restMs += dt;
    while (restMs >= 600) { restMs -= 600; pet.energy = raise(pet.energy, 1); dirty = true; }
    pet.sleepLeftMs = pet.sleepLeftMs > dt ? pet.sleepLeftMs - dt : 0;
    if (!pet.sleepLeftMs && gameState == YODA_PLAYING) wake(now);
    else if (!pet.sleepLeftMs) { mode = IDLE; save(); }
  }
  decayMs += dt;
  // Sleeping halves hunger and fun loss; a messy room doubles fun loss.
  const uint32_t period = mode == SLEEPING ? 20000 : 10000;
  while (decayMs >= period) {
    decayMs -= period;
    pet.hunger = raise(pet.hunger, -1);
    pet.fun = raise(pet.fun, pet.poops ? -2 : -1);
    if (mode != SLEEPING) pet.energy = raise(pet.energy, -1);
    dirty = true;
  }
  if (poopInMs) {
    if (poopInMs <= dt) { poopInMs = 0; if (pet.poops < 3) ++pet.poops; dirty = true; }
    else poopInMs -= dt;
  }
  if (pet.energy == 0 && mode == IDLE) { startSleep(now); say("Zzz...", now); }
  if (dirty && now - lastSave >= 60000) save();
}

void pause(uint32_t pausedFor) {
  lastTick += pausedFor; modeStarted += pausedFor; lastFrame += pausedFor;
  messageUntil += pausedFor; nextBlink += pausedFor; nextDrop += pausedFor; catchEnds += pausedFor;
}

// ---- Drawing -------------------------------------------------------------
int S(float v, float s) { return (int)lroundf(v * s); }
// Half ellipse stroked 2px thick: lower half for smiles, upper for frowns.
void arc(int cx, int cy, int rx, int ry, bool lower, uint16_t color) {
  if (rx < 1) rx = 1;
  for (int dx = -rx; dx <= rx; ++dx) {
    int dy = (int)(ry * sqrtf(1.0f - (float)(dx * dx) / (rx * rx)));
    gameFrame.fillRect(cx + dx, lower ? cy + dy : cy - dy, 1, 2, color);
  }
}

void ear(int x, int y, float s, int side, int lift) {
  const int tipX = x + side * S(88, s), tipY = y - S(8, s) + S(lift, s);
  gameFrame.fillTriangle(x + side * S(28, s), y - S(16, s), tipX, tipY, x + side * S(32, s), y + S(10, s), green);
  gameFrame.fillTriangle(x + side * S(34, s), y - S(9, s), x + side * S(76, s), tipY + S(1, s),
                         x + side * S(34, s), y + S(4, s), earInner);
  gameFrame.drawLine(x + side * S(28, s), y - S(16, s), tipX, tipY, greenDark);
  gameFrame.drawLine(x + side * S(32, s), y + S(10, s), tipX, tipY, greenDark);
}

void hand(int x, int y, float s) {
  gameFrame.fillCircle(x, y, S(6, s), green);
  for (int f = -1; f <= 1; ++f) gameFrame.fillCircle(x + f * S(4, s), y + S(5, s), max(1, S(2, s)), green);
}

void drawPet(int x, int y, float s, Face face, int lift = 0) {
  GameGfx::darkenEllipse(x, y + S(66, s), S(42, s), S(7, s));
  // Robe with a soft collar and sleeves that meet in front.
  gameFrame.fillRoundRect(x - S(36, s), y + S(18, s), S(72, s), S(48, s), S(16, s), robeShade);
  gameFrame.fillRoundRect(x - S(33, s), y + S(18, s), S(60, s), S(46, s), S(15, s), robe);
  for (int i = 0; i < 3; ++i)
    gameFrame.drawFastVLine(x - S(16 - i * 14, s), y + S(36, s), S(26, s), robeShade);
  gameFrame.fillEllipse(x - S(20, s), y + S(46, s), S(12, s), S(9, s), robeShade);
  gameFrame.fillEllipse(x + S(20, s), y + S(46, s), S(12, s), S(9, s), robeShade);
  hand(x - S(10, s), y + S(48, s), s);
  hand(x + S(10, s), y + S(48, s), s);
  gameFrame.fillEllipse(x, y + S(27, s), S(34, s), S(10, s), collar);
  gameFrame.fillEllipse(x, y + S(29, s), S(24, s), S(6, s), robeShade);

  ear(x, y, s, -1, lift);
  ear(x, y, s, 1, lift);
  gameFrame.fillEllipse(x, y, S(40, s), S(33, s), green);
  GameGfx::glow(x - S(8, s), y - S(14, s), S(22, s), greenLight);
  gameFrame.drawEllipse(x, y, S(40, s), S(33, s), greenDark);
  // Wispy hair and forehead creases.
  for (int i = -2; i <= 2; ++i)
    gameFrame.drawLine(x + i * S(6, s), y - S(32, s), x + i * S(8, s), y - S(38, s), rgb(219,219,170));
  arc(x, y - S(18, s), S(9, s), S(2, s), false, greenDark);
  arc(x, y - S(13, s), S(6, s), S(2, s), false, greenDark);

  const int eyeY = y + S(1, s);
  for (int side = -1; side <= 1; side += 2) {
    const int ex = x + side * S(16, s);
    if (face == BLINK || face == ASLEEP) {
      arc(ex, eyeY, S(8, s), S(3, s), true, eyeColor);
    } else if (face == HAPPY || face == CHEW) {
      arc(ex, eyeY + S(3, s), S(8, s), S(6, s), false, eyeColor);
    } else if (face == OUCH) {
      gameFrame.drawLine(ex - S(6, s), eyeY - S(4, s), ex + S(6, s), eyeY + S(2, s), eyeColor);
      gameFrame.drawLine(ex - S(6, s), eyeY + S(4, s), ex + S(6, s), eyeY - S(2, s), eyeColor);
    } else {
      gameFrame.fillEllipse(ex, eyeY, S(11, s), S(12, s), greenDark);
      gameFrame.fillEllipse(ex, eyeY + S(1, s), S(10, s), S(11, s), eyeColor);
      gameFrame.fillCircle(ex - S(3, s), eyeY - S(4, s), max(1, S(3, s)), TFT_WHITE);
      gameFrame.fillCircle(ex + S(4, s), eyeY + S(4, s), max(1, S(1, s)), TFT_WHITE);
      if (face == SAD) {
        gameFrame.drawLine(ex - side * S(10, s), eyeY - S(16, s), ex + side * S(6, s), eyeY - S(12, s), greenDark);
      }
    }
  }
  if (face == HAPPY || face == CHEW) {
    GameGfx::glow(x - S(28, s), y + S(13, s), S(7, s), rgb(255,146,170));
    GameGfx::glow(x + S(28, s), y + S(13, s), S(7, s), rgb(255,146,170));
  }
  gameFrame.fillCircle(x - S(3, s), y + S(13, s), max(1, S(1, s)), greenDark);
  gameFrame.fillCircle(x + S(3, s), y + S(13, s), max(1, S(1, s)), greenDark);
  const int mouthY = y + S(20, s);
  if (face == CHEW) gameFrame.fillEllipse(x, mouthY, S(5, s), S(4, s), rgb(109,36,36));
  else if (face == SAD || face == OUCH) arc(x, mouthY + S(3, s), S(6, s), S(3, s), false, eyeColor);
  else if (face == HAPPY) {
    gameFrame.fillEllipse(x, mouthY, S(7, s), S(4, s), rgb(109,36,36));
    gameFrame.fillRect(x - S(8, s), mouthY - S(5, s), S(16, s), S(5, s), green);
  } else arc(x, mouthY - S(2, s), S(6, s), S(3, s), true, eyeColor);
}

void drawFood(Food food, int x, int y, float s) {
  if (food == FROG) {
    gameFrame.fillEllipse(x, y, S(14, s), S(10, s), rgb(73,182,0));
    gameFrame.fillCircle(x - S(7, s), y - S(9, s), S(5, s), rgb(73,182,0));
    gameFrame.fillCircle(x + S(7, s), y - S(9, s), S(5, s), rgb(73,182,0));
    gameFrame.fillCircle(x - S(7, s), y - S(9, s), max(1, S(2, s)), TFT_BLACK);
    gameFrame.fillCircle(x + S(7, s), y - S(9, s), max(1, S(2, s)), TFT_BLACK);
    gameFrame.fillEllipse(x, y + S(3, s), S(9, s), S(5, s), rgb(182,219,85));
  } else if (food == SOUP) {
    gameFrame.fillEllipse(x, y - S(4, s), S(18, s), S(5, s), rgb(219,146,0));
    gameFrame.fillTriangle(x - S(18, s), y - S(4, s), x + S(18, s), y - S(4, s), x, y + S(12, s), rgb(73,109,170));
    gameFrame.fillEllipse(x, y + S(2, s), S(14, s), S(9, s), rgb(73,109,170));
    gameFrame.drawEllipse(x, y - S(4, s), S(18, s), S(5, s), rgb(36,73,170));
    for (int i = -1; i <= 1; ++i) arc(x + i * S(7, s), y - S(14, s), S(2, s), S(4, s), i != 0, rgb(219,219,255));
  } else {
    gameFrame.fillEllipse(x, y - S(5, s), S(14, s), S(6, s), rgb(73,182,255));
    gameFrame.fillEllipse(x, y + S(5, s), S(14, s), S(6, s), rgb(73,146,255));
    gameFrame.fillRect(x - S(12, s), y - S(2, s), S(24, s), S(4, s), rgb(255,255,170));
    gameFrame.fillCircle(x - S(5, s), y - S(7, s), max(1, S(2, s)), rgb(182,219,255));
  }
}

void drawPoop(int x, int y) {
  gameFrame.fillEllipse(x, y + 6, 11, 5, poopColor);
  gameFrame.fillEllipse(x, y, 8, 4, poopColor);
  gameFrame.fillEllipse(x, y - 5, 5, 3, poopColor);
  gameFrame.drawPixel(x - 3, y - 1, rgb(182,146,85));
  gameFrame.drawFastHLine(x - 6, y + 4, 4, rgb(182,146,85));
}

void drawRoom(bool dark) {
  GameGfx::gradient(0, 52, 320, 112, dark ? rgb(0,0,85) : rgb(109,73,85), dark ? rgb(36,36,85) : rgb(182,146,85));
  // Round window: stars by night, a twin-sun sky by day.
  gameFrame.fillCircle(46, 92, 25, rgb(73,36,36));
  if (dark) {
    gameFrame.fillCircle(46, 92, 21, rgb(0,0,85));
    gameFrame.fillCircle(52, 84, 7, rgb(255,255,170));
    gameFrame.fillCircle(55, 82, 6, rgb(0,0,85));
    gameFrame.drawPixel(34, 96, TFT_WHITE); gameFrame.drawPixel(42, 106, TFT_WHITE);
  } else {
    GameGfx::gradient(25, 71, 42, 42, rgb(255,182,85), rgb(255,219,170));
    gameFrame.fillCircle(40, 88, 5, rgb(255,255,170));
    gameFrame.fillCircle(53, 94, 4, rgb(255,219,85));
    gameFrame.fillRect(25, 104, 42, 9, rgb(219,146,85));
    gameFrame.drawCircle(46, 92, 21, rgb(73,36,36));
  }
  gameFrame.drawFastVLine(46, 70, 44, rgb(73,36,36));
  gameFrame.drawFastHLine(24, 92, 44, rgb(73,36,36));
  // Floating cradle in the corner.
  gameFrame.fillEllipse(276, 140, 28, 13, rgb(182,182,170));
  gameFrame.fillEllipse(276, 134, 24, 7, rgb(109,109,85));
  gameFrame.drawEllipse(276, 140, 28, 13, rgb(219,219,255));
  GameGfx::darkenEllipse(276, 168, 20, 3);
  // Wooden floor.
  gameFrame.fillRect(0, 164, 320, 28, dark ? rgb(36,36,0) : rgb(146,109,36));
  for (int y = 170; y < 192; y += 7) gameFrame.drawFastHLine(0, y, 320, dark ? rgb(0,0,0) : rgb(109,73,0));
  for (int i = 0; i < pet.poops; ++i)
    drawPoop(poopSpots[i].x + poopSpots[i].w / 2, poopSpots[i].y + poopSpots[i].h / 2);
  if (dark) GameGfx::darkenRect(0, 52, 320, 140);
}

void drawStat(int x, uint8_t value, uint16_t color, int icon, uint32_t now) {
  const int y = 36;
  if (icon == 0) drawFood(COOKIE, x + 7, y + 6, 0.45f);
  else if (icon == 1) { gameFrame.fillCircle(x + 7, y + 6, 6, COLOR_PINK); gameFrame.drawFastHLine(x + 1, y + 6, 13, TFT_WHITE); }
  else { gameFrame.fillCircle(x + 7, y + 6, 6, rgb(255,255,170)); gameFrame.fillCircle(x + 10, y + 3, 5, COLOR_SKY); }
  const bool low = value < 25;
  const uint16_t fill = low && (now / 400) % 2 ? rgb(255,36,0) : color;
  gameFrame.drawRoundRect(x + 17, y, 78, 12, 5, low ? rgb(255,36,0) : TFT_WHITE);
  if (value) gameFrame.fillRoundRect(x + 19, y + 2, max(3, 74 * value / 100), 8, 3, fill);
}

void drawButton(const TouchButton &b, const char *label, uint16_t fill, bool disabled = false) {
  if (disabled) fill = rgb(73,73,85);
  gameFrame.fillRoundRect(b.x, b.y + 2, b.w, b.h, 8, GameGfx::darker(fill));
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h - 2, 8, fill);
  gameFrame.drawRoundRect(b.x, b.y, b.w, b.h - 2, 8, disabled ? rgb(146,146,170) : TFT_WHITE);
  gameFrame.setTextColor(disabled ? rgb(146,146,170) : TFT_WHITE);
  gameFrame.drawCentreString(label, b.x + b.w / 2, b.y + 8, 2);
}

void drawBubble(int x, int y, const char *text) {
  const int w = gameFrame.textWidth(text, 2) + 14;
  gameFrame.fillRoundRect(x, y, w, 22, 8, TFT_WHITE);
  gameFrame.fillTriangle(x + 8, y + 20, x + 18, y + 20, x + 4, y + 30, TFT_WHITE);
  gameFrame.setTextColor(TFT_BLACK);
  gameFrame.drawString(text, x + 7, y + 3, 2);
}

void drawHeader(const char *title, const char *right) {
  GameGfx::gradient(0, 0, 320, 32, rgb(36,36,85), rgb(0,0,0));
  gameFrame.fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 8, COLOR_PURPLE);
  gameFrame.drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 8, TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString(mode == FOOD_MENU ? "CLOSE" : "BACK", exitButton.x + exitButton.w / 2, exitButton.y + 5, 2);
  gameFrame.setTextColor(COLOR_GOLD);
  gameFrame.drawCentreString(title, 170, 8, 2);
  gameFrame.setTextColor(COLOR_MINT);
  gameFrame.drawRightString(right, 312, 8, 2);
}

const char *needMessage() {
  if (pet.hunger < 25) return "Hungry!";
  if (pet.energy < 25) return "Sleepy...";
  if (pet.fun < 25) return "Bored!";
  if (pet.poops) return "Messy!";
  return nullptr;
}

void renderCatch(uint32_t now) {
  GameGfx::gradient(0, 32, 320, 140, rgb(73,146,255), rgb(255,219,170));
  gameFrame.fillCircle(270, 62, 12, rgb(255,255,170));
  gameFrame.fillCircle(248, 70, 8, rgb(255,219,85));
  GameGfx::gradient(0, 172, 320, 68, rgb(219,182,85), rgb(182,146,36));
  for (const Drop &d : drops) {
    if (!d.active) continue;
    if (d.kind == 2) {
      gameFrame.fillCircle((int)d.x, (int)d.y, 9, rgb(109,109,85));
      gameFrame.fillCircle((int)d.x - 3, (int)d.y - 3, 3, rgb(182,182,170));
    } else drawFood(d.kind ? FROG : COOKIE, (int)d.x, (int)d.y, 0.8f);
  }
  const Face face = flinchFrames > 0 ? OUCH : (catchScore && (now / 300) % 4 == 0 ? HAPPY : NORMAL);
  drawPet((int)catcherX, 168, 0.55f, face, -4);
  char hud[24];
  const uint32_t left = catchEnds > now ? (catchEnds - now + 999) / 1000 : 0;
  snprintf(hud, sizeof(hud), "%lus", (unsigned long)left);
  drawHeader("CATCH SNACKS", hud);
  snprintf(hud, sizeof(hud), "SNACKS %d", catchScore);
  gameFrame.setTextColor(TFT_BLACK);
  gameFrame.drawString(hud, 10, 38, 2);
  gameFrame.setTextColor(rgb(73,36,0));
  gameFrame.drawCentreString("DRAG OR HOLD P0/P1 TO MOVE - AVOID ROCKS", 160, 228, 1);
}

void render(uint32_t now) {
  lastFrame = now;
  gameFrame.fillSprite(COLOR_SKY);
  if (mode == CATCH_GAME) { renderCatch(now); UiNavigation::draw(); gameFrame.pushSprite(0, 0); return; }

  char right[24];
  if (mode == SLEEPING) snprintf(right, sizeof(right), "%lus", (unsigned long)(pet.sleepLeftMs + 999) / 1000);
  else snprintf(right, sizeof(right), "DAY %lu", (unsigned long)(pet.ageMinutes / 5 + 1));
  drawRoom(mode == SLEEPING);
  drawHeader("BABY YODA", right);
  drawStat(6, pet.hunger, COLOR_GOLD, 0, now);
  drawStat(110, pet.fun, COLOR_PINK, 1, now);
  drawStat(214, pet.energy, COLOR_MINT, 2, now);

  const uint32_t t = now - modeStarted;
  int bob = mode == SLEEPING ? (int)(2 * sinf(now / 900.0f)) : (int)(2 * sinf(now / 260.0f));
  const int px = 160, py = 104 + bob;
  const uint8_t lowest = min(pet.hunger, min(pet.fun, pet.energy));
  Face face = lowest < 25 ? SAD : (lowest > 70 ? HAPPY : NORMAL);
  if (face != SAD && (int32_t)(now - nextBlink) >= 0) {
    face = BLINK;
    if (now - nextBlink > 150) nextBlink = now + random(1800, 4200);
  }
  int lift = face == SAD ? 14 : (face == HAPPY ? -6 : 0);
  if (mode == SLEEPING) { face = ASLEEP; lift = 10; }
  if (mode == EATING) face = (t / 220) % 2 ? CHEW : HAPPY;
  if (messageUntil > now && sulking) face = SAD;
  drawPet(px, py, 1.0f, face, lift);

  if (mode == EATING) {
    // Three bites: the food shrinks each time the mouth closes.
    const float left = 1.0f - min(t, (uint32_t)1500) / 1500.0f;
    if (left > 0.05f) drawFood(eating, px + 4, py + 34, 0.5f + 0.6f * left);
  }
  if (mode == SLEEPING) {
    for (int i = 0; i < 3; ++i) {
      const int phase = (now / 40 + i * 25) % 75;
      gameFrame.setTextColor(rgb(219,219,255));
      gameFrame.drawString("z", px + 40 + phase / 2 + i * 4, py - 20 - phase, i == 2 ? 4 : 2);
    }
  } else if (mode != FOOD_MENU) {
    const char *text = messageUntil > now ? message : needMessage();
    if (text) drawBubble(px + 42, py - 52, text);
  }

  if (mode == FOOD_MENU) {
    GameGfx::darkenRect(0, 52, 320, 145);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString("WHAT'S FOR LUNCH?", 160, 55, 2);
    for (int i = 0; i < 3; ++i) {
      const TouchButton &c = foodCards[i];
      gameFrame.fillRoundRect(c.x, c.y, c.w, c.h, 10, rgb(36,36,85));
      gameFrame.drawRoundRect(c.x, c.y, c.w, c.h, 10, COLOR_GOLD);
      drawFood((Food)i, c.x + c.w / 2, c.y + 40, 1.6f);
      gameFrame.setTextColor(TFT_WHITE);
      gameFrame.drawCentreString(foodNames[i], c.x + c.w / 2, c.y + 70, 2);
      char info[16];
      snprintf(info, sizeof(info), "+%d FOOD", foodHunger[i]);
      gameFrame.setTextColor(COLOR_GOLD);
      gameFrame.drawCentreString(info, c.x + c.w / 2, c.y + 92, 1);
    }
  }
  if (mode == CATCH_RESULT) {
    GameGfx::darkenRect(0, 52, 320, 145);
    const TouchButton &c = continueButton;
    gameFrame.fillRoundRect(c.x, c.y, c.w, c.h, 10, rgb(36,36,85));
    gameFrame.drawRoundRect(c.x, c.y, c.w, c.h, 10, COLOR_PINK);
    char line[32];
    snprintf(line, sizeof(line), "%d SNACKS!", catchScore);
    gameFrame.setTextColor(COLOR_GOLD);
    gameFrame.drawCentreString(line, 160, c.y + 12, 4);
    snprintf(line, sizeof(line), "BEST %u", pet.best);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(line, 160, c.y + 44, 2);
    gameFrame.setTextColor(COLOR_MINT);
    gameFrame.drawCentreString("TAP TO CONTINUE", 160, c.y + 68, 2);
  }

  const bool busy = mode == EATING || mode == FOOD_MENU || mode == CATCH_RESULT;
  drawButton(feedButton, "FEED", COLOR_PURPLE, busy || mode == SLEEPING);
  drawButton(playButton, "PLAY", COLOR_PINK, busy || mode == SLEEPING);
  drawButton(restButton, mode == SLEEPING ? "WAKE" : "REST", 0x2585, busy);
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

// ---- Input ---------------------------------------------------------------
int navItems(const TouchButton **items) {
  int count = 0;
  if (mode == IDLE) {
    items[count++] = &feedButton; items[count++] = &playButton; items[count++] = &restButton;
    for (int i = 0; i < pet.poops; ++i) items[count++] = &poopSpots[i];
  } else if (mode == FOOD_MENU) {
    for (int i = 0; i < 3; ++i) items[count++] = &foodCards[i];
  } else if (mode == SLEEPING) items[count++] = &restButton;
  else if (mode == CATCH_RESULT) items[count++] = &continueButton;
  return count;
}

void startCatch(uint32_t now) {
  mode = CATCH_GAME;
  catcherX = 160; catchScore = 0; flinchFrames = 0;
  for (Drop &d : drops) d.active = false;
  nextDrop = now + 400;
  catchEnds = now + catchLength;
  GameAudio::play(GameAudio::Start);
}

void finishCatch(uint32_t now) {
  mode = CATCH_RESULT;
  modeStarted = now;
  pet.fun = raise(pet.fun, 8 + catchScore * 3);
  pet.energy = raise(pet.energy, -12);
  pet.hunger = raise(pet.hunger, -6);
  if (catchScore > pet.best) pet.best = catchScore;
  GameAudio::play(catchScore >= 8 ? GameAudio::Win : GameAudio::Happy);
  save();
}

void updateCatch(uint32_t now, bool touching, uint16_t tx, uint16_t ty, uint8_t held) {
  if (now - lastFrame < 33) return;
  const float dt = min(0.1f, (now - lastFrame) / 1000.0f);
  float move = (int)((held & 2) != 0) - (int)((held & 1) != 0);
  if (touching && ty > 40) move = constrain((tx - catcherX) / 20.0f, -1.0f, 1.0f);
  catcherX = constrain(catcherX + move * 230 * dt, 30.0f, 290.0f);
  if (flinchFrames) --flinchFrames;
  if ((int32_t)(now - nextDrop) >= 0) {
    for (Drop &d : drops) {
      if (d.active) continue;
      const float progress = 1.0f - (float)(catchEnds - now) / catchLength;
      const int roll = random(0, 10);
      d = {(float)random(20, 300), 40, (float)random(70, 110) + 70 * progress,
           (uint8_t)(roll < 5 ? 0 : roll < 7 ? 1 : 2), true};
      break;
    }
    nextDrop = now + random(450, 850);
  }
  for (Drop &d : drops) {
    if (!d.active) continue;
    d.y += d.vy * dt;
    if (d.y > 150 && d.y < 182 && fabsf(d.x - catcherX) < 28) {
      d.active = false;
      if (d.kind == 2) { catchScore = max(0, catchScore - 2); flinchFrames = 12; GameAudio::play(GameAudio::Hurt); }
      else { catchScore += d.kind ? 2 : 1; GameAudio::play(GameAudio::Pop); }
    } else if (d.y > 200) d.active = false;
  }
  if ((int32_t)(now - catchEnds) >= 0) { finishCatch(now); render(now); return; }
  render(now);
}

void update(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y, uint8_t held) {
  if (tapped && exitButton.contains(x, y)) {
    if (mode == FOOD_MENU || mode == CATCH_RESULT) { mode = IDLE; render(now); }
    else if (mode == CATCH_GAME) { finishCatch(now); render(now); }
    else { save(); drawGameList(); }
    return;
  }
  if (mode == CATCH_GAME) { updateCatch(now, touching, x, y, held); return; }
  if (mode == EATING && now - modeStarted >= 1500) {
    mode = IDLE;
    pet.hunger = raise(pet.hunger, foodHunger[eating]);
    pet.fun = raise(pet.fun, foodFun[eating]);
    if (!poopInMs && random(0, 2)) poopInMs = 15000;
    say("Yum!", now, 1500);
    save();
  }
  if (tapped) {
    if (mode == IDLE) {
      if (feedButton.contains(x, y)) {
        if (pet.hunger >= 95) { say("I'm full!", now, 2200, true); GameAudio::play(GameAudio::Click); }
        else { mode = FOOD_MENU; GameAudio::play(GameAudio::Click); }
      } else if (playButton.contains(x, y)) {
        if (pet.energy < 15) { say("Too sleepy...", now, 2200, true); GameAudio::play(GameAudio::Click); }
        else { startCatch(now); render(now); return; }
      } else if (restButton.contains(x, y)) {
        if (pet.energy >= 95) { say("I'm not tired!", now, 2200, true); GameAudio::play(GameAudio::Click); }
        else startSleep(now);
      } else {
        for (int i = pet.poops - 1; i >= 0; --i) {
          if (!poopSpots[i].contains(x, y)) continue;
          --pet.poops;
          pet.fun = raise(pet.fun, 3);
          say("All clean!", now, 1500);
          GameAudio::play(GameAudio::Pop);
          save();
          break;
        }
      }
    } else if (mode == FOOD_MENU) {
      for (int i = 0; i < 3; ++i) {
        if (!foodCards[i].contains(x, y)) continue;
        eating = (Food)i;
        mode = EATING;
        modeStarted = now;
        GameAudio::play(GameAudio::Feed);
      }
    } else if (mode == SLEEPING && restButton.contains(x, y)) wake(now);
    else if (mode == CATCH_RESULT && continueButton.contains(x, y)) { mode = IDLE; say("That was fun!", now); }
    render(now);
    return;
  }
  // Idle animation (breathing, blinking, sleep Zs) at ~15 fps.
  if (now - lastFrame >= 66) render(now);
}
} // namespace BabyYoda

void beginYodaGame() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  using namespace BabyYoda;
  gameState = YODA_PLAYING;
  if (mode != SLEEPING) mode = IDLE;
  modeStarted = millis();
  nextBlink = modeStarted + random(1800, 4200);
  render(modeStarted);
}
