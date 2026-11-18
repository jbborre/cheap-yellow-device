#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
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
  float x, vx; int hp; uint8_t type; float windup, rest, stun, flash, dying, walk; int facing; bool active;
} enemies[maxEnemies];

struct Boss {
  float x, startX, lift; int hp, facing; float windup, rest, stun, flash, superTimer, charge, superX, slamFx, dying, walk, dash, daze;
  bool active;
} boss;

int wave = 0, spawned = 0;
bool waveActive = false;
float spawnTimer = 0;

struct Spark { float x, y, t; } sparks[maxSparks];

// ---- Drawing helpers -------------------------------------------------------
struct Look { uint16_t leg, boot, torso, arm, hand, head; };
struct Joints { int headX, headY, shY, hipY, handX, handY, backX, backY, lean; };
enum Pose { IDLE, PUNCH, KICK, WINDUP, HURT, RAISE };

void limb(int ax, int ay, int bx, int by, float w, uint16_t c) { gameFrame.drawWideLine(ax, ay, bx, by, w, c); }

// One articulated side-view figure; scale s=1 is about 57 pixels tall.
Joints humanoid(int x, int f, int d, float s, const Look &lk, float walk, Pose pose, int lift = 0) {
  auto S = [s](float v) { return (int)lroundf(v * s); };
  const int ff = f - lift;
  Joints j;
  j.hipY = ff - S(22); j.shY = ff - S(41); j.headY = ff - S(50);
  j.lean = pose == HURT ? -d * S(4) : 0;
  const float a = sinf(walk) * 7 * s;
  const float lw = max(3.0f, 6 * s);
  GameGfx::darkenEllipse(x, f + 1, S(14), max(2, S(3)));
  // Legs, with a boot at each foot.
  int f1x, f1y, f2x, f2y;
  if (pose == KICK) {
    f1x = x - d * S(6); f1y = ff; f2x = x + d * S(28); f2y = j.hipY - S(3);
    limb(x - d * S(2), j.hipY, f1x, f1y, lw, lk.leg);
    limb(x + d * S(2), j.hipY, f2x, f2y, lw, lk.leg);
  } else {
    f1x = x + (int)a; f1y = ff; f2x = x - (int)a; f2y = ff;
    limb(x - S(1), j.hipY, f1x, f1y, lw, lk.leg);
    limb(x + S(1), j.hipY, f2x, f2y, lw, lk.leg);
  }
  gameFrame.fillRect(f1x - S(3) + d * S(1), f1y - S(3), S(8), max(2, S(3)), lk.boot);
  if (pose == KICK) gameFrame.fillRect(f2x - S(2), f2y - S(3), max(3, S(5)), S(7), lk.boot);
  else gameFrame.fillRect(f2x - S(3) + d * S(1), f2y - S(3), S(8), max(2, S(3)), lk.boot);
  // Back arm behind the torso.
  const int bsx = x - d * S(3) + j.lean, bsy = j.shY + S(3);
  if (pose == RAISE) { j.backX = x - d * S(2); j.backY = j.shY - S(16); }
  else if (pose == PUNCH) { j.backX = x - d * S(9); j.backY = j.shY + S(12); }
  else { j.backX = x - d * S(7) - (int)(a * 0.5f); j.backY = j.shY + S(16); }
  limb(bsx, bsy, j.backX, j.backY, max(2.0f, 5 * s), GameGfx::darker(lk.arm));
  gameFrame.fillCircle(j.backX, j.backY, max(2, S(3)), GameGfx::darker(lk.hand));
  gameFrame.fillRoundRect(x - S(7) + j.lean, j.shY - S(1), S(14), j.hipY - j.shY + S(5), S(4), lk.torso);
  j.headX = x + d * S(1) + j.lean;
  gameFrame.fillCircle(j.headX, j.headY, S(7), lk.head);
  // Front arm.
  const int fsx = x + d * S(2) + j.lean, fsy = j.shY + S(3);
  if (pose == PUNCH) { j.handX = x + d * S(25); j.handY = j.shY + S(3); }
  else if (pose == WINDUP) { j.handX = x - d * S(4); j.handY = j.shY - S(13); }
  else if (pose == RAISE) { j.handX = x + d * S(3); j.handY = j.shY - S(18); }
  else if (pose == HURT) { j.handX = x - d * S(8); j.handY = j.shY + S(12); }
  else { j.handX = x + d * S(7) + (int)(a * 0.5f); j.handY = j.shY + S(16); }
  limb(fsx, fsy, j.handX, j.handY, max(2.0f, 5 * s), lk.arm);
  gameFrame.fillCircle(j.handX, j.handY, max(2, S(3)), lk.hand);
  return j;
}

void starDot(int x, int y, uint16_t c) {
  gameFrame.drawFastHLine(x - 2, y, 5, c);
  gameFrame.drawFastVLine(x, y - 2, 5, c);
}

void hitFlash(int x, int f, float s) {
  GameGfx::screenRect(x - (int)(13 * s), f - (int)(58 * s), (int)(26 * s), (int)(58 * s), TFT_WHITE);
}

void drawHero(int which, int x, int f, int d, float walk, Pose pose, float s = 1.0f) {
  auto S = [s](float v) { return (int)lroundf(v * s); };
  if (which == KNIGHT) {
    const Look lk = {rgb(36,36,36), rgb(0,0,0), rgb(36,36,36), rgb(73,73,73), rgb(109,109,109), rgb(36,36,36)};
    Joints j = humanoid(x, f, d, s, lk, walk, pose);
    // Steel trim, a gold belt and a shoulder plate.
    gameFrame.fillRect(x - S(5) + j.lean, j.shY + S(3), S(10), max(1, S(2)), rgb(109,109,109));
    gameFrame.fillRect(x - S(7) + j.lean, j.hipY - S(1), S(14), max(2, S(2)), rgb(219,182,0));
    gameFrame.fillCircle(x + d * S(1) + j.lean, j.shY + S(3), S(5), rgb(73,73,73));
    // Helmet: glowing visor slit and a dark red plume.
    gameFrame.fillRect(d > 0 ? j.headX : j.headX - S(6), j.headY - S(1), S(6), max(2, S(2)), rgb(255,36,0));
    gameFrame.fillTriangle(j.headX - d * S(1), j.headY - S(7), j.headX - d * S(13), j.headY - S(2),
                           j.headX - d * S(1), j.headY - S(3), rgb(146,0,0));
    // Sword: thrust forward when attacking, angled down at rest.
    const int tipX = pose == PUNCH ? j.handX + d * S(24) : j.handX + d * S(12);
    const int tipY = pose == PUNCH ? j.handY - S(2) : j.handY + S(14);
    limb(j.handX, j.handY, tipX, tipY, max(2.0f, 3 * s), rgb(219,219,255));
    gameFrame.fillRect(j.handX - S(1), j.handY - S(4), max(2, S(3)), S(8), rgb(219,182,0));
    if (pose == PUNCH) {
      for (int i = 0; i < 3; ++i)
        gameFrame.drawLine(j.handX + d * S(6), j.handY - S(14) + i * S(4), tipX + d * S(4), tipY - S(4) + i * S(4), rgb(182,182,255));
    }
  } else if (which == ECLIPSE) {
    const uint16_t purple = rgb(109,36,170), deep = rgb(73,0,109);
    // Starry cape flows behind him.
    const int shY = f - S(41);
    gameFrame.fillTriangle(x - d * S(2), shY, x - d * S(18) - (int)(sinf(walk) * 2), f - S(6), x + d * S(1), f - S(8), rgb(36,0,73));
    for (int i = 0; i < 4; ++i) starDot(x - d * S(5 + i * 3), shY + S(8 + i * 8), rgb(255,255,170));
    const Look lk = {deep, rgb(36,0,73), purple, purple, rgb(146,73,219), purple};
    Joints j = humanoid(x, f, d, s, lk, walk, pose);
    // Star pattern on the body and a crescent moon on the chest.
    const int stars[][2] = {{-3,8},{4,14},{-2,20},{3,3}};
    for (const auto &p : stars) starDot(x + d * S(p[0]) + j.lean, j.shY + S(p[1]), rgb(255,255,170));
    starDot(x - S(1), j.hipY + S(9), rgb(255,255,170));
    gameFrame.fillCircle(x + j.lean, j.shY + S(9), max(2, S(3)), rgb(255,219,0));
    gameFrame.fillCircle(x + d * S(1) + j.lean, j.shY + S(8), max(2, S(3)), purple);
    gameFrame.fillRect(d > 0 ? j.headX + S(1) : j.headX - S(5), j.headY - S(2), S(4), max(2, S(2)), TFT_WHITE);
    starDot(j.headX - d * S(3), j.headY - S(4), rgb(255,255,170));
  } else {
    const uint16_t gi = rgb(36,36,85), skin = rgb(255,219,170);
    const Look lk = {gi, rgb(36,36,36), gi, gi, skin, skin};
    Joints j = humanoid(x, f, d, s, lk, walk, pose);
    // Hair, ponytail, red headband with trailing tails, and a face mask.
    gameFrame.fillRect(j.headX - S(7), j.headY - S(7), S(14), S(5), rgb(36,0,0));
    limb(j.headX - d * S(6), j.headY - S(3), j.headX - d * S(13), j.headY + S(6), max(2.0f, 3 * s), rgb(36,0,0));
    gameFrame.fillRect(j.headX - S(7), j.headY - S(3), S(14), max(2, S(2)), rgb(219,0,0));
    const int wave = (int)(sinf(walk * 1.5f) * 2);
    gameFrame.drawLine(j.headX - d * S(7), j.headY - S(2), j.headX - d * S(15), j.headY - S(4) + wave, rgb(219,0,0));
    gameFrame.drawLine(j.headX - d * S(7), j.headY - S(1), j.headX - d * S(14), j.headY + S(1) - wave, rgb(219,0,0));
    gameFrame.fillRect(j.headX - S(7), j.headY + S(1), S(14), S(6), gi);
    gameFrame.fillRect(d > 0 ? j.headX + S(2) : j.headX - S(4), j.headY - S(1), max(2, S(2)), max(1, S(2)), TFT_BLACK);
    // Red belt and a white spade on the chest.
    gameFrame.fillRect(x - S(7) + j.lean, j.hipY - S(1), S(14), max(2, S(2)), rgb(219,0,0));
    const int cx = x + j.lean, cy = j.shY + S(9);
    gameFrame.fillTriangle(cx, cy - S(4), cx - S(4), cy + S(1), cx + S(4), cy + S(1), TFT_WHITE);
    gameFrame.fillCircle(cx - S(2), cy + S(1), max(1, S(2)), TFT_WHITE);
    gameFrame.fillCircle(cx + S(2), cy + S(1), max(1, S(2)), TFT_WHITE);
    gameFrame.fillRect(cx, cy + S(1), 1, S(4), TFT_WHITE);
  }
}

void drawPirate(const Enemy &e, int x) {
  const bool brute = e.type == 1;
  const float s = brute ? 1.25f : 1.0f;
  auto S = [s](float v) { return (int)lroundf(v * s); };
  const Pose pose = e.dying > 0 || e.stun > 0 ? HURT : e.windup > 0 ? WINDUP : IDLE;
  const uint16_t skin = rgb(219,146,85);
  const Look lk = brute ? Look{rgb(73,36,0), rgb(36,36,36), skin, skin, skin, skin}
                        : Look{rgb(36,36,36), rgb(73,36,0), rgb(255,255,255), rgb(255,255,255), skin, skin};
  Joints j = humanoid(x, floorY, e.facing, s, lk, e.walk, pose);
  if (brute) {
    // Open leather vest, bald head and a bushy beard.
    gameFrame.fillRect(x - S(7) + j.lean, j.shY, S(4), j.hipY - j.shY + S(3), rgb(109,73,0));
    gameFrame.fillRect(x + S(3) + j.lean, j.shY, S(4), j.hipY - j.shY + S(3), rgb(109,73,0));
    gameFrame.fillCircle(j.headX + e.facing * S(2), j.headY + S(5), S(5), rgb(36,36,0));
    gameFrame.fillRect(e.facing > 0 ? j.headX + S(2) : j.headX - S(4), j.headY - S(2), max(2, S(2)), max(2, S(2)), TFT_BLACK);
  } else {
    // Striped shirt, red bandana, eye patch and a cutlass.
    for (int y = j.shY + S(3); y < j.hipY + S(3); y += max(3, S(4)))
      gameFrame.drawFastHLine(x - S(6) + j.lean, y, S(12), rgb(219,0,0));
    gameFrame.fillRect(j.headX - S(7), j.headY - S(7), S(14), S(4), rgb(219,0,0));
    gameFrame.fillTriangle(j.headX - e.facing * S(6), j.headY - S(5), j.headX - e.facing * S(12), j.headY,
                           j.headX - e.facing * S(8), j.headY + S(2), rgb(219,0,0));
    gameFrame.drawLine(j.headX - S(6), j.headY - S(3), j.headX + S(6), j.headY + S(1), TFT_BLACK);
    gameFrame.fillCircle(j.headX + e.facing * S(3), j.headY, max(1, S(2)), TFT_BLACK);
    const int tipX = pose == WINDUP ? j.handX - e.facing * S(4) : j.handX + e.facing * S(16);
    const int tipY = pose == WINDUP ? j.handY - S(16) : j.handY + S(6);
    limb(j.handX, j.handY, tipX, tipY, max(2.0f, 3 * s), rgb(219,219,219));
    gameFrame.fillRect(j.handX - 1, j.handY - 2, 3, 4, rgb(219,182,0));
  }
  if (e.flash > 0) hitFlash(x, floorY, s);
}

void drawBoss(int x) {
  const float s = 1.55f;
  auto S = [s](float v) { return (int)lroundf(v * s); };
  const Pose pose = boss.dying > 0 ? HURT : boss.charge > 0 ? RAISE : boss.windup > 0 ? WINDUP : IDLE;
  const int d = boss.facing, lift = (int)boss.lift;
  const uint16_t coat = rgb(36,73,73), skin = rgb(73,146,109);
  // Long coat tails behind the legs.
  gameFrame.fillTriangle(x - d * S(3), floorY - lift - S(22), x - d * S(14), floorY - lift - S(4),
                         x + d * S(4), floorY - lift - S(6), GameGfx::darker(coat));
  const Look lk = {rgb(36,36,36), rgb(0,0,0), coat, coat, skin, skin};
  Joints j = humanoid(x, floorY, d, s, lk, boss.walk, pose, lift);
  // Gold buttons and barnacles crusted on the coat.
  for (int i = 0; i < 3; ++i) gameFrame.fillCircle(x + d * S(3) + j.lean, j.shY + S(5 + i * 6), max(1, S(1)), rgb(219,182,0));
  gameFrame.fillCircle(x - d * S(4) + j.lean, j.shY + S(10), max(1, S(2)), rgb(182,182,170));
  gameFrame.fillCircle(x - d * S(2) + j.lean, j.shY + S(16), max(1, S(1)), rgb(182,182,170));
  // Crab claw for a back hand.
  gameFrame.fillTriangle(j.backX, j.backY, j.backX - d * S(7), j.backY + S(6), j.backX + d * S(2), j.backY + S(8), rgb(255,73,0));
  gameFrame.fillTriangle(j.backX, j.backY, j.backX - d * S(6), j.backY - S(4), j.backX + d * S(3), j.backY - S(3), rgb(255,73,0));
  // Wriggling tentacle beard.
  const float t = millis() / 220.0f;
  for (int i = 0; i < 6; ++i) {
    int sx = j.headX - S(5) + i * S(2) + d * S(2), sy = j.headY + S(3);
    for (int k = 0; k < 4; ++k) {
      int nx = sx + (int)(sinf(t + i + k) * S(2)), ny = sy + S(4);
      limb(sx, sy, nx, ny, max(2.0f, 2 * s), k % 2 ? rgb(73,146,109) : rgb(36,109,73));
      sx = nx; sy = ny;
    }
  }
  // Glowing eye and a battered tricorn hat.
  gameFrame.fillCircle(j.headX + d * S(3), j.headY - S(1), max(1, S(1)), rgb(146,255,255));
  gameFrame.fillTriangle(j.headX - S(13), j.headY - S(5), j.headX + S(13), j.headY - S(5), j.headX, j.headY - S(17), rgb(36,36,36));
  gameFrame.fillRect(j.headX - S(14), j.headY - S(6), S(28), max(2, S(3)), rgb(36,36,36));
  gameFrame.drawFastHLine(j.headX - S(14), j.headY - S(6), S(28), rgb(219,182,0));
  // Heavy cutlass held high for the slam, otherwise ready to slash.
  int tipX, tipY;
  if (pose == RAISE) { tipX = j.handX + d * S(4); tipY = j.handY - S(28); }
  else if (pose == WINDUP) { tipX = j.handX - d * S(10); tipY = j.handY - S(22); }
  else { tipX = j.handX + d * S(24); tipY = j.handY + S(4); }
  limb(j.handX, j.handY, tipX, tipY, 5, rgb(182,182,170));
  limb(j.handX, j.handY, tipX, tipY, 2, rgb(255,255,255));
  gameFrame.fillRect(j.handX - 3, j.handY - 4, 6, 8, rgb(219,182,0));
  if (boss.flash > 0) hitFlash(x, floorY - lift, s);
}

// ---- City street level: thugs and Brock ------------------------------------
// Steel crowbar with a hooked claw at the far end.
void crowbar(int hx, int hy, int tx, int ty, float s) {
  const float dx = tx - hx, dy = ty - hy, len = max(1.0f, sqrtf(dx * dx + dy * dy));
  const float ux = dx / len, uy = dy / len;
  limb(hx - (int)(ux * 4 * s), hy - (int)(uy * 4 * s), tx, ty, max(2.0f, 3 * s), rgb(146,146,170));
  // The hook bends back toward the holder.
  const int kx = tx - (int)(ux * 4 * s) - (int)(uy * 5 * s), ky = ty - (int)(uy * 4 * s) + (int)(ux * 5 * s);
  limb(tx, ty, kx, ky, max(2.0f, 3 * s), rgb(146,146,170));
  gameFrame.drawLine(hx, hy, tx, ty, rgb(219,219,255));
}

void drawThug(const Enemy &e, int x) {
  const bool big = e.type == 1;
  const float s = big ? 1.3f : 1.0f;
  auto S = [s](float v) { return (int)lroundf(v * s); };
  const Pose pose = e.dying > 0 || e.stun > 0 ? HURT : e.windup > 0 ? WINDUP : IDLE;
  const int d = e.facing;
  const uint16_t skin = rgb(219,146,109);
  if (big) {
    // Muscle shirt, cargo pants, buzz cut and bare fists.
    const Look lk = {rgb(73,73,36), rgb(36,36,36), rgb(219,219,219), skin, skin, skin};
    Joints j = humanoid(x, floorY, d, s, lk, e.walk, pose);
    gameFrame.fillRect(x - S(7) + j.lean, j.hipY - S(1), S(14), max(2, S(2)), rgb(36,36,36));
    gameFrame.fillRect(j.headX - S(7), j.headY - S(7), S(14), S(3), rgb(73,36,0));
    gameFrame.fillRect(j.headX - S(4) + d * S(1), j.headY + S(3), S(9), S(3), rgb(146,73,36));  // Stubble.
    gameFrame.fillRect(d > 0 ? j.headX + S(2) : j.headX - S(4), j.headY - S(2), max(2, S(2)), max(1, S(1)), TFT_BLACK);
    gameFrame.drawLine(j.headX + d * S(1), j.headY - S(4), j.headX + d * S(6), j.headY - S(2), TFT_BLACK);  // Scowl.
    // Tattoo on the front arm and oversized fists.
    const int sx = x + d * S(2) + j.lean, sy = j.shY + S(3);
    gameFrame.fillCircle((sx + j.handX) / 2, (sy + j.handY) / 2, max(1, S(2)), rgb(36,73,146));
    gameFrame.fillCircle(j.handX, j.handY, S(4), skin);
    gameFrame.fillCircle(j.backX, j.backY, S(4), GameGfx::darker(skin));
  } else {
    // Hoodie, jeans and a beanie, swinging a crowbar.
    const uint16_t hoodie = rgb(73,109,73);
    const Look lk = {rgb(36,73,146), rgb(36,36,36), hoodie, hoodie, skin, skin};
    Joints j = humanoid(x, floorY, d, s, lk, e.walk, pose);
    gameFrame.fillRect(j.headX - S(6), j.headY + S(5), S(12), S(3), GameGfx::darker(hoodie));  // Hood at the neck.
    gameFrame.fillRect(x - S(4) + j.lean, j.shY + S(13), S(8), S(4), GameGfx::darker(hoodie));  // Front pocket.
    gameFrame.fillRect(j.headX - S(7), j.headY - S(8), S(14), S(5), rgb(36,36,36));
    gameFrame.drawFastHLine(j.headX - S(7), j.headY - S(4), S(14), rgb(109,109,109));
    gameFrame.fillRect(d > 0 ? j.headX + S(2) : j.headX - S(4), j.headY - S(1), max(2, S(2)), max(1, S(2)), TFT_BLACK);
    const int tipX = pose == WINDUP ? j.handX - d * S(6) : j.handX + d * S(15);
    const int tipY = pose == WINDUP ? j.handY - S(17) : j.handY + S(9);
    crowbar(j.handX, j.handY, tipX, tipY, s);
  }
  if (e.flash > 0) hitFlash(x, floorY, s);
}

void drawBrock(int x, uint32_t now) {
  const float s = 1.55f;
  auto S = [s](float v) { return (int)lroundf(v * s); };
  const bool dashing = boss.dash > 0;
  const Pose pose = boss.dying > 0 ? HURT : dashing ? PUNCH : boss.charge > 0 || boss.windup > 0 ? WINDUP : IDLE;
  const int d = boss.facing;
  const uint16_t skin = rgb(219,146,109), red = rgb(219,0,0);
  if (dashing) {
    // Speed lines and kicked-up dust behind the charge.
    for (int i = 0; i < 4; ++i)
      gameFrame.drawFastHLine(x - d * S(24 + i * 5) - (d > 0 ? S(16) : 0), floorY - S(14 + i * 11), S(16), rgb(219,219,219));
    for (int i = 0; i < 3; ++i)
      gameFrame.fillCircle(x - d * S(14 + i * 9), floorY - S(2 + i), S(4 - i), rgb(146,146,146));
  }
  const Look lk = {rgb(36,73,146), rgb(73,36,0), rgb(219,219,219), skin, skin, skin};
  Joints j = humanoid(x, floorY, d, s, lk, boss.walk, pose);
  // Black leather vest over a white tank, and a belt with a big gold buckle.
  const int vh = j.hipY - j.shY + S(5);
  gameFrame.fillRect(x - S(7) + j.lean, j.shY - S(1), S(5), vh, rgb(36,36,36));
  gameFrame.fillRect(x + S(2) + j.lean, j.shY - S(1), S(5), vh, rgb(36,36,36));
  gameFrame.fillRect(x - S(7) + j.lean, j.hipY - S(1), S(14), max(2, S(2)), rgb(73,36,0));
  gameFrame.fillRect(x - S(2) + j.lean, j.hipY - S(2), S(4), S(3), rgb(219,182,0));
  // Beard, a scowl and big fists.
  gameFrame.fillRect(j.headX - S(5) + d * S(1), j.headY + S(2), S(10), S(5), rgb(73,36,0));
  gameFrame.fillRect(d > 0 ? j.headX + S(2) : j.headX - S(4), j.headY - S(1), max(2, S(2)), max(1, S(1)), TFT_BLACK);
  gameFrame.drawLine(j.headX + d * S(1), j.headY - S(3), j.headX + d * S(6), j.headY - S(1), TFT_BLACK);
  gameFrame.fillCircle(j.handX, j.handY, S(4), skin);
  gameFrame.fillCircle(j.backX, j.backY, S(4), GameGfx::darker(skin));
  // Red bandana with white dots, its knot tails flapping behind his head.
  gameFrame.fillRoundRect(j.headX - S(7), j.headY - S(8), S(14), S(6), S(3), red);
  gameFrame.drawFastHLine(j.headX - S(7), j.headY - S(3), S(14), rgb(146,0,0));
  for (int i = -1; i <= 1; ++i) gameFrame.fillRect(j.headX + i * S(4), j.headY - S(6), 2, 2, TFT_WHITE);
  const int flap = (int)(sinf(now / (dashing ? 40.0f : 110.0f)) * S(2));
  gameFrame.fillTriangle(j.headX - d * S(6), j.headY - S(5), j.headX - d * S(15), j.headY - S(8) + flap,
                         j.headX - d * S(14), j.headY - S(3) + flap, red);
  gameFrame.fillTriangle(j.headX - d * S(6), j.headY - S(4), j.headX - d * S(13), j.headY + S(1) - flap,
                         j.headX - d * S(10), j.headY + S(3) - flap, red);
  // Stars circling his head after he slams into a wall.
  if (boss.daze > 0)
    for (int i = 0; i < 3; ++i) {
      const float a = now / 150.0f + i * 2.09f;
      starDot(j.headX + (int)(cosf(a) * S(11)), j.headY - S(13) + (int)(sinf(a) * S(3)), rgb(255,255,0));
    }
  // The tell for his charge: he flashes red.
  if (boss.charge > 0 && (now / 100) % 2) {
    GameGfx::glow(x, floorY - S(30), S(40), rgb(255,0,0));
    GameGfx::screenRect(x - S(14), floorY - S(60), S(28), S(60), rgb(255,0,0));
  }
  if (boss.flash > 0) hitFlash(x, floorY, s);
}

// ---- Run-down city backdrop ---------------------------------------------------
uint32_t cityHash(int a, int b) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

void cityWindow(int x, int y, int w, int h, uint32_t kind, uint32_t now) {
  const uint16_t glass = rgb(73,109,146);
  gameFrame.fillRect(x - 1, y - 1, w + 2, h + 2, rgb(36,36,36));
  switch (kind % 8) {
    case 0: case 1:  // Dark but whole.
      gameFrame.fillRect(x, y, w, h, rgb(36,36,73));
      gameFrame.drawLine(x + 2, y + h - 4, x + w - 4, y + 2, glass);
      break;
    case 2: case 3:  // Smashed out, with shards left in the frame.
      gameFrame.fillRect(x, y, w, h, rgb(0,0,0));
      gameFrame.fillTriangle(x, y, x + w * 2 / 3, y, x, y + h / 2, glass);
      gameFrame.fillTriangle(x + w - 1, y + h - 1, x + w / 3, y + h - 1, x + w - 1, y + h / 3, glass);
      gameFrame.drawLine(x + w * 2 / 3, y, x, y + h / 2, rgb(182,219,255));
      break;
    case 4:  // Boarded up.
      gameFrame.fillRect(x, y, w, h, rgb(0,0,0));
      limb(x, y + 3, x + w - 1, y + h - 4, 4, rgb(146,109,73));
      limb(x, y + h - 4, x + w - 1, y + 3, 4, rgb(182,146,73));
      gameFrame.drawPixel(x + 2, y + 4, rgb(36,36,36));
      gameFrame.drawPixel(x + w - 3, y + 4, rgb(36,36,36));
      break;
    case 5: {  // Cracked like a spider web.
      gameFrame.fillRect(x, y, w, h, rgb(36,36,73));
      const int cx = x + w / 3, cy = y + h / 3;
      gameFrame.drawLine(cx, cy, x, y + h - 1, rgb(182,182,219));
      gameFrame.drawLine(cx, cy, x + w - 1, y + h / 2, rgb(182,182,219));
      gameFrame.drawLine(cx, cy, x + w / 2, y, rgb(182,182,219));
      gameFrame.drawLine(cx, cy, x, y + 2, rgb(182,182,219));
      break;
    }
    case 6: {  // Someone's still home; the bulb flickers.
      const bool off = (now / 90 + kind) % 31 == 0;
      gameFrame.fillRect(x, y, w, h, off ? rgb(73,73,36) : rgb(255,219,109));
      gameFrame.fillRect(x, y, w / 2, h, off ? rgb(73,36,36) : rgb(219,146,73));  // Torn curtain.
      break;
    }
    default:  // Empty frame, the glass long gone.
      gameFrame.fillRect(x, y, w, h, rgb(0,0,0));
      gameFrame.drawFastHLine(x, y + h / 2, w, rgb(73,73,73));
  }
  gameFrame.drawFastHLine(x - 2, y + h + 1, w + 4, rgb(146,146,146));  // Sill.
}

constexpr int streetTop = 166;
void building(int x, int w, int top, int idx, uint32_t now) {
  const uint16_t walls[3] = {rgb(146,73,36), rgb(109,73,73), rgb(109,109,109)};
  const uint16_t c = walls[idx % 3], mortar = GameGfx::darker(c);
  gameFrame.fillRect(x, top, w, streetTop - top, c);
  if (idx % 3 != 2) {
    // Brick courses with staggered joints.
    for (int y = top + 5, row = 0; y < streetTop; y += 6, ++row) {
      gameFrame.drawFastHLine(x, y, w, mortar);
      for (int bx = x + (row % 2) * 6; bx < x + w; bx += 12) gameFrame.drawFastVLine(bx, y - 5, 5, mortar);
    }
  } else {
    // Stained concrete.
    for (int i = 0; i < 4; ++i) gameFrame.fillRect(x + 10 + i * w / 4, top + 6, 3, 30 + (int)(cityHash(idx, i) % 40), mortar);
  }
  // Crumbling roof ledge with a chunk missing.
  const int gap = x + 20 + (int)(cityHash(idx, 9) % (uint32_t)max(1, w - 50));
  gameFrame.fillRect(x - 2, top - 4, gap - x + 2, 5, mortar);
  gameFrame.fillRect(gap + 14, top - 4, x + w + 2 - gap - 14, 5, mortar);
  gameFrame.fillTriangle(gap, top, gap + 14, top, gap + 7, top + 9, rgb(36,0,73));
  // Windows above the ground floor.
  const int cols = (w - 12) / 26;
  const int x0 = x + (w - (cols * 26 - 10)) / 2;
  for (int r = 0; top + 12 + r * 30 + 20 < streetTop - 34; ++r)
    for (int col = 0; col < cols; ++col)
      cityWindow(x0 + col * 26, top + 12 + r * 30, 16, 20, cityHash(idx * 8 + col, r), now);
  // A long crack running down the wall.
  int cx = x + w / 4 + (int)(cityHash(idx, 3) % (uint32_t)(w / 2)), cy = top + 8;
  for (int i = 0; i < 5; ++i) {
    const int nx = cx + (i % 2 ? 4 : -3), ny = cy + 9;
    gameFrame.drawLine(cx, cy, nx, ny, rgb(36,36,36));
    cx = nx; cy = ny;
  }
  // Ground floor: a dented roll-down shutter with graffiti.
  const int sx = x + w / 2 - 22;
  gameFrame.fillRect(sx, streetTop - 30, 44, 30, rgb(146,146,146));
  for (int y = streetTop - 27; y < streetTop; y += 4) gameFrame.drawFastHLine(sx, y, 44, rgb(109,109,109));
  gameFrame.drawRect(sx - 2, streetTop - 32, 48, 32, rgb(73,73,73));
  const uint16_t paint[3] = {rgb(255,73,182), rgb(73,219,255), rgb(146,255,73)};
  const uint16_t tag = paint[idx % 3];
  for (int i = 0; i < 4; ++i)
    gameFrame.drawLine(sx + 6 + i * 9, streetTop - 12 - (i % 2) * 8, sx + 12 + i * 9, streetTop - 20 + (i % 2) * 8, tag);
  // Rusty fire escape on every other building.
  if (idx % 2) {
    const int fx = x + w - 22;
    for (int y = top + 34; y < streetTop - 40; y += 30) {
      gameFrame.fillRect(fx - 4, y, 26, 2, rgb(73,36,36));
      gameFrame.drawFastVLine(fx - 4, y - 10, 10, rgb(73,36,36));
      gameFrame.drawFastVLine(fx + 21, y - 10, 10, rgb(73,36,36));
      gameFrame.drawLine(fx, y + 2, fx + 16, y + 28, rgb(73,36,36));
    }
  }
}

void trashCan(int x, int y) {
  gameFrame.fillRoundRect(x - 9, y - 22, 18, 22, 3, rgb(109,109,109));
  for (int i = -5; i <= 5; i += 5) gameFrame.drawFastVLine(x + i, y - 20, 18, rgb(73,73,73));
  gameFrame.fillCircle(x + 4, y - 12, 3, rgb(146,146,146));  // Dent.
  limb(x - 12, y - 23, x + 9, y - 29, 3, rgb(146,146,146));  // Lid knocked askew.
  gameFrame.fillEllipse(x + 14, y - 3, 6, 4, rgb(36,36,36));  // Spilled bag.
  gameFrame.fillRect(x - 16, y - 3, 5, 3, rgb(219,219,182));
}

void fireBarrel(int x, int y, uint32_t now) {
  GameGfx::glow(x, y - 30, 26, rgb(255,146,0));
  for (int i = -1; i <= 1; ++i) {
    const int h = 10 + (int)((now / 70 + i * 3) % 6);
    gameFrame.fillTriangle(x + i * 5 - 4, y - 22, x + i * 5 + 4, y - 22, x + i * 5, y - 22 - h, i ? rgb(255,146,0) : rgb(255,219,0));
  }
  gameFrame.fillRoundRect(x - 10, y - 24, 20, 24, 3, rgb(146,73,36));
  gameFrame.drawFastHLine(x - 10, y - 17, 20, rgb(73,36,0));
  gameFrame.drawFastHLine(x - 10, y - 7, 20, rgb(73,36,0));
  gameFrame.fillRect(x - 3, y - 13, 4, 4, rgb(36,0,0));  // Rust hole.
}

void streetLamp(int x, bool broken, uint32_t now) {
  gameFrame.fillRect(x - 1, 92, 3, streetTop + 8 - 92, rgb(36,36,36));
  if (broken) {
    // Bent over and dark.
    limb(x, 92, x - 14, 100, 3, rgb(36,36,36));
    gameFrame.fillRect(x - 20, 99, 10, 5, rgb(73,73,73));
    return;
  }
  limb(x, 92, x + 12, 88, 3, rgb(36,36,36));
  const bool on = (now / 60 + x) % 23 > 3;  // Buzzing, flickering bulb.
  if (on) GameGfx::glow(x + 14, 96, 22, rgb(255,219,109));
  gameFrame.fillRect(x + 9, 89, 10, 5, on ? rgb(255,255,182) : rgb(109,109,73));
}

void drawCity(int cam, uint32_t now) {
  // Smoggy dusk sky.
  GameGfx::gradient(0, 32, 320, streetTop - 32, rgb(36,0,73), rgb(219,109,73));
  const int moonX = 60 - cam / 24;
  gameFrame.fillCircle(moonX, 58, 9, rgb(219,182,146));
  // Far skyline silhouettes drift slowly.
  for (int i = -1; i < 9; ++i) {
    const int world = i + cam / 4 / 44;
    const int bx = world * 44 - cam / 4, h = 40 + (int)(cityHash(world, 1) % 50);
    gameFrame.fillRect(bx, streetTop - 30 - h, 40, h + 30, rgb(73,36,73));
    for (int k = 0; k < 4; ++k) {
      const uint32_t v = cityHash(world, k + 2);
      if (v % 3 == 0) gameFrame.fillRect(bx + 6 + (v / 3) % 28, streetTop - 26 - h + (int)((v / 97) % (uint32_t)h), 2, 2, rgb(255,219,109));
    }
  }
  // Run-down buildings line the street (world layer).
  const int bxs[] = {-40, 110, 270, 410, 560, 720, 870, 1020, 1180, 1330, 1480, 1640};
  const int bws[] = {140, 150, 130, 140, 150, 140, 140, 150, 140, 140, 150, 180};
  const int tops[] = {60, 80, 50, 72, 58, 90, 64, 54, 78, 62, 70, 56};
  for (int i = 0; i < 12; ++i)
    if (bxs[i] + bws[i] - cam > 0 && bxs[i] - cam < 320) building(bxs[i] - cam, bws[i], tops[i], i, now);
  // Brock's name sprayed on the last wall.
  if (cam > arenaStart - 320) {
    gameFrame.setTextColor(rgb(255,36,36));
    gameFrame.drawString("BROCK", 1652 - cam, 122, 4);
  }
  // Cracked sidewalk and curb.
  gameFrame.fillRect(0, streetTop, 320, 12, rgb(146,146,146));
  for (int x = -(cam % 40); x < 320; x += 40) gameFrame.drawFastVLine(x, streetTop, 12, rgb(109,109,109));
  gameFrame.fillRect(0, streetTop + 12, 320, 3, rgb(182,182,182));
  // Asphalt with potholes, cracks and a faded center line.
  GameGfx::gradient(0, streetTop + 15, 320, 240 - streetTop - 15, rgb(73,73,73), rgb(36,36,36));
  for (int x = -(cam % 50); x < 320; x += 50)
    if (((x + cam) / 50) % 3) gameFrame.fillRect(x, 226, 22, 3, rgb(146,146,73));
  const int potholes[] = {380, 820, 1260};
  for (int p : potholes) {
    const int x = p - cam;
    if (x < -30 || x > 350) continue;
    gameFrame.fillEllipse(x, 214, 18, 5, rgb(0,0,0));
    gameFrame.drawEllipse(x, 214, 19, 6, rgb(109,109,109));
  }
  const int cracks[] = {200, 640, 1080, 1560};
  for (int c : cracks) {
    int x = c - cam, y = streetTop + 18;
    if (x < -40 || x > 360) continue;
    for (int i = 0; i < 5; ++i) { const int nx = x + 8, ny = y + (i % 2 ? -3 : 5); gameFrame.drawLine(x, y, nx, ny, rgb(0,0,0)); x = nx; y = ny; }
  }
  // Props sit on the sidewalk behind the fighting line.
  const int cans[] = {330, 780, 1240}, barrels[] = {520, 1000, 1560, 1760}, lamps[] = {240, 690, 1140, 1520};
  for (int i = 0; i < 4; ++i)
    if (lamps[i] - cam > -30 && lamps[i] - cam < 350) streetLamp(lamps[i] - cam, i % 2 == 1, now);
  for (int c : cans) if (c - cam > -30 && c - cam < 350) trashCan(c - cam, streetTop + 12);
  for (int b : barrels) if (b - cam > -40 && b - cam < 360) fireBarrel(b - cam, streetTop + 12, now);
}

// ---- Shipwreck backdrop ----------------------------------------------------
void barrel(int x, int y) {
  gameFrame.fillRoundRect(x - 10, y - 26, 20, 26, 5, rgb(146,73,0));
  gameFrame.drawFastHLine(x - 10, y - 20, 20, rgb(73,36,36));
  gameFrame.drawFastHLine(x - 10, y - 7, 20, rgb(73,36,36));
  gameFrame.drawFastVLine(x - 4, y - 25, 24, rgb(109,36,0));
}
void cannon(int x, int y) {
  gameFrame.fillRoundRect(x - 18, y - 16, 36, 10, 4, rgb(36,36,36));
  gameFrame.fillCircle(x + 18, y - 11, 5, rgb(0,0,0));
  gameFrame.fillCircle(x - 8, y - 4, 6, rgb(109,73,0));
  gameFrame.fillCircle(x + 8, y - 4, 6, rgb(109,73,0));
}
void crate(int x, int y) {
  gameFrame.fillRect(x - 13, y - 24, 26, 24, rgb(182,146,73));
  gameFrame.drawRect(x - 13, y - 24, 26, 24, rgb(109,73,36));
  gameFrame.drawLine(x - 13, y - 24, x + 12, y - 1, rgb(109,73,36));
}
void mast(int x) {
  gameFrame.fillRect(x - 5, 46, 10, 120, rgb(109,73,36));
  gameFrame.fillTriangle(x - 5, 46, x + 5, 46, x - 1, 34, rgb(109,73,36)); // Snapped top.
  gameFrame.fillTriangle(x + 5, 46, x - 1, 34, x + 4, 40, rgb(73,36,0));
  gameFrame.fillRect(x - 40, 64, 80, 5, rgb(73,36,0));
  // Tattered sail with ragged holes.
  gameFrame.fillTriangle(x - 36, 69, x + 36, 69, x + 28, 128, rgb(219,219,182));
  gameFrame.fillTriangle(x - 36, 69, x + 28, 128, x - 30, 118, rgb(219,219,182));
  gameFrame.fillTriangle(x - 12, 92, x + 2, 100, x - 6, 112, rgb(36,73,109));
  gameFrame.fillTriangle(x + 14, 78, x + 22, 84, x + 16, 96, rgb(36,73,109));
  gameFrame.drawLine(x - 36, 69, x - 70, 166, rgb(73,36,0)); // Rigging.
}

void drawShip(int cam, uint32_t now) {
  GameGfx::gradient(0, 32, 320, 110, rgb(0,0,36), rgb(36,73,109));
  const int moonX = 250 - cam / 20;
  GameGfx::glow(moonX, 66, 26, rgb(146,146,170));
  gameFrame.fillCircle(moonX, 66, 13, rgb(255,255,219));
  for (int i = 0; i < 4; ++i) {
    const int cx = ((i * 130 - cam / 8) % 440 + 440) % 440 - 60;
    gameFrame.fillEllipse(cx, 52 + i * 9, 46, 9, rgb(36,36,73));
  }
  // Rolling sea behind the wreck.
  gameFrame.fillRect(0, 130, 320, 26, rgb(0,36,109));
  for (int y = 134; y < 156; y += 6)
    for (int x = -((cam / 3 + (int)(now / 40)) % 32); x < 320; x += 32)
      gameFrame.drawFastHLine(x + (y % 12), y, 12, rgb(73,109,170));
  // Masts and sails (world layer).
  const int masts[] = {150, 700, 1200};
  for (int m : masts) if (m - cam > -80 && m - cam < 400) mast(m - cam);
  // Broken railing along the far side of the deck.
  gameFrame.fillRect(0, 150, 320, 4, rgb(109,73,36));
  for (int x = -(cam % 24); x < 320; x += 24) {
    const int world = x + cam;
    if ((world / 24) % 7 == 3) continue; // Gaps where the railing has snapped.
    gameFrame.fillRect(x, 150, 4, 18, rgb(73,36,0));
  }
  // Deck planks with staggered seams.
  GameGfx::gradient(0, 166, 320, 74, rgb(146,109,73), rgb(73,36,0));
  for (int y = 172; y < 240; y += 11) {
    gameFrame.drawFastHLine(0, y, 320, rgb(73,36,0));
    const int offset = (y / 11 % 2) * 30;
    for (int x = -((cam + offset) % 60); x < 320; x += 60) gameFrame.drawFastVLine(x, y - 10, 10, rgb(73,36,0));
  }
  // Splintered holes in the deck.
  const int holes[] = {420, 910, 1270};
  for (int h : holes) {
    const int x = h - cam;
    if (x < -40 || x > 360) continue;
    gameFrame.fillTriangle(x - 26, 178, x + 24, 176, x + 4, 190, rgb(36,0,0));
    gameFrame.fillTriangle(x - 18, 176, x + 10, 186, x - 8, 192, rgb(36,0,0));
    gameFrame.drawLine(x - 26, 178, x - 32, 172, rgb(182,146,73));
    gameFrame.drawLine(x + 24, 176, x + 30, 182, rgb(182,146,73));
  }
  // Props sit behind the fighting line.
  const int barrels[] = {320, 560, 1050, 1420}, cannons[] = {480, 860}, crates[] = {760, 1150};
  for (int b : barrels) if (b - cam > -20 && b - cam < 340) barrel(b - cam, 186);
  for (int c : cannons) if (c - cam > -30 && c - cam < 350) cannon(c - cam, 186);
  for (int c : crates) if (c - cam > -20 && c - cam < 340) crate(c - cam, 186);
  // Captain's quarters: ship's wheel and lanterns at the stern.
  if (cam > arenaStart - 340) {
    const int wx = 1700 - cam;
    gameFrame.fillRect(wx - 3, 146, 6, 30, rgb(73,36,0));
    gameFrame.drawCircle(wx, 140, 16, rgb(146,73,0));
    gameFrame.drawCircle(wx, 140, 15, rgb(146,73,0));
    for (int i = 0; i < 8; ++i)
      gameFrame.drawLine(wx, 140, wx + (int)(21 * cosf(i * PI / 4)), 140 + (int)(21 * sinf(i * PI / 4)), rgb(146,73,0));
    const int lamps[] = {1520, 1780};
    for (int l : lamps) {
      const int lx = l - cam;
      gameFrame.fillRect(lx - 1, 112, 3, 56, rgb(36,36,36));
      GameGfx::glow(lx, 110, 16, rgb(255,182,0));
      gameFrame.fillRoundRect(lx - 4, 104, 9, 12, 2, rgb(255,219,85));
    }
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
      if (level) drawThug(e, (int)e.x - cam);
      else drawPirate(e, (int)e.x - cam);
    }
  if (player.immunity <= 0 || (now / 70) % 2) {
    const Pose pose = player.attackAnim > 0 ? (player.attackKind ? KICK : PUNCH) : IDLE;
    drawHero(hero, (int)player.x - cam, floorY, player.facing, player.walk, pose);
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
  if (fabsf(dist) > 40) { boss.x += boss.facing * 36 * dt; boss.walk += dt * 7; }
  else boss.windup = 0.6f;
}

void updateBoss(float dt) {
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
  if (fabsf(dist) > 44) { boss.x += boss.facing * 30 * dt; boss.walk += dt * 7; }
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
