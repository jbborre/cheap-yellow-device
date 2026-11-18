#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
#include "art/BlackKnightArt.h"
#include <math.h>

// Secret side-scrolling brawler, unlocked by holding all four buttons on the
// home screen. Enemies attack in waves that lock the screen until cleared,
// then a boss waits at the end. Everyone fights on one walking plane.
// Level 1: pirates and Davy Jones on a shipwreck.
// Level 2: crowbar thugs and Brock on a run-down city street.
namespace BlackKnight {
using GameGfx::rgb;
enum Stage { SELECT, ACTIVE, VICTORY, DEFEAT };
enum Hero { KNIGHT, ECLIPSE, SPADE };
struct HeroStats {
  const char *name; int health; float speed, punchReach, kickReach, punchCooldown, kickCooldown;
};
const HeroStats heroes[3] = {
  {"BLACK KNIGHT", 12, 72, 40, 40, 0.34f, 0.50f}, // Sword swings reach farther.
  {"ECLIPSE",      10, 86, 30, 40, 0.26f, 0.44f},
  {"SPADE",         8, 100, 28, 44, 0.20f, 0.36f}, // Fastest karate.
};
const TouchButton heroCards[3] = {{5,38,100,198},{110,38,100,198},{215,38,100,198}};
const TouchButton exitButton = {5, 4, 52, 26};
const TouchButton retryButton = {40, 158, 112, 38};
const TouchButton menuButton = {168, 158, 112, 38};

constexpr int floorY = 206;           // Feet of everyone on the deck.
constexpr float levelEnd = 1800;
constexpr float arenaStart = levelEnd - 320;
constexpr int waveCount = 4;
const float waveAt[waveCount] = {260, 620, 980, 1340};
const int waveSize[waveCount] = {3, 4, 5, 6};
constexpr int maxEnemies = 6, maxAlive = 3, maxSparks = 4;
constexpr float superWindup = 1.5f, superRadius = 52;
// Brock flashes red, then charges across the street until he hits a wall
// or runs a little past where the player stood.
constexpr float brockFlash = 1.2f, brockDashSpeed = 260, brockOvershoot = 60;
constexpr int levelCount = 2;

Stage stage = SELECT;
int hero = KNIGHT;
int level = 0;
int bossMaxHp() { return level ? 12 : 10; }
uint32_t lastFrame = 0;
float camera = 0, shake = 0, banner = 0, goTimer = 0, endTimer = 0;
int kos = 0;
// Button presses last one loop; hold them until the next 30 fps game step.
uint8_t pendingPresses = 0;

struct Player {
  float x; int facing, hp; float immunity, attackAnim, cooldown, walk; int attackKind; bool moving;
} player;

struct Enemy {
  float x, vx; int hp; uint8_t type; float windup, rest, stun, flash, dying, walk; int facing; bool active, moving;
} enemies[maxEnemies];

struct Boss {
  float x, startX, lift; int hp, facing; float windup, rest, stun, flash, superTimer, charge, superX, slamFx, dying, walk, dash, daze;
  bool active, moving;
} boss;

int wave = 0, spawned = 0;
bool waveActive = false;
float spawnTimer = 0;

struct Spark { float x, y, t; } sparks[maxSparks];

// ---- Drawing ----------------------------------------------------------------
// Fighters and scenery are pre-rendered images from include/art/BlackKnightArt.h
// (made by tools/art). Every frame faces right and is flipped to face left.
using GameGfx::Image;
namespace Art = BlackKnightArt;
enum Pose { IDLE, PUNCH, KICK, WINDUP, HURT, RAISE, DASH };
struct Frames { const Image *idle, *walk1, *walk2, *punch, *kick, *windup, *raise, *hurt, *dash; };
const Frames heroFrames[3] = {
  {&Art::knightIdle, &Art::knightWalk1, &Art::knightWalk2, &Art::knightPunch, &Art::knightKick},
  {&Art::eclipseIdle, &Art::eclipseWalk1, &Art::eclipseWalk2, &Art::eclipsePunch, &Art::eclipseKick},
  {&Art::spadeIdle, &Art::spadeWalk1, &Art::spadeWalk2, &Art::spadePunch, &Art::spadeKick},
};
const Frames pirateFrames = {&Art::pirateIdle, &Art::pirateWalk1, &Art::pirateWalk2, nullptr, nullptr, &Art::pirateWindup, nullptr, &Art::pirateHurt};
const Frames bruteFrames = {&Art::bruteIdle, &Art::bruteWalk1, &Art::bruteWalk2, nullptr, nullptr, &Art::bruteWindup, nullptr, &Art::bruteHurt};
const Frames captainFrames = {&Art::captainIdle, &Art::captainWalk1, &Art::captainWalk2, nullptr, nullptr, &Art::captainWindup, &Art::captainRaise, &Art::captainHurt};
const Frames thugFrames = {&Art::thugIdle, &Art::thugWalk1, &Art::thugWalk2, nullptr, nullptr, &Art::thugWindup, nullptr, &Art::thugHurt};
const Frames bigThugFrames = {&Art::bigThugIdle, &Art::bigThugWalk1, &Art::bigThugWalk2, nullptr, nullptr, &Art::bigThugWindup, nullptr, &Art::bigThugHurt};
const Frames brockFrames = {&Art::brockIdle, &Art::brockWalk1, &Art::brockWalk2, nullptr, nullptr, &Art::brockWindup, nullptr, &Art::brockHurt, &Art::brockDash};

// An action pose if the character has one, otherwise standing or a walk step.
const Image &frameFor(const Frames &f, Pose pose, float walk, bool moving) {
  const Image *const action[] = {nullptr, f.punch, f.kick, f.windup, f.hurt, f.raise, f.dash};
  if (action[pose]) return *action[pose];
  if (!moving) return *f.idle;
  switch (((int)(walk / (PI / 2))) & 3) {  // Step, pass, other step, pass.
    case 0: return *f.walk1;
    case 2: return *f.walk2;
    default: return *f.idle;
  }
}

void drawFigure(const Image &img, int x, int foot, int d, float s = 1.0f) {
  if (s == 1.0f) GameGfx::drawImage(img, x, foot, d < 0);
  else GameGfx::drawImageScaled(img, x, foot, s, d < 0);
}

void shadow(int x, int f, float s) { GameGfx::darkenEllipse(x, f + 1, (int)(14 * s), max(2, (int)(3 * s))); }

void starDot(int x, int y, uint16_t c) {
  gameFrame.drawFastHLine(x - 2, y, 5, c);
  gameFrame.drawFastVLine(x, y - 2, 5, c);
}

void hitFlash(int x, int f, float s) {
  GameGfx::screenRect(x - (int)(13 * s), f - (int)(58 * s), (int)(26 * s), (int)(58 * s), TFT_WHITE);
}

void drawHero(int which, int x, int f, int d, float walk, Pose pose, float s = 1.0f, bool moving = false) {
  shadow(x, f, s);
  drawFigure(frameFor(heroFrames[which], pose, walk, moving), x, f, d, s);
}

void drawEnemy(const Enemy &e, int x) {
  const bool big = e.type == 1;
  const float s = big ? (level ? 1.3f : 1.25f) : 1.0f;
  const Frames &f = level ? (big ? bigThugFrames : thugFrames) : (big ? bruteFrames : pirateFrames);
  const Pose pose = e.dying > 0 || e.stun > 0 ? HURT : e.windup > 0 ? WINDUP : IDLE;
  shadow(x, floorY, s);
  drawFigure(frameFor(f, pose, e.walk, e.moving), x, floorY, e.facing);
  if (e.flash > 0) hitFlash(x, floorY, s);
}

// Davy Jones, the ghost captain.
void drawBoss(int x) {
  const float s = 1.55f;
  const Pose pose = boss.dying > 0 ? HURT : boss.charge > 0 ? RAISE : boss.windup > 0 ? WINDUP : IDLE;
  const int lift = (int)boss.lift;
  shadow(x, floorY, s * max(0.4f, 1 - lift / 120.0f));
  drawFigure(frameFor(captainFrames, pose, boss.walk, boss.moving), x, floorY - lift, boss.facing);
  if (boss.flash > 0) hitFlash(x, floorY - lift, s);
}

void drawBrock(int x, uint32_t now) {
  const float s = 1.55f;
  const int d = boss.facing;
  const bool dashing = boss.dash > 0;
  if (dashing) {
    // Speed lines and kicked-up dust behind the charge.
    for (int i = 0; i < 4; ++i)
      gameFrame.drawFastHLine(x - d * (int)((24 + i * 5) * s) - (d > 0 ? (int)(16 * s) : 0), floorY - (int)((14 + i * 11) * s), (int)(16 * s), rgb(219,219,219));
    for (int i = 0; i < 3; ++i)
      gameFrame.fillCircle(x - d * (int)((14 + i * 9) * s), floorY - (int)((2 + i) * s), (int)((4 - i) * s), rgb(146,146,146));
  }
  const Pose pose = boss.dying > 0 ? HURT : dashing ? DASH : boss.charge > 0 || boss.windup > 0 ? WINDUP : IDLE;
  shadow(x, floorY, s);
  drawFigure(frameFor(brockFrames, pose, boss.walk, boss.moving), x, floorY, d);
  // Stars circling his head after he slams into a wall.
  if (boss.daze > 0) {
    const int hx = x + d * (int)(2 * s), hy = floorY - (int)(52 * s);
    for (int i = 0; i < 3; ++i) {
      const float a = now / 150.0f + i * 2.09f;
      starDot(hx + (int)(cosf(a) * 11 * s), hy - (int)(12 * s) + (int)(sinf(a) * 3 * s), rgb(255,255,0));
    }
  }
  // The tell for his charge: he flashes red.
  if (boss.charge > 0 && (now / 100) % 2) {
    GameGfx::glow(x, floorY - (int)(30 * s), (int)(40 * s), rgb(255,0,0));
    GameGfx::screenRect(x - (int)(14 * s), floorY - (int)(60 * s), (int)(28 * s), (int)(60 * s), rgb(255,0,0));
  }
  if (boss.flash > 0) hitFlash(x, floorY, s);
}

// ---- Shipwreck backdrop ----------------------------------------------------
void drawShip(int cam, uint32_t now) {
  GameGfx::drawImage(Art::shipSky, -(int)(cam * 80 / levelEnd), 32);
  // Moonlit wave glints drifting over the sea.
  for (int y = 134; y < 156; y += 6)
    for (int x = -((cam / 3 + (int)(now / 40)) % 32); x < 320; x += 32)
      gameFrame.drawFastHLine(x + (y % 12), y, 6, rgb(73,109,170));
  const int masts[] = {150, 700, 1200};
  for (int m : masts) if (m - cam > -80 && m - cam < 420) GameGfx::drawImage(Art::shipMast, m - cam, 166);
  GameGfx::drawStrip(Art::shipRailing, 148, cam);
  GameGfx::drawStrip(Art::shipDeck, 166, cam);
  const int holes[] = {420, 910, 1270};
  for (int h : holes) if (h - cam > -40 && h - cam < 360) GameGfx::drawImage(Art::shipHole, h - cam, 184);
  // Props sit behind the fighting line.
  const int barrels[] = {320, 560, 1050, 1420}, cannons[] = {480, 860}, crates[] = {760, 1150};
  for (int b : barrels) if (b - cam > -20 && b - cam < 340) GameGfx::drawImage(Art::shipBarrel, b - cam, 186);
  for (int c : cannons) if (c - cam > -30 && c - cam < 350) GameGfx::drawImage(Art::shipCannon, c - cam, 186);
  for (int c : crates) if (c - cam > -20 && c - cam < 340) GameGfx::drawImage(Art::shipCrate, c - cam, 186);
  // Captain's quarters: ship's wheel and lanterns at the stern.
  if (cam > arenaStart - 340) {
    GameGfx::drawImage(Art::shipWheel, 1700 - cam, 176);
    const int lamps[] = {1520, 1780};
    for (int l : lamps) {
      GameGfx::glow(l - cam, 110, 16, rgb(255,182,0));
      GameGfx::drawImage(Art::shipLantern, l - cam, 168);
    }
  }
}

// ---- Run-down city backdrop ---------------------------------------------------
constexpr int streetTop = 166;
const Image *const cityBuildings[] = {
  &Art::cityBuilding0, &Art::cityBuilding1, &Art::cityBuilding2, &Art::cityBuilding3, &Art::cityBuilding4, &Art::cityBuilding5,
  &Art::cityBuilding1, &Art::cityBuilding0, &Art::cityBuilding3, &Art::cityBuilding2, &Art::cityBuilding4, &Art::cityBuildingBrock,
};

void drawCity(int cam, uint32_t now) {
  GameGfx::drawImage(Art::citySky, -(int)(cam * 80 / levelEnd), 32);
  // Buildings stand side by side with narrow alleys between them; Brock's is last.
  int bx = -40;
  for (const Image *b : cityBuildings) {
    const int w = b->w - 6;
    if (bx + w - cam > -10 && bx - cam < 330) GameGfx::drawImage(*b, bx - cam, streetTop);
    bx += w + 10;
  }
  GameGfx::drawStrip(Art::cityStreet, streetTop, cam);
  const int potholes[] = {380, 820, 1260};
  for (int p : potholes) if (p - cam > -30 && p - cam < 350) GameGfx::drawImage(Art::cityPothole, p - cam, 214);
  // Props sit on the sidewalk behind the fighting line.
  const int lamps[] = {240, 690, 1140, 1520};
  for (int i = 0; i < 4; ++i) {
    const int x = lamps[i] - cam;
    if (x < -40 || x > 360) continue;
    if (i % 2) { GameGfx::drawImage(Art::cityLampBroken, x, streetTop + 8); continue; }
    const bool on = (now / 60 + lamps[i]) % 23 > 3;  // Buzzing, flickering bulb.
    if (on) GameGfx::glow(x + 15, 94, 22, rgb(255,219,109));
    GameGfx::drawImage(on ? Art::cityLampOn : Art::cityLampOff, x, streetTop + 8);
  }
  const int cans[] = {330, 780, 1240}, barrels[] = {520, 1000, 1560, 1760};
  for (int c : cans) if (c - cam > -30 && c - cam < 350) GameGfx::drawImage(Art::cityTrashCan, c - cam, streetTop + 12);
  const Image *const flames[3] = {&Art::cityFireBarrel0, &Art::cityFireBarrel1, &Art::cityFireBarrel2};
  for (int b : barrels) {
    const int x = b - cam;
    if (x < -40 || x > 360) continue;
    GameGfx::glow(x, streetTop - 18, 26, rgb(255,146,0));
    GameGfx::drawImage(*flames[(now / 110 + b) % 3], x, streetTop + 12);
  }
}

// ---- Screens --------------------------------------------------------------
void header(const char *title) {
  GameGfx::gradient(0, 0, 320, 32, rgb(36,0,0), rgb(0,0,0));
  gameFrame.drawFastHLine(0, 31, 320, rgb(146,0,0));
  gameFrame.fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h - 3, 5, rgb(73,0,0));
  gameFrame.drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h - 3, 5, TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawString("BACK", 13, 8, 2);
  if (title) {
    gameFrame.setTextColor(rgb(255,36,0));
    gameFrame.drawCentreString(title, 186, 8, 2);
  }
}

void statBar(int x, int y, const char *label, float value, float most, uint16_t color, int w) {
  gameFrame.setTextColor(rgb(182,182,170));
  gameFrame.drawString(label, x, y, 1);
  gameFrame.drawRect(x, y + 10, w, 7, rgb(109,73,73));
  gameFrame.fillRect(x + 1, y + 11, (int)((w - 2) * value / most), 5, color);
}

void renderSelect() {
  GameGfx::gradient(0, 32, 320, 208, rgb(36,0,0), rgb(0,0,0));
  header("BLACK KNIGHT");
  for (int i = 0; i < 3; ++i) {
    const TouchButton &b = heroCards[i];
    const HeroStats &h = heroes[i];
    const int cx = b.x + b.w / 2;
    gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 10, rgb(36,0,36));
    gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 10, rgb(146,0,0));
    GameGfx::glow(cx, b.y + 52, 34, rgb(109,0,36));
    drawHero(i, cx, b.y + 88, 1, 0, IDLE, 1.15f);
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(h.name, cx, b.y + 96, 2);
    statBar(b.x + 8, b.y + 118, "HEALTH", h.health, 12, rgb(255,36,85), b.w - 16);
    statBar(b.x + 8, b.y + 138, "SPEED", h.speed, 100, rgb(0,219,255), b.w - 16);
    statBar(b.x + 8, b.y + 158, "REACH", h.punchReach, 40, rgb(255,182,0), b.w - 16);
    gameFrame.setTextColor(rgb(255,219,85));
    gameFrame.drawCentreString("TAP TO FIGHT", cx, b.y + 182, 1);
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

void hud() {
  header(nullptr);
  const HeroStats &h = heroes[hero];
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawString(h.name, 64, 4, 1);
  for (int i = 0; i < h.health; ++i)
    gameFrame.fillRect(64 + i * 9, 15, 7, 9, i < player.hp ? rgb(255,36,85) : rgb(73,36,36));
  if (boss.active) {
    gameFrame.setTextColor(level ? rgb(255,109,109) : rgb(146,255,255));
    gameFrame.drawRightString(level ? "BROCK" : "DAVY JONES", 314, 4, 1);
    const int x0 = 316 - bossMaxHp() * 9;
    for (int i = 0; i < bossMaxHp(); ++i)
      gameFrame.fillRect(x0 + i * 9, 15, 7, 9, i < boss.hp ? (level ? rgb(255,73,36) : rgb(73,219,146)) : (level ? rgb(73,36,36) : rgb(36,73,73)));
  } else {
    char text[24];
    snprintf(text, sizeof(text), "KOs %d", kos);
    gameFrame.setTextColor(rgb(255,219,85));
    gameFrame.drawRightString(level ? "CITY STREETS" : "SHIPWRECK", 314, 4, 1);
    gameFrame.drawRightString(text, 314, 16, 1);
  }
}

void renderLevel(uint32_t now) {
  const int cam = (int)camera + (shake > 0 ? random(-4, 5) : 0);
  if (level) drawCity(cam, now);
  else drawShip(cam, now);
  // Red target circle for the boss's sword slam.
  if (!level && boss.active && boss.charge > 0) {
    const int cx = (int)boss.superX - cam;
    GameGfx::darkenEllipse(cx, floorY, (int)superRadius, 11);
    const bool pulse = (now / 120) % 2;
    gameFrame.drawEllipse(cx, floorY, (int)superRadius, 11, rgb(255,0,0));
    gameFrame.drawEllipse(cx, floorY, (int)superRadius - 1, 10, pulse ? rgb(255,73,0) : rgb(255,0,0));
    gameFrame.drawEllipse(cx, floorY, (int)(superRadius * (1 - boss.charge / superWindup)), 5, rgb(255,73,85));
  }
  if (boss.slamFx > 0) {
    const int cx = (int)boss.superX - cam, r = (int)((0.5f - boss.slamFx) * 180) + 10;
    gameFrame.drawEllipse(cx, floorY, r, r / 5 + 2, rgb(255,182,0));
    gameFrame.drawEllipse(cx, floorY, r + 3, r / 5 + 3, TFT_WHITE);
    for (int i = 0; i < 6; ++i)
      gameFrame.fillRect(cx - 40 + i * 16, floorY - 10 - (int)(boss.slamFx * 60 * ((i % 3) + 1)), 4, 4, rgb(182,146,73));
  }
  if (boss.active && (boss.dying <= 0 || (now / 80) % 2)) {
    if (level) drawBrock((int)boss.x - cam, now);
    else drawBoss((int)boss.x - cam);
  }
  for (const Enemy &e : enemies)
    if (e.active && (e.dying <= 0 || (now / 60) % 2)) {
      drawEnemy(e, (int)e.x - cam);
    }
  if (player.immunity <= 0 || (now / 70) % 2) {
    const Pose pose = player.attackAnim > 0 ? (player.attackKind ? KICK : PUNCH) : IDLE;
    drawHero(hero, (int)player.x - cam, floorY, player.facing, player.walk, pose, 1.0f, player.moving);
  }
  for (const Spark &s : sparks) {
    if (s.t <= 0) continue;
    const int x = (int)s.x - cam, y = (int)s.y, r = 4 + (int)((0.15f - s.t) * 60);
    for (int i = 0; i < 8; ++i)
      gameFrame.drawLine(x, y, x + (int)(r * cosf(i * PI / 4)), y + (int)(r * sinf(i * PI / 4)), i % 2 ? TFT_WHITE : rgb(255,219,0));
  }
  hud();
  if (banner > 0) {
    gameFrame.fillRoundRect(30, 40, 260, 24, 8, rgb(0,0,0));
    gameFrame.drawRoundRect(30, 40, 260, 24, 8, rgb(146,0,0));
    gameFrame.setTextColor(TFT_WHITE);
    const char *text = boss.active ? (level ? "BROCK STEPS OUT OF THE ALLEY!" : "DAVY JONES RISES FROM THE DEEP!")
                     : level ? "LEVEL 2: CITY STREETS" : "P0/P1 MOVE  P2 PUNCH  P3 KICK";
    gameFrame.drawCentreString(text, 160, 46, 2);
  } else if (!waveActive && !boss.active && goTimer > 0 && (now / 250) % 2) {
    gameFrame.setTextColor(rgb(255,219,0));
    gameFrame.drawString("GO >", 262, 44, 4);
  }
  if (stage == VICTORY || stage == DEFEAT) {
    GameGfx::darkenRect(0, 32, 320, 208);
    gameFrame.fillRoundRect(26, 62, 268, 146, 12, rgb(36,0,0));
    gameFrame.drawRoundRect(26, 62, 268, 146, 12, rgb(255,36,0));
    gameFrame.setTextColor(stage == VICTORY ? rgb(255,219,0) : rgb(255,73,85));
    gameFrame.drawCentreString(stage == VICTORY ? (level ? "CITY CLEARED!" : "SHIPWRECK CLEARED!") : "KNOCKED OUT", 160, 76, 4);
    char line[40];
    snprintf(line, sizeof(line), "%s  -  %d %s KO'd", heroes[hero].name, kos, level ? "thugs" : "pirates");
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(line, 160, 112, 2);
    if (stage == VICTORY)
      gameFrame.drawCentreString(level ? "Brock runs off. The city is safe!" : "Davy Jones is sent back to the deep.", 160, 132, 2);
    const TouchButton *buttons[2] = {&retryButton, &menuButton};
    const bool next = stage == VICTORY && level + 1 < levelCount;
    const char *labels[2] = {next ? "NEXT LEVEL" : "FIGHT AGAIN", "HOME"};
    for (int i = 0; i < 2; ++i) {
      const TouchButton &b = *buttons[i];
      gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 8, i ? rgb(73,0,73) : rgb(146,0,0));
      gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 8, TFT_WHITE);
      gameFrame.setTextColor(TFT_WHITE);
      gameFrame.drawCentreString(labels[i], b.x + b.w / 2, b.y + 11, 2);
    }
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0, 0);
}

void render() {
  if (stage == SELECT) renderSelect();
  else renderLevel(millis());
}

// ---- Gameplay -------------------------------------------------------------
void spark(float x, float y) {
  for (Spark &s : sparks) if (s.t <= 0) { s = {x, y, 0.15f}; return; }
}

void hurtPlayer(int amount, float from) {
  if (player.immunity > 0 || stage != ACTIVE) return;
  player.hp = max(0, player.hp - amount);
  player.immunity = 1.0f;
  player.x += (player.x < from ? -14 : 14);
  DamageLed::flash(millis());
  GameAudio::play(GameAudio::Hurt);
  if (player.hp == 0) { stage = DEFEAT; GameAudio::play(GameAudio::Lose); }
}

void startFight(int chosen, int lvl) {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  hero = chosen;
  level = lvl;
  stage = ACTIVE;
  player = {60, 1, heroes[hero].health, 0, 0, 0, 0, 0, false};
  for (Enemy &e : enemies) e.active = false;
  for (Spark &s : sparks) s.t = 0;
  boss = {};
  boss.hp = bossMaxHp(); boss.facing = -1;
  camera = shake = goTimer = endTimer = 0;
  banner = 3.5f; kos = 0;
  wave = spawned = 0; waveActive = false; spawnTimer = 0;
  lastFrame = millis();
  pendingPresses = 0;
}

void spawnEnemy() {
  for (Enemy &e : enemies) {
    if (e.active) continue;
    const bool brute = wave >= 2 && spawned % 3 == 2;
    const float x = spawned % 2 ? camera - 18 : camera + 338;
    e = {x, 0, brute ? 6 : 3, (uint8_t)(brute ? 1 : 0), 0, 0, 0, 0, 0, 0, x < player.x ? 1 : -1, true};
    ++spawned;
    return;
  }
}

void hitEnemy(Enemy &e, bool kick) {
  e.hp--;
  e.flash = 0.15f; e.windup = 0;
  e.stun = kick ? 0.5f : 0.3f;
  e.vx = player.facing * (kick ? 220.0f : 90.0f) / (e.type ? 1.6f : 1.0f);
  spark(e.x - player.facing * 6, floorY - (kick ? 28 : 40));
  if (e.hp <= 0) { e.dying = 0.6f; e.vx = player.facing * 200.0f; ++kos; GameAudio::play(GameAudio::Pop); }
}

void hitBoss(bool kick) {
  if (boss.hp <= 0) return;
  boss.hp--;
  boss.flash = 0.15f;
  if (boss.charge <= 0 && boss.dash <= 0) { boss.stun = 0.2f; boss.windup = 0; }
  spark(boss.x - player.facing * 10, floorY - boss.lift - (kick ? 40 : 62));
  if (boss.hp <= 0) { boss.dying = 1.5f; boss.charge = boss.dash = boss.daze = 0; boss.lift = 0; GameAudio::play(GameAudio::Crash); }
}

void attack(bool kick) {
  const HeroStats &h = heroes[hero];
  player.attackKind = kick;
  player.attackAnim = 0.18f;
  player.cooldown = kick ? h.kickCooldown : h.punchCooldown;
  const float reach = kick ? h.kickReach : h.punchReach;
  bool landed = false;
  // Brawler hits connect with every enemy in reach, not just the nearest.
  for (Enemy &e : enemies) {
    if (!e.active || e.dying > 0) continue;
    const float dx = (e.x - player.x) * player.facing;
    if (dx > -6 && dx < reach + (e.type ? 4 : 0)) { hitEnemy(e, kick); landed = true; }
  }
  if (boss.active && boss.dying <= 0 && boss.lift < 20) {
    const float dx = (boss.x - player.x) * player.facing;
    if (dx > -8 && dx < reach + 14) { hitBoss(kick); landed = true; }
  }
  if (landed) GameAudio::play(GameAudio::Hit);
  else GameAudio::play(hero == KNIGHT && !kick ? GameAudio::Sword : GameAudio::Click);
}

void updateEnemies(float dt) {
  for (Enemy &e : enemies) {
    if (!e.active) continue;
    e.moving = false;
    e.flash = max(0.0f, e.flash - dt);
    e.x += e.vx * dt;
    e.vx -= e.vx * min(1.0f, dt * 6);
    if (e.dying > 0) {
      e.dying -= dt;
      if (e.dying <= 0) e.active = false;
      continue;
    }
    if (e.stun > 0) { e.stun -= dt; continue; }
    const float dist = player.x - e.x;
    e.facing = dist >= 0 ? 1 : -1;
    const float reach = e.type ? 32 : 36;
    if (e.windup > 0) {
      e.windup -= dt;
      if (e.windup <= 0) {
        if (fabsf(dist) < reach) hurtPlayer(e.type ? 2 : 1, e.x);
        e.rest = e.type ? 0.9f : 0.7f;
      }
      continue;
    }
    if (e.rest > 0) { e.rest -= dt; continue; }
    if (fabsf(dist) > 28) {
      e.x += e.facing * (e.type ? 34 : 48) * dt;
      e.walk += dt * 10;
      e.moving = true;
    } else e.windup = e.type ? 0.8f : 0.55f;
  }
  // Keep pirates from stacking on one spot.
  for (int i = 0; i < maxEnemies; ++i)
    for (int k = i + 1; k < maxEnemies; ++k) {
      Enemy &a = enemies[i], &b = enemies[k];
      if (!a.active || !b.active || a.dying > 0 || b.dying > 0) continue;
      const float gap = b.x - a.x;
      if (fabsf(gap) < 16) { const float push = (16 - fabsf(gap)) / 2 * (gap >= 0 ? 1 : -1); a.x -= push; b.x += push; }
    }
}

// Brock: heavy punches, and a charge he warns about by flashing red.
void updateBrock(float dt) {
  boss.daze = max(0.0f, boss.daze - dt);
  if (boss.charge > 0) {
    // Flashing red, locked facing the spot the player was in.
    boss.charge -= dt;
    if (boss.charge <= 0) { boss.dash = 3; GameAudio::play(GameAudio::Sword); }
    return;
  }
  if (boss.dash > 0) {
    boss.dash -= dt;
    boss.x += boss.facing * brockDashSpeed * dt;
    boss.walk += dt * 18;
    if (fabsf(player.x - boss.x) < 24) hurtPlayer(2, boss.x - boss.facing * 50);
    const float lo = arenaStart + 24, hi = levelEnd - 24;
    const bool wall = boss.x <= lo || boss.x >= hi;
    if (wall || (boss.x - boss.superX) * boss.facing >= 0 || boss.dash <= 0) {
      boss.x = constrain(boss.x, lo, hi);
      boss.dash = 0;
      // Crashing into a wall leaves him seeing stars: a chance to hit back.
      boss.rest = wall ? 2.0f : 0.9f;
      boss.daze = wall ? 2.0f : 0;
      if (wall) { shake = 0.3f; GameAudio::play(GameAudio::Crash); }
    }
    return;
  }
  const float dist = player.x - boss.x;
  boss.facing = dist >= 0 ? 1 : -1;
  boss.superTimer -= dt;
  if (boss.stun > 0) { boss.stun -= dt; return; }
  if (boss.superTimer <= 0 && boss.windup <= 0 && boss.rest <= 0) {
    boss.charge = brockFlash; boss.superTimer = 6.5f;
    boss.superX = player.x + boss.facing * brockOvershoot;
    GameAudio::play(GameAudio::Warning);
    return;
  }
  if (boss.windup > 0) {
    boss.windup -= dt;
    if (boss.windup <= 0) {
      if (fabsf(dist) < 50) hurtPlayer(1, boss.x);
      boss.rest = 0.6f;
    }
    return;
  }
  if (boss.rest > 0) { boss.rest -= dt; return; }
  if (fabsf(dist) > 40) { boss.x += boss.facing * 36 * dt; boss.walk += dt * 7; boss.moving = true; }
  else boss.windup = 0.6f;
}

void updateBoss(float dt) {
  boss.moving = false;
  boss.flash = max(0.0f, boss.flash - dt);
  boss.slamFx = max(0.0f, boss.slamFx - dt);
  if (boss.dying > 0) {
    boss.dying -= dt;
    if (boss.dying <= 0) { boss.active = false; stage = VICTORY; GameAudio::play(GameAudio::Win); }
    return;
  }
  if (level) { updateBrock(dt); return; }
  const float dist = player.x - boss.x;
  if (boss.charge > 0) {
    // Crouch, then leap in an arc onto the red circle and slam the sword down.
    boss.charge -= dt;
    const float p = 1 - max(0.0f, boss.charge) / superWindup;
    if (p > 0.33f) {
      const float t = (p - 0.33f) / 0.67f;
      boss.x = boss.startX + (boss.superX - boss.startX) * t;
      boss.lift = sinf(t * PI) * 80;
    }
    if (boss.charge <= 0) {
      boss.lift = 0; boss.x = boss.superX;
      boss.slamFx = 0.5f; shake = 0.35f;
      GameAudio::play(GameAudio::Crash);
      if (fabsf(player.x - boss.superX) < superRadius) hurtPlayer(2, boss.superX);
      boss.rest = 0.9f;
    }
    return;
  }
  boss.facing = dist >= 0 ? 1 : -1;
  boss.superTimer -= dt;
  if (boss.stun > 0) { boss.stun -= dt; return; }
  if (boss.superTimer <= 0 && boss.windup <= 0) {
    boss.charge = superWindup; boss.superTimer = 7;
    boss.superX = constrain(player.x, arenaStart + 30, levelEnd - 30);
    boss.startX = boss.x;
    GameAudio::play(GameAudio::Warning);
    return;
  }
  if (boss.windup > 0) {
    boss.windup -= dt;
    if (boss.windup <= 0) {
      if (fabsf(dist) < 54) hurtPlayer(1, boss.x);
      boss.rest = 0.7f;
    }
    return;
  }
  if (boss.rest > 0) { boss.rest -= dt; return; }
  if (fabsf(dist) > 44) { boss.x += boss.facing * 30 * dt; boss.walk += dt * 7; boss.moving = true; }
  else boss.windup = 0.75f;
}

void step(float dt, uint8_t held, uint8_t pressed) {
  if (stage != ACTIVE) {
    // Let the defeated boss finish falling and sparks fade behind the panel.
    for (Spark &s : sparks) s.t = max(0.0f, s.t - dt);
    return;
  }
  banner = max(0.0f, banner - dt);
  goTimer = max(0.0f, goTimer - dt);
  shake = max(0.0f, shake - dt);
  for (Spark &s : sparks) s.t = max(0.0f, s.t - dt);
  player.immunity = max(0.0f, player.immunity - dt);
  player.attackAnim = max(0.0f, player.attackAnim - dt);
  player.cooldown = max(0.0f, player.cooldown - dt);

  const int move = (int)((held & 2) != 0) - (int)((held & 1) != 0);
  player.moving = move != 0;
  if (move) { player.facing = move; player.walk += dt * 12; }
  if (player.attackAnim <= 0) player.x += move * heroes[hero].speed * dt;
  const bool locked = waveActive || boss.active;
  const float minX = locked ? camera + 12 : 12;
  const float maxX = waveActive ? camera + 308 : levelEnd - 20;
  player.x = constrain(player.x, minX, maxX);
  if (player.cooldown <= 0) {
    if (pressed & 4) attack(false);
    else if (pressed & 8) attack(true);
  }

  // Waves lock the camera until every pirate in them is down.
  if (!waveActive && !boss.active && wave < waveCount && player.x >= waveAt[wave]) {
    waveActive = true; spawned = 0; spawnTimer = 0.3f;
  }
  if (waveActive) {
    int alive = 0;
    for (const Enemy &e : enemies) if (e.active) ++alive;
    spawnTimer -= dt;
    if (spawned < waveSize[wave] && alive < maxAlive && spawnTimer <= 0) { spawnEnemy(); spawnTimer = 0.9f; }
    if (spawned >= waveSize[wave] && alive == 0) { waveActive = false; ++wave; goTimer = 2.5f; }
  } else if (!boss.active) {
    camera = constrain(player.x - 130, 0.0f, arenaStart);
  }
  if (!boss.active && wave >= waveCount && player.x >= arenaStart + 40) {
    boss.active = true; boss.x = levelEnd - 60; boss.superTimer = 4; boss.hp = bossMaxHp();
    banner = 2.5f;
    GameAudio::play(GameAudio::Warning);
  }
  if (boss.active) {
    camera += (arenaStart - camera) * min(1.0f, dt * 4);
    updateBoss(dt);
  }
  updateEnemies(dt);
}
} // namespace BlackKnight

void beginBlackKnight() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Win);
  gameState = BLACK_KNIGHT;
  BlackKnight::stage = BlackKnight::SELECT;
  BlackKnight::render();
}

void updateBlackKnight(uint32_t now, bool tapped, uint16_t x, uint16_t y, uint8_t held, uint8_t pressed) {
  using namespace BlackKnight;
  if (tapped && exitButton.contains(x, y)) { drawHomeMenu(); return; }
  if (stage == SELECT) {
    for (int i = 0; i < 3; ++i) if (tapped && heroCards[i].contains(x, y)) startFight(i, 0);
    return;
  }
  if (stage == VICTORY || stage == DEFEAT) {
    if (tapped && retryButton.contains(x, y)) {
      // Win: on to the next level, or back to the first after the last one.
      // Lose: try the same level again.
      startFight(hero, stage == DEFEAT ? level : (level + 1) % levelCount);
      return;
    }
    if (tapped && menuButton.contains(x, y)) { drawHomeMenu(); return; }
  }
  pendingPresses |= pressed;
  if (now - lastFrame < 33) return;
  const float dt = min(0.05f, (now - lastFrame) / 1000.0f);
  lastFrame = now;
  step(dt, held, pendingPresses);
  pendingPresses = 0;
  render();
}
