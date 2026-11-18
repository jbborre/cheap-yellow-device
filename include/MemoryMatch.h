#pragma once
#include <Preferences.h>
#include "GameAudio.h"
#include "GameGfx.h"
#include <math.h>

// Memory match: flip two cards at a time to find pairs of cute animals.
// Play solo (fewest moves), with a friend taking turns, or against the computer.
namespace Memory {
using GameGfx::rgb;
enum Stage { MODE, DIFFICULTY, PLAY, DONE };
enum Mode { SOLO, TWO_PLAYER, VS_COMPUTER };
enum Animal { BUNNY, KITTY, PANDA, PUPPY, FROG, PIGGY, CHICK, PENGUIN, ANIMAL_COUNT };

constexpr int maxCards = 12;
constexpr uint32_t flipMs = 220;
const int cardCounts[3] = {8, 10, 12};
const int gridCols[3] = {4, 5, 4};
// Chance that the computer remembers a card it has seen.
const int computerRecall[3] = {60, 70, 80};
const char *const modeNames[3] = {"SOLO", "2 PLAYERS", "VS COMPUTER"};
const char *const difficultyNames[3] = {"EASY", "MEDIUM", "HARD"};
const uint16_t playerColors[2] = {rgb(255,73,182), rgb(73,146,255)};
const uint16_t headerColors[2] = {rgb(255,146,219), rgb(146,182,255)};

const TouchButton exitButton = {8, 6, 60, 25};
const TouchButton choiceButtons[3] = {{40,74,240,44},{40,126,240,44},{40,178,240,44}};
const TouchButton againButton = {52, 172, 104, 32};
const TouchButton newGameButton = {164, 172, 104, 32};

Stage stage = MODE;
Mode mode = SOLO;
int difficulty = 0;
int cardCount = 8;
uint8_t animal[maxCards];
bool faceUp[maxCards], matched[maxCards];
uint32_t flipAt[maxCards];
int8_t memory[maxCards];  // What the computer remembers: animal, or -1.
TouchButton cardButtons[maxCards];
int first = -1, second = -1;
int turn = 0, score[2] = {0, 0}, moves = 0, pairsLeft = 0, best[3] = {0, 0, 0};
bool newBest = false;
uint32_t resolveAt = 0, computerAt = 0, finishAt = 0, cheerUntil = 0, lastFrame = 0;
struct Heart { float x, y, vy, life; } hearts[10];

void loadBest() {
  Preferences prefs;
  if (prefs.begin("memory-match", true)) {
    for (int i = 0; i < 3; ++i) { char key[8]; snprintf(key, sizeof(key), "best%d", i); best[i] = prefs.getUChar(key, 0); }
    prefs.end();
  }
}
void saveBest() {
  Preferences prefs;
  if (prefs.begin("memory-match", false)) {
    char key[8]; snprintf(key, sizeof(key), "best%d", difficulty);
    prefs.putUChar(key, best[difficulty]);
    prefs.end();
  }
}

bool computerTurn() { return mode == VS_COMPUTER && turn == 1; }
const char *playerName(int p) {
  if (mode == VS_COMPUTER) return p ? "COMPUTER" : "YOU";
  return p ? "PLAYER 2" : "PLAYER 1";
}

// ---- Animals --------------------------------------------------------------
// Outlined shapes read clearly on the white card face.
const uint16_t ink = rgb(36,0,36);
void blob(int x, int y, int rx, int ry, uint16_t fill, uint16_t line = ink) {
  gameFrame.fillEllipse(x, y, rx + 1, ry + 1, line);
  gameFrame.fillEllipse(x, y, rx, ry, fill);
}
void smile(int cx, int cy, int rx, int ry, uint16_t color = ink) {
  if (rx < 1) rx = 1;
  for (int dx = -rx; dx <= rx; ++dx)
    gameFrame.drawPixel(cx + dx, cy + (int)(ry * sqrtf(1.0f - (float)(dx * dx) / (rx * rx))), color);
}
void eyes(int cx, int cy, int gap, int r) {
  for (int s = -1; s <= 1; s += 2) {
    gameFrame.fillCircle(cx + s * gap, cy, r, ink);
    gameFrame.fillCircle(cx + s * gap - r / 3, cy - r / 3, max(1, r / 3), TFT_WHITE);
  }
}
void cheeks(int cx, int cy, int gap, int r) {
  for (int s = -1; s <= 1; s += 2) gameFrame.fillEllipse(cx + s * gap, cy, r, max(1, r / 2 + 1), rgb(255,146,182));
}
// Triangle with a one-pixel outline, for ears.
void ear(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t fill) {
  gameFrame.fillTriangle(x0, y0, x1, y1, x2, y2, ink);
  const int mx = (x0 + x1 + x2) / 3, my = (y0 + y1 + y2) / 3;
  auto in = [](int v, int m) { return v + (m > v ? 1 : m < v ? -1 : 0) * 2; };
  gameFrame.fillTriangle(in(x0, mx), in(y0, my), in(x1, mx), in(y1, my), in(x2, mx), in(y2, my), fill);
}

// Animal head centered on (cx, cy) with radius r.
void drawAnimal(int a, int cx, int cy, int r) {
  const int er = max(2, r / 7), eg = r * 2 / 5, ey = cy - r / 8;
  switch (a) {
    case BUNNY: {
      const uint16_t fur = rgb(255,219,255), pink = rgb(255,146,182);
      cy += r / 5; const int y = cy - r / 8;
      for (int s = -1; s <= 1; s += 2) {
        blob(cx + s * r * 2 / 5, cy - r * 5 / 4, r / 4, r * 3 / 5, fur);
        gameFrame.fillEllipse(cx + s * r * 2 / 5, cy - r * 5 / 4, max(1, r / 9), r * 2 / 5, pink);
      }
      blob(cx, cy, r * 9 / 10, r * 4 / 5, fur);
      eyes(cx, y, eg, er); cheeks(cx, y + er + 3, eg + er, er);
      gameFrame.fillEllipse(cx, y + er + 2, max(2, r / 9), max(1, r / 12), pink);
      smile(cx - 2, y + er + 4, 2, 2); smile(cx + 2, y + er + 4, 2, 2);
      break;
    }
    case KITTY: {
      const uint16_t fur = rgb(255,182,85), pink = rgb(255,146,182);
      for (int s = -1; s <= 1; s += 2) {
        ear(cx + s * r * 9 / 10, cy - r / 6, cx + s * r * 7 / 10, cy - r * 11 / 10, cx + s * r / 6, cy - r * 3 / 5, fur);
        gameFrame.fillTriangle(cx + s * r * 7 / 10, cy - r / 3, cx + s * r * 13 / 20, cy - r * 4 / 5, cx + s * r * 2 / 5, cy - r * 3 / 5, pink);
      }
      blob(cx, cy, r, r * 17 / 20, fur);
      // Tabby stripes on the forehead.
      for (int i = -1; i <= 1; ++i) gameFrame.fillRect(cx + i * r / 4 - 1, cy - r * 4 / 5, 3, r / 4, rgb(219,109,0));
      eyes(cx, ey, eg, er); cheeks(cx, ey + er + 4, eg + er, er);
      gameFrame.fillTriangle(cx - 3, ey + er + 1, cx + 3, ey + er + 1, cx, ey + er + 4, pink);
      smile(cx - 2, ey + er + 4, 2, 2); smile(cx + 2, ey + er + 4, 2, 2);
      for (int s = -1; s <= 1; s += 2) {
        gameFrame.drawLine(cx + s * r / 2, ey + er + 3, cx + s * r * 11 / 10, ey + er, ink);
        gameFrame.drawLine(cx + s * r / 2, ey + er + 6, cx + s * r * 11 / 10, ey + er + 8, ink);
      }
      break;
    }
    case PANDA: {
      for (int s = -1; s <= 1; s += 2) gameFrame.fillCircle(cx + s * r * 7 / 10, cy - r * 7 / 10, r / 3, ink);
      blob(cx, cy, r, r * 9 / 10, TFT_WHITE);
      for (int s = -1; s <= 1; s += 2) {
        gameFrame.fillEllipse(cx + s * eg, ey + 1, r / 4, r / 3, ink);
        gameFrame.fillCircle(cx + s * eg, ey, max(1, er - 1), TFT_WHITE);
        gameFrame.fillCircle(cx + s * eg, ey + 1, max(1, er / 2), ink);
      }
      cheeks(cx, ey + r / 3 + 3, eg + er + 2, er);
      gameFrame.fillEllipse(cx, ey + r / 3, max(2, r / 7), max(2, r / 10), ink);
      smile(cx, ey + r / 3 + 2, 3, 3);
      break;
    }
    case PUPPY: {
      const uint16_t fur = rgb(219,182,109), dark = rgb(146,73,36);
      blob(cx, cy, r * 9 / 10, r * 17 / 20, fur);
      // Floppy ears hanging over the sides.
      for (int s = -1; s <= 1; s += 2) blob(cx + s * r * 4 / 5, cy - r / 8, r / 4, r / 2, dark);
      gameFrame.fillCircle(cx + eg, ey - 1, er + 3, rgb(182,109,36));  // Patch over one eye.
      eyes(cx, ey, eg, er);
      blob(cx, cy + r / 3, r * 2 / 5, r / 4, rgb(255,255,219));
      gameFrame.fillEllipse(cx, cy + r / 4, max(2, r / 7), max(2, r / 10), ink);
      gameFrame.fillEllipse(cx + 1, cy + r / 2 + 2, max(2, r / 9), max(2, r / 8), rgb(255,73,109));  // Tongue.
      break;
    }
    case FROG: {
      const uint16_t skin = rgb(109,219,85);
      for (int s = -1; s <= 1; s += 2) blob(cx + s * eg, cy - r * 3 / 5, r / 3, r / 3, skin);
      blob(cx, cy, r, r * 3 / 4, skin);
      for (int s = -1; s <= 1; s += 2) {
        gameFrame.fillCircle(cx + s * eg, cy - r * 3 / 5, r / 4, TFT_WHITE);
        gameFrame.fillCircle(cx + s * eg, cy - r * 3 / 5 + 1, max(2, r / 7), ink);
      }
      cheeks(cx, cy + r / 6, r * 3 / 5, er + 1);
      smile(cx, cy + r / 8, r / 2, r / 4); smile(cx, cy + r / 8 + 1, r / 2, r / 4);
      break;
    }
    case PIGGY: {
      const uint16_t skin = rgb(255,182,219), dark = rgb(219,109,146);
      for (int s = -1; s <= 1; s += 2)
        ear(cx + s * r * 9 / 10, cy - r / 2, cx + s * r * 4 / 5, cy - r * 11 / 10, cx + s * r / 4, cy - r * 4 / 5, skin);
      blob(cx, cy, r, r * 17 / 20, skin);
      eyes(cx, ey - 2, eg, er);
      blob(cx, cy + r / 4, r * 2 / 5, r / 4, rgb(255,146,182), dark);
      for (int s = -1; s <= 1; s += 2) gameFrame.fillEllipse(cx + s * r / 7, cy + r / 4, max(1, r / 14), max(2, r / 9), dark);
      cheeks(cx, cy + r / 4, r * 7 / 10, er);
      break;
    }
    case CHICK: {
      const uint16_t fluff = rgb(255,219,36);
      for (int i = -1; i <= 1; ++i) blob(cx + i * r / 6, cy - r * 9 / 10, max(2, r / 10), r / 4, fluff);
      blob(cx, cy, r * 9 / 10, r * 9 / 10, fluff);
      eyes(cx, ey, eg, er); cheeks(cx, ey + er + 4, eg + er, er);
      gameFrame.fillTriangle(cx - r / 5, ey + er + 1, cx + r / 5, ey + er + 1, cx, ey + er + 1 + r / 4, rgb(255,109,0));
      break;
    }
    case PENGUIN: {
      const uint16_t navy = rgb(36,36,109);
      blob(cx, cy, r, r * 9 / 10, navy);
      // White heart-shaped face.
      gameFrame.fillCircle(cx - r / 3, cy - r / 10, r / 2, TFT_WHITE);
      gameFrame.fillCircle(cx + r / 3, cy - r / 10, r / 2, TFT_WHITE);
      gameFrame.fillEllipse(cx, cy + r / 4, r * 3 / 5, r / 2, TFT_WHITE);
      eyes(cx, ey, eg - 1, er); cheeks(cx, ey + er + 4, eg + er - 1, er);
      gameFrame.fillTriangle(cx - r / 6, ey + er + 2, cx + r / 6, ey + er + 2, cx, ey + er + 2 + r / 5, rgb(255,146,0));
      break;
    }
  }
}

// ---- Cards ------------------------------------------------------------------
void drawCardBack(int x, int y, int w, int h, bool details) {
  gameFrame.fillRoundRect(x, y, w, h, 8, rgb(255,73,146));
  if (!details) return;
  gameFrame.drawRoundRect(x + 3, y + 3, w - 6, h - 6, 6, rgb(255,182,219));
  const int cx = x + w / 2, cy = y + h / 2, r = max(4, min(w, h) / 6);
  for (int s = -1; s <= 1; s += 2) {
    gameFrame.fillCircle(x + 8, cy + s * (h / 2 - 9), 2, rgb(255,146,219));
    gameFrame.fillCircle(x + w - 9, cy + s * (h / 2 - 9), 2, rgb(255,146,219));
  }
  drawHeartShape(cx, cy - r / 2 - 1, r, TFT_WHITE);
  gameFrame.fillCircle(cx - r - r / 3, cy - r / 2 - r / 3, max(1, r / 4), rgb(255,182,219));
}

void drawCardFace(int i, int x, int y, int w, int h, bool details, uint32_t now) {
  const bool glow = matched[i];
  gameFrame.fillRoundRect(x, y, w, h, 8, glow ? COLOR_GOLD : rgb(255,146,219));
  gameFrame.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 7, TFT_WHITE);
  if (!details) return;
  if (glow) gameFrame.drawRoundRect(x + 3, y + 3, w - 6, h - 6, 6, COLOR_GOLD);
  const TouchButton &b = cardButtons[i];
  // Matched cards do a little hop for a moment.
  const int hop = glow && now < cheerUntil && (i == first || i == second) ? (int)(4 * fabsf(sinf(now / 70.0f))) : 0;
  drawAnimal(animal[i], b.x + b.w / 2, b.y + b.h / 2 + 2 - hop, min(b.w, b.h) * 7 / 20);
}

void drawCard(int i, uint32_t now) {
  const TouchButton &b = cardButtons[i];
  const float p = min(1.0f, (now - flipAt[i]) / (float)flipMs);
  // The card squeezes edge-on, then opens on its other side.
  const float squeeze = fabsf(cosf(p * PI));
  const bool showFace = p < 0.5f ? !faceUp[i] : faceUp[i];
  const int w = max(2, (int)(b.w * squeeze)), x = b.x + (b.w - w) / 2;
  gameFrame.fillRoundRect(x + 2, b.y + 3, w, b.h, 8, rgb(219,146,219));
  if (showFace) drawCardFace(i, x, b.y, w, b.h, squeeze > 0.85f, now);
  else drawCardBack(x, b.y, w, b.h, squeeze > 0.6f);
}

bool flipping(uint32_t now) {
  for (int i = 0; i < cardCount; ++i) if (now - flipAt[i] < flipMs) return true;
  return false;
}

// ---- Screens ----------------------------------------------------------------
void header(const char *title, uint16_t fill) {
  gameFrame.fillRect(0, 0, 320, 34, fill);
  gameFrame.drawFastHLine(0, 34, 320, GameGfx::darker(fill));
  gameFrame.fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 10, TFT_WHITE);
  gameFrame.drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 10, GameGfx::darker(fill));
  gameFrame.setTextColor(rgb(182,36,146));
  gameFrame.drawCentreString("BACK", exitButton.x + exitButton.w / 2, exitButton.y + 5, 2);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString(title, 194, 9, 2);
  gameFrame.drawCentreString(title, 195, 9, 2);
}

void buddy(int x, int y, uint16_t color) {
  blob(x, y, 9, 9, color);
  gameFrame.fillCircle(x - 3, y - 1, 1, ink);
  gameFrame.fillCircle(x + 3, y - 1, 1, ink);
  smile(x, y + 2, 3, 2);
}
void robot(int x, int y) {
  gameFrame.drawFastVLine(x, y - 13, 4, ink);
  gameFrame.fillCircle(x, y - 13, 2, rgb(255,73,85));
  gameFrame.fillRoundRect(x - 10, y - 9, 20, 18, 4, ink);
  gameFrame.fillRoundRect(x - 9, y - 8, 18, 16, 3, rgb(182,182,219));
  gameFrame.fillRect(x - 6, y - 4, 4, 4, rgb(73,146,255));
  gameFrame.fillRect(x + 2, y - 4, 4, 4, rgb(73,146,255));
  gameFrame.drawFastHLine(x - 4, y + 3, 9, ink);
}

void drawChoice(int i, const char *label, const char *detail, uint32_t now) {
  const TouchButton &b = choiceButtons[i];
  const uint16_t accent = i == 0 ? rgb(255,73,182) : i == 1 ? rgb(182,73,255) : rgb(73,146,255);
  gameFrame.fillRoundRect(b.x + 2, b.y + 4, b.w, b.h, 14, rgb(182,73,182));
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 14, accent);
  gameFrame.fillRoundRect(b.x + 3, b.y + 3, b.w - 6, b.h - 6, 11, TFT_WHITE);
  const int ix = b.x + 30, iy = b.y + b.h / 2;
  if (stage == MODE) {
    if (i == SOLO) buddy(ix, iy, rgb(255,182,219));
    else if (i == TWO_PLAYER) { buddy(ix - 9, iy, rgb(255,182,219)); buddy(ix + 9, iy, rgb(146,182,255)); }
    else { buddy(ix - 10, iy, rgb(255,182,219)); robot(ix + 11, iy + 2); }
  } else {
    // Tiny face-down cards, one per pair.
    const int pairs = cardCounts[i] / 2;
    for (int k = 0; k < pairs; ++k) {
      const int cx = ix - (pairs - 1) * 4 + k * 8, bob = (int)((now / 250 + k) % pairs == 0) * 2;
      gameFrame.fillRoundRect(cx - 4, iy - 8 - bob, 9, 16, 2, rgb(255,73,146));
      gameFrame.drawRoundRect(cx - 4, iy - 8 - bob, 9, 16, 2, TFT_WHITE);
    }
  }
  const int textX = b.x + 60, room = b.x + b.w - 12 - textX;
  const int font = gameFrame.textWidth(label, 4) <= room ? 4 : 2;
  gameFrame.setTextColor(GameGfx::darker(accent));
  gameFrame.drawString(label, textX, b.y + (detail ? 3 : (b.h - (font == 4 ? 18 : 14)) / 2), font);
  if (detail) {
    gameFrame.setTextColor(rgb(146,73,146));
    gameFrame.drawString(detail, textX, b.y + 29, 1);
  }
}

void renderMenu(uint32_t now) {
  drawPastelBackdrop(now);
  drawCandyHills();
  header("MEMORY MATCH", rgb(255,146,219));
  if (stage == MODE) {
    drawLabelPill("WHO IS PLAYING?", 44, rgb(182,36,146), rgb(255,73,182));
    for (int i = 0; i < 3; ++i) drawChoice(i, modeNames[i], nullptr, now);
  } else {
    drawLabelPill("HOW MANY CARDS?", 44, rgb(182,36,146), rgb(255,73,182));
    for (int i = 0; i < 3; ++i) {
      char detail[32];
      if (mode == SOLO && best[i]) snprintf(detail, sizeof(detail), "%d CARDS - BEST: %d MOVES", cardCounts[i], best[i]);
      else snprintf(detail, sizeof(detail), "%d CARDS", cardCounts[i]);
      drawChoice(i, difficultyNames[i], detail, now);
    }
  }
}

void scoreChip(int p, int x) {
  const bool active = p == turn && stage == PLAY;
  const uint16_t c = playerColors[p];
  gameFrame.fillRoundRect(x, 38, 128, 22, 11, active ? c : TFT_WHITE);
  gameFrame.drawRoundRect(x, 38, 128, 22, 11, c);
  char text[20];
  snprintf(text, sizeof(text), "%s: %d", playerName(p), score[p]);
  gameFrame.setTextColor(active ? TFT_WHITE : GameGfx::darker(c));
  gameFrame.drawCentreString(text, x + 64, 41, 2);
  if (active) gameFrame.drawCentreString(text, x + 65, 41, 2);
}

void renderPlay(uint32_t now) {
  bands(0, 0, 320, 240, rgb(255,219,255), rgb(219,182,255), 10);
  for (int i = 0; i < 12; ++i) {
    const int x = (i * 83 + 17) % 320, y = 40 + (i * 47) % 190;
    if ((now / 150 + i * 5) % 16 < 3) { gameFrame.drawFastHLine(x - 3, y, 7, TFT_WHITE); gameFrame.drawFastVLine(x, y - 3, 7, TFT_WHITE); }
    else gameFrame.drawPixel(x, y, TFT_WHITE);
  }
  // The header takes the current player's color so it's clear whose turn it is.
  const bool cheering = now < cheerUntil;
  char title[28];
  if (stage == DONE) snprintf(title, sizeof(title), "ALL MATCHED!");
  else if (cheering) snprintf(title, sizeof(title), "IT'S A MATCH!");
  else if (mode == SOLO) snprintf(title, sizeof(title), "FIND THE PAIRS!");
  else if (computerTurn()) snprintf(title, sizeof(title), "COMPUTER IS THINKING...");
  else if (mode == VS_COMPUTER) snprintf(title, sizeof(title), "YOUR TURN!");
  else snprintf(title, sizeof(title), "PLAYER %d'S TURN", turn + 1);
  header(title, mode == SOLO ? headerColors[0] : headerColors[turn]);

  if (mode == SOLO) {
    char text[32];
    if (best[difficulty]) snprintf(text, sizeof(text), "MOVES: %d     BEST: %d", moves, best[difficulty]);
    else snprintf(text, sizeof(text), "MOVES: %d", moves);
    drawLabelPill(text, 38, rgb(182,36,146), rgb(255,73,182));
  } else {
    scoreChip(0, 8);
    scoreChip(1, 184);
  }
  for (int i = 0; i < cardCount; ++i) drawCard(i, now);
}

void renderDone(uint32_t now) {
  GameGfx::darkenRect(0, 64, 320, 176);
  gameFrame.fillRoundRect(42, 72, 240, 146, 18, rgb(182,73,182));
  gameFrame.fillRoundRect(40, 68, 240, 146, 18, TFT_WHITE);
  gameFrame.drawRoundRect(40, 68, 240, 146, 18, rgb(255,73,182));
  char big[24], small[32];
  if (mode == SOLO) {
    snprintf(big, sizeof(big), newBest ? "NEW BEST!" : "YOU DID IT!");
    snprintf(small, sizeof(small), "ALL PAIRS IN %d MOVES", moves);
  } else {
    const int winner = score[0] > score[1] ? 0 : score[1] > score[0] ? 1 : -1;
    if (winner < 0) snprintf(big, sizeof(big), "IT'S A TIE!");
    else if (mode == VS_COMPUTER) snprintf(big, sizeof(big), winner ? "COMPUTER WINS!" : "YOU WIN!");
    else snprintf(big, sizeof(big), "PLAYER %d WINS!", winner + 1);
    snprintf(small, sizeof(small), "%d PAIRS TO %d", max(score[0], score[1]), min(score[0], score[1]));
  }
  for (int s = -1; s <= 1; s += 2) drawHeartShape(160 + s * 96, 86 + (int)(3 * sinf(now / 200.0f + s)), 6, rgb(255,73,146));
  gameFrame.setTextColor(rgb(219,36,146));
  gameFrame.drawCentreString(big, 160, 84, 4);
  gameFrame.setTextColor(rgb(146,73,146));
  gameFrame.drawCentreString(small, 160, 114, 2);
  // A row of the animals that were in this game.
  const int pairs = cardCount / 2;
  for (int k = 0; k < pairs; ++k) {
    int a = -1;
    for (int i = 0, n = 0; i < cardCount && a < 0; ++i) {
      bool seen = false;
      for (int j = 0; j < i; ++j) if (animal[j] == animal[i]) seen = true;
      if (!seen && n++ == k) a = animal[i];
    }
    const int bob = (int)(3 * sinf(now / 180.0f + k));
    drawAnimal(a, 160 - (pairs - 1) * 18 + k * 36, 148 - bob, 12);
  }
  const TouchButton *buttons[2] = {&againButton, &newGameButton};
  const char *labels[2] = {"PLAY AGAIN", "NEW GAME"};
  for (int i = 0; i < 2; ++i) {
    const TouchButton &b = *buttons[i];
    const uint16_t c = i ? rgb(182,73,255) : rgb(255,73,182);
    gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 14, c);
    gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 14, TFT_WHITE);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(labels[i], b.x + b.w / 2, b.y + 8, 2);
  }
  for (const Heart &h : hearts)
    if (h.life > 0) drawHeartShape((int)h.x, (int)h.y, 2 + (int)(h.life * 3), h.life > 0.5f ? rgb(255,73,146) : rgb(255,182,219));
}

void render() {
  const uint32_t now = millis();
  lastFrame = now;
  if (stage == MODE || stage == DIFFICULTY) renderMenu(now);
  else {
    renderPlay(now);
    if (stage == DONE) renderDone(now);
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

// ---- Game logic -------------------------------------------------------------
void deal() {
  cardCount = cardCounts[difficulty];
  const int cols = gridCols[difficulty], rows = cardCount / cols, gap = 6;
  const int cw = (304 - (cols - 1) * gap) / cols, ch = (172 - (rows - 1) * gap) / rows;
  const int x0 = (320 - (cols * cw + (cols - 1) * gap)) / 2;
  // Pick a different set of animals each game, then shuffle the pairs.
  uint8_t pool[ANIMAL_COUNT];
  for (int i = 0; i < ANIMAL_COUNT; ++i) pool[i] = i;
  for (int i = ANIMAL_COUNT - 1; i > 0; --i) { const int j = random(0, i + 1); const uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
  for (int i = 0; i < cardCount; ++i) animal[i] = pool[i / 2];
  for (int i = cardCount - 1; i > 0; --i) { const int j = random(0, i + 1); const uint8_t t = animal[i]; animal[i] = animal[j]; animal[j] = t; }
  for (int i = 0; i < cardCount; ++i) {
    cardButtons[i] = {(int16_t)(x0 + (i % cols) * (cw + gap)), (int16_t)(64 + (i / cols) * (ch + gap)), (int16_t)cw, (int16_t)ch};
    faceUp[i] = matched[i] = false;
    flipAt[i] = 0;
    memory[i] = -1;
  }
  first = second = -1;
  turn = 0; score[0] = score[1] = 0; moves = 0; pairsLeft = cardCount / 2;
  resolveAt = finishAt = cheerUntil = 0; newBest = false;
  for (Heart &h : hearts) h.life = 0;
  stage = PLAY;
}

void flip(int i, uint32_t now) {
  faceUp[i] = true;
  flipAt[i] = now;
  GameAudio::play(GameAudio::Click);
  // The computer watches every card that is turned over, but sometimes forgets.
  if (mode == VS_COMPUTER && random(0, 100) < computerRecall[difficulty]) memory[i] = animal[i];
  if (first < 0) first = i;
  else {
    second = i;
    resolveAt = now + (animal[first] == animal[second] ? 650 : 1100);
  }
}

bool hidden(int i) { return !matched[i] && !faceUp[i]; }

int randomHidden(int except, bool unknownOnly) {
  int options[maxCards], n = 0;
  for (int i = 0; i < cardCount; ++i)
    if (i != except && hidden(i) && (!unknownOnly || memory[i] < 0)) options[n++] = i;
  return n ? options[random(0, n)] : -1;
}

int computerPick() {
  if (first < 0) {
    // A pair it remembers? Go for it.
    for (int i = 0; i < cardCount; ++i)
      for (int j = i + 1; j < cardCount; ++j)
        if (hidden(i) && hidden(j) && memory[i] >= 0 && memory[i] == memory[j]) return i;
  } else {
    for (int j = 0; j < cardCount; ++j)
      if (hidden(j) && memory[j] == animal[first]) return j;
  }
  const int pick = randomHidden(first, true);
  return pick >= 0 ? pick : randomHidden(first, false);
}

void resolve(uint32_t now) {
  ++moves;
  if (animal[first] == animal[second]) {
    matched[first] = matched[second] = true;
    ++score[turn];
    --pairsLeft;
    cheerUntil = now + 600;
    GameAudio::play(GameAudio::Happy);
    if (!pairsLeft) finishAt = now + 700;
    // A match earns another turn, so first/second stay set for the hop.
  } else {
    faceUp[first] = faceUp[second] = false;
    flipAt[first] = flipAt[second] = now;
    GameAudio::play(GameAudio::Hit);
    if (mode != SOLO) turn ^= 1;
  }
  resolveAt = 0;
  computerAt = now + 800;
}

void finish() {
  stage = DONE;
  finishAt = 0;
  if (mode == SOLO && (!best[difficulty] || moves < best[difficulty])) {
    newBest = best[difficulty] != 0;
    best[difficulty] = min(moves, 255);
    saveBest();
  }
  const bool lost = mode == VS_COMPUTER && score[1] > score[0];
  GameAudio::play(lost ? GameAudio::Lose : GameAudio::Win);
  if (!lost)
    for (int i = 0; i < 10; ++i) hearts[i] = {60.0f + random(0, 200), 230.0f + random(0, 40), -(float)random(60, 120), 1.0f};
}

int navItems(const TouchButton **items) {
  if (stage == MODE || stage == DIFFICULTY) { for (int i = 0; i < 3; ++i) items[i] = &choiceButtons[i]; return 3; }
  if (stage == DONE) { items[0] = &againButton; items[1] = &newGameButton; return 2; }
  if (computerTurn()) return 0;
  for (int i = 0; i < cardCount; ++i) items[i] = &cardButtons[i];
  return cardCount;
}
int navContext() { return (int)stage * 4 + (stage >= PLAY ? difficulty : 0); }

void pause(uint32_t ms) {
  for (int i = 0; i < cardCount; ++i) flipAt[i] += ms;
  if (resolveAt) resolveAt += ms;
  if (finishAt) finishAt += ms;
  computerAt += ms; cheerUntil += ms; lastFrame += ms;
}

void start() { stage = MODE; }

void update(uint32_t now, bool tapped, uint16_t x, uint16_t y) {
  if (tapped && exitButton.contains(x, y)) {
    GameAudio::play(GameAudio::Click);
    if (stage == MODE) { drawGameList(); return; }
    stage = stage == DIFFICULTY ? MODE : DIFFICULTY;
    render();
    return;
  }
  if (tapped && stage <= DIFFICULTY) {
    for (int i = 0; i < 3; ++i)
      if (choiceButtons[i].contains(x, y)) {
        GameAudio::play(GameAudio::Click);
        if (stage == MODE) { mode = (Mode)i; stage = DIFFICULTY; }
        else { difficulty = i; deal(); GameAudio::play(GameAudio::Start); computerAt = now + 800; }
        render();
        return;
      }
  } else if (tapped && stage == DONE) {
    if (againButton.contains(x, y)) { deal(); GameAudio::play(GameAudio::Start); render(); return; }
    if (newGameButton.contains(x, y)) { stage = MODE; GameAudio::play(GameAudio::Click); render(); return; }
  } else if (tapped && stage == PLAY && !computerTurn() && !resolveAt && !finishAt) {
    for (int i = 0; i < cardCount; ++i)
      if (cardButtons[i].contains(x, y) && hidden(i)) {
        if (second >= 0) first = second = -1;  // Clear the pair from a previous match.
        flip(i, now);
        render();
        return;
      }
  }

  if (stage == PLAY) {
    if (resolveAt && (int32_t)(now - resolveAt) >= 0) resolve(now);
    else if (finishAt && (int32_t)(now - finishAt) >= 0) finish();
    else if (computerTurn() && !resolveAt && !finishAt && (int32_t)(now - computerAt) >= 0) {
      if (second >= 0) first = second = -1;
      const int pick = computerPick();
      if (pick >= 0) flip(pick, now);
      computerAt = now + 750;
    }
  }
  if (stage == DONE) {
    const float dt = min(0.08f, (now - lastFrame) / 1000.0f);
    for (Heart &h : hearts) if (h.life > 0) { h.y += h.vy * dt; h.life -= dt * 0.5f; }
  }
  // Smooth frames while cards flip or hop; a gentle pace otherwise for the sparkles.
  const bool busy = stage >= PLAY && (flipping(now) || now < cheerUntil || stage == DONE);
  if (now - lastFrame >= (busy ? 30u : 80u)) render();
}
} // namespace Memory

void beginMemoryMatch() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  gameState = MEMORY_MATCH;
  Memory::start();
  Memory::render();
}
