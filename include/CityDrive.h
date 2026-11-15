#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
#include <math.h>
#ifndef GAME_AUDIO_HOST_TEST
#include <Preferences.h>
#endif

// A small pseudo-3D renderer: world coordinates project toward a fixed horizon.
// Reuses the existing 8-bit framebuffer and calibrated, single-touch input.
namespace CityDrive {
constexpr int horizon = 63;
constexpr float focal = 100.0f;
constexpr float nearZ = 3.0f;
constexpr float cameraHeight = 5.5f;
constexpr int playerBottom = 232;
constexpr float playerZ = cameraHeight*focal/(playerBottom-horizon);
struct Traffic { float x, z, speed; uint16_t color; };
Traffic traffic[6];
float position = 0, playerX = 0, speed = 0, distance = 0;
float invulnerable = 0;
// Road bend: curve eases toward curveTarget; skyShift pans the skyline.
float curve = 0, curveTarget = 0, nextBend = 150, skyShift = 0;
// Police chase: gap is meters between the cop and the player.
constexpr float mph = 2.237f;                // m/s to mph
constexpr float copTriggerSpeed = 75 / mph;  // 75 mph
constexpr float copTriggerStep = 800;        // meters between possible chases
float copGap = 0, nextCopAt = copTriggerStep, sirenTimer = 0;
// Star pickup: a speed boost plus 3 seconds of protection from crashes.
constexpr float starPowerTime = 3.0f;
struct Star { float x, z; bool active; } star = {0, 0, false};
float nextStarAt = 250, starPower = 0;
bool chased = false, busted = false;
// Vehicles chosen in the garage before each drive from the game list.
enum Body { SPORTS, TRUCK, GOLF_CART };
struct Vehicle { const char *name; int health; float acceleration, topSpeed, boostBonus; Body body; };
constexpr int vehicleCount = 3;
const Vehicle vehicles[vehicleCount] = {
  {"TRUCK", 5, 6.0f, 39.0f, 0, TRUCK},            // ~87 mph
  {"SPORTS CAR", 3, 11.0f, 50.0f, 0, SPORTS},     // ~112 mph
  {"GOLF CART", 4, 8.5f, 36.0f, 2, GOLF_CART},    // ~81 mph, stars last 2s longer
};
const TouchButton vehicleCards[vehicleCount] = {{5,38,100,198},{110,38,100,198},{215,38,100,198}};
// The golf cart unlocks after this many police escapes, counted across all drives.
constexpr int golfCartEscapes = 3;
int totalEscapes = 0;
float unlockBanner = 0;
bool unlocked(int i) { return vehicles[i].body != GOLF_CART || totalEscapes >= golfCartEscapes; }
void loadEscapes() {
#ifndef GAME_AUDIO_HOST_TEST
  Preferences prefs;
  if (prefs.begin("city-drive", true)) { totalEscapes = prefs.getUInt("escapes", 0); prefs.end(); }
#endif
}
void saveEscapes() {
#ifndef GAME_AUDIO_HOST_TEST
  Preferences prefs;
  if (prefs.begin("city-drive", false)) { prefs.putUInt("escapes", totalEscapes); prefs.end(); }
#endif
}
int vehicle = 1;
bool choosing = false;
const Vehicle &ride() { return vehicles[vehicle]; }
float topSpeed() { return ride().topSpeed; }
float starTime() { return starPowerTime + ride().boostBonus; }
float boostSpeed() { return ride().topSpeed + 10; }
// Police stay a little slower than the player's top speed, so escaping is possible.
float copSpeed() { return min(40.0f, ride().topSpeed - 3); }
int escapes = 0;
uint32_t lastFrame = 0;
int damage = 0, passed = 0;
bool finished = false;
const TouchButton leftButton = {6, 194, 57, 42};
const TouchButton rightButton = {69, 194, 57, 42};
const TouchButton brakeButton = {190, 194, 59, 42};
const TouchButton gasButton = {255, 194, 59, 42};
const TouchButton exitButton = {5, 4, 52, 26};
const TouchButton retryButton = {90, 133, 140, 38};

// Curvature shifts points sideways in proportion to depth, bending the whole scene.
int projectX(float x, float z) { return 160 + (int)((x - playerX * 0.65f) * focal / z + curve * 0.4f * z); }
int projectY(float z) { return horizon + (int)(cameraHeight * focal / z); }
void quad(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy, uint16_t color) {
  gameFrame.fillTriangle(ax, ay, bx, by, cx, cy, color);
  gameFrame.fillTriangle(ax, ay, cx, cy, dx, dy, color);
}
using GameGfx::rgb;
const uint16_t trafficColors[6]={rgb(219,36,0),rgb(255,219,0),rgb(36,109,255),
                                 rgb(219,219,255),rgb(182,36,170),rgb(255,146,0)};
const uint16_t facadeColors[4]={rgb(109,73,170),rgb(73,109,170),rgb(146,73,85),rgb(73,146,170)};
const uint16_t haze=rgb(219,109,170), windowLit=rgb(255,219,85), windowDark=rgb(36,36,85);
bool braking = false;

void resetTraffic(int i, float z) {
  traffic[i] = {(float)((i % 3 - 1) * 3), z, (float)random(5, 12),
                trafficColors[(i+random(0,6))%6]};
}
// Rear view of a car; detail drops away as it shrinks into the distance.
void car(int x, int bottom, int w, uint16_t color, bool brakes = false, bool player = false) {
  int h = w * 3 / 4;
  if (w < 9) {
    gameFrame.fillRect(x-w/2, bottom-h, w, h, color);
    gameFrame.drawFastHLine(x-w/2, bottom-h/2, w, TFT_RED);
    return;
  }
  const uint16_t shade=GameGfx::darker(color), shine=GameGfx::lighter(color);
  const int wheel=max(2,w/6), cab=bottom-h/2, bumper=max(2,h/8);
  GameGfx::darkenEllipse(x, bottom, w/2+w/6, max(2,w/9));
  // Tires peek out below the body, with a grey tread highlight.
  gameFrame.fillRoundRect(x-w/2, bottom-h/3, wheel, h/3, max(1,wheel/3), TFT_BLACK);
  gameFrame.fillRoundRect(x+w/2-wheel, bottom-h/3, wheel, h/3, max(1,wheel/3), TFT_BLACK);
  if (w >= 20) {
    gameFrame.drawFastVLine(x-w/2+wheel/2, bottom-h/3+2, h/3-4, rgb(109,109,109));
    gameFrame.drawFastVLine(x+w/2-wheel/2-1, bottom-h/3+2, h/3-4, rgb(109,109,109));
  }
  // Cabin: pillars, tinted rear glass with a glint, and a lit roof edge.
  quad(x-w/3, cab+1, x-w/4, bottom-h, x+w/4, bottom-h, x+w/3, cab+1, shade);
  quad(x-w/4+2, cab, x-w/5+1, bottom-h+2, x+w/5-1, bottom-h+2, x+w/4-2, cab, rgb(36,73,109));
  gameFrame.drawLine(x-w/8, cab-1, x-w/16, bottom-h+3, rgb(146,182,255));
  gameFrame.drawLine(x-w/8+2, cab-1, x-w/16+2, bottom-h+3, rgb(73,109,170));
  gameFrame.drawFastHLine(x-w/4+1, bottom-h, w/2-2, shine);
  // Body with a shoulder highlight and wing mirrors.
  quad(x-w/2, bottom-bumper, x-w/2+2, cab, x+w/2-2, cab, x+w/2, bottom-bumper, color);
  gameFrame.drawFastHLine(x-w/2+2, cab, w-4, shine);
  gameFrame.drawFastHLine(x-w/2+3, cab+1, w-6, shine);
  if (w >= 16) {
    gameFrame.fillRect(x-w/2-max(1,w/14), cab-max(1,h/10), max(2,w/10), max(2,h/10), shade);
    gameFrame.fillRect(x+w/2-max(1,w/14), cab-max(1,h/10), max(2,w/10), max(2,h/10), shade);
  }
  // Trunk seam, then a dark bumper.
  gameFrame.drawFastHLine(x-w/3, cab+max(2,h/8), w*2/3, shade);
  gameFrame.fillRect(x-w/2+1, bottom-bumper, w-2, bumper, rgb(36,36,36));
  if (player) {
    // Twin racing stripes and a rear spoiler.
    gameFrame.fillRect(x-4, bottom-h, 3, h-bumper, TFT_WHITE);
    gameFrame.fillRect(x+1, bottom-h, 3, h-bumper, TFT_WHITE);
    gameFrame.fillRect(x-w/2+3, cab-3, w-6, 3, TFT_BLACK);
    gameFrame.fillRect(x-w/3, cab, 2, 3, TFT_BLACK);
    gameFrame.fillRect(x+w/3-2, cab, 2, 3, TFT_BLACK);
  }
  // Wide taillights with a bright core; brakes glow.
  const int lamp=max(3,w/4), lampH=max(2,h/7), lampY=bottom-h/3-1;
  const uint16_t lit = brakes ? rgb(255,73,85) : rgb(182,0,0);
  gameFrame.fillRect(x-w/2+2, lampY, lamp, lampH, lit);
  gameFrame.fillRect(x+w/2-2-lamp, lampY, lamp, lampH, lit);
  if (w >= 20) {
    gameFrame.drawFastHLine(x-w/2+3, lampY+lampH/2, lamp-2, rgb(255,146,170));
    gameFrame.drawFastHLine(x+w/2-1-lamp, lampY+lampH/2, lamp-2, rgb(255,146,170));
  }
  if (brakes) {
    GameGfx::glow(x-w/2+2+lamp/2, lampY+lampH/2, lamp+2, TFT_RED);
    GameGfx::glow(x+w/2-2-lamp/2, lampY+lampH/2, lamp+2, TFT_RED);
  }
  // License plate and an exhaust pipe.
  if (w >= 16) {
    const int pw=max(4,w/4), ph=max(2,h/8);
    gameFrame.fillRect(x-pw/2, bottom-bumper-ph, pw, ph, TFT_WHITE);
    if (ph >= 3) gameFrame.drawFastHLine(x-pw/2+1, bottom-bumper-ph/2-1, pw-2, rgb(36,36,85));
    gameFrame.fillCircle(x-w/3, bottom-bumper/2, max(1,bumper/3), rgb(109,109,109));
  }
}
// Spinning five-point star pickup.
void starShape(int cx, int cy, int r, float spin, uint16_t color) {
  int px[10], py[10];
  for (int i=0;i<10;++i) {
    const float a=spin+i*PI/5, rr=(i%2) ? r*0.45f : r;
    px[i]=cx+(int)(rr*sinf(a)); py[i]=cy-(int)(rr*cosf(a));
  }
  for (int i=0;i<10;++i) gameFrame.fillTriangle(cx,cy,px[i],py[i],px[(i+1)%10],py[(i+1)%10],color);
}
// Distance fades facades toward the dusk haze; nearer floors show lit windows.
void building(float x, float z, int index) {
  const float width = 3.5f, height = 5.0f + (index % 4) * 2;
  int a = projectX(x, z), b = projectX(x + width, z);
  int base = projectY(z), top = base - (int)(height * focal / z);
  int rear = projectX(x < 0 ? x : x+width, z + 4);
  int rearBase = projectY(z + 4), rearTop = rearBase - (int)(height*focal/(z+4));
  int edge = x < 0 ? b : a;
  int fog = constrain((int)(z*1.4f)-14, 0, 80);
  uint16_t face = GameGfx::mix565(facadeColors[(index*5+(x<0))%4], haze, fog, 100);
  quad(edge,top,rear,rearTop,rear,rearBase,edge,base,GameGfx::darker(face));
  gameFrame.fillRect(a, top, max(1,b-a), base-top, face);
  gameFrame.drawFastHLine(a, top, max(1,b-a), GameGfx::lighter(face));
  int floors = (int)height, floorH = (base-top)/floors;
  if (b-a > 9 && floorH >= 3) {
    uint16_t lit = GameGfx::mix565(windowLit, face, fog, 100);
    uint16_t dark = GameGfx::mix565(windowDark, face, fog, 100);
    int cols = (b-a) >= 40 ? 4 : 3, colW = (b-a)/cols;
    int ww = max(1,colW/2), wh = max(1,floorH/2);
    for (int f=1; f<floors; ++f) {
      int wy = top + f*floorH - floorH/2 - wh/2;
      for (int c=0; c<cols; ++c) {
        bool on = ((index*31 + f*7 + c*13) % 5) < 3;
        gameFrame.fillRect(a + c*colW + (colW-ww)/2, wy, ww, wh, on ? lit : dark);
      }
    }
    // Lit shopfront under an awning at street level.
    int shop = max(2,floorH-1);
    gameFrame.fillRect(a+2, base-shop, b-a-4, shop, lit);
    gameFrame.fillRect(a+1, base-shop-2, b-a-2, 2, trafficColors[index%6]);
  } else if (b-a > 3) {
    for (int wy=top+2; wy<base-1; wy+=3) gameFrame.drawFastHLine(a+1, wy, b-a-2, GameGfx::darker(face));
  }
}
// Curbside lamp post whose head throws a soft halo.
void lamp(float x, float z) {
  if (z < nearZ+1) return;
  int px = projectX(x, z), base = projectY(z);
  int top = base - (int)(5.0f*focal/z), arm = (int)(1.0f*focal/z) * (x < 0 ? 1 : -1);
  int thick = max(1, (int)(0.25f*focal/z));
  gameFrame.fillRect(px-thick/2, top, thick, base-top, rgb(36,36,85));
  gameFrame.drawWideLine(px, top, px+arm, top, max(1,thick), rgb(36,36,85));
  GameGfx::glow(px+arm, top+thick, max(2,thick*3), rgb(255,219,170));
  gameFrame.fillCircle(px+arm, top+thick, max(1,thick), rgb(255,255,170));
}
void skyline(int offset, int seed, int minH, int range, uint16_t color, bool windows) {
  for (int i=-1;i<18;++i) {
    int w = 14 + (i*seed)%11, x = i*20 + ((offset%20)+20)%20 - 20;
    int h = minH + (i*seed*7 + seed)%range;
    gameFrame.fillRect(x, horizon-h, w, h, color);
    if (windows)
      for (int wy=horizon-h+3; wy<horizon-2; wy+=4)
        for (int wx=x+2; wx<x+w-2; wx+=4)
          if (((wx-x)*3+wy*5+i*7)%7==0) gameFrame.drawPixel(wx, wy, windowLit);
  }
}
// Rear view of a pickup: tall cab, open bed rails, tailgate and big tires.
void truck(int x, int bottom, int w, uint16_t color, bool brakes = false) {
  const int h = w * 9 / 10;
  const uint16_t shade=GameGfx::darker(color), shine=GameGfx::lighter(color);
  const int wheel=max(3,w/5), bed=bottom-h/2, cabTop=bottom-h;
  GameGfx::darkenEllipse(x, bottom, w/2+w/5, max(2,w/9));
  gameFrame.fillRoundRect(x-w/2-1, bottom-h/3-2, wheel, h/3+2, max(1,wheel/3), TFT_BLACK);
  gameFrame.fillRoundRect(x+w/2-wheel+1, bottom-h/3-2, wheel, h/3+2, max(1,wheel/3), TFT_BLACK);
  // Cab with rear window and amber roof lights.
  gameFrame.fillRoundRect(x-w*3/8, cabTop, w*3/4, h/2+2, max(2,w/12), shade);
  gameFrame.fillRect(x-w*3/8+3, cabTop+3, w*3/4-6, h/4, rgb(36,73,109));
  gameFrame.drawLine(x-w/6, cabTop+h/4+1, x-w/10, cabTop+4, rgb(146,182,255));
  gameFrame.drawFastHLine(x-w*3/8+2, cabTop, w*3/4-4, shine);
  for (int i=-1;i<=1;++i) gameFrame.fillRect(x+i*w/8-1, cabTop-2, 3, 2, rgb(255,182,0));
  // Bed walls and tailgate.
  gameFrame.fillRect(x-w/2, bed, w, h/2-max(2,h/9), color);
  gameFrame.drawFastHLine(x-w/2, bed, w, shine);
  gameFrame.drawFastHLine(x-w/2, bed+1, w, shine);
  gameFrame.drawRect(x-w/3, bed+h/10, w*2/3, h/4, shade);
  gameFrame.fillRect(x-w/2, bottom-max(2,h/9), w, max(2,h/9), rgb(73,73,73));
  // Tall taillights down the corners.
  const uint16_t lit = brakes ? rgb(255,73,85) : rgb(182,0,0);
  const int lw=max(2,w/12);
  gameFrame.fillRect(x-w/2+1, bed+2, lw, h/4, lit);
  gameFrame.fillRect(x+w/2-1-lw, bed+2, lw, h/4, lit);
  if (brakes) {
    GameGfx::glow(x-w/2+1+lw/2, bed+2+h/8, lw+4, TFT_RED);
    GameGfx::glow(x+w/2-1-lw/2, bed+2+h/8, lw+4, TFT_RED);
  }
  if (w >= 16) {
    const int pw=max(4,w/4), ph=max(2,h/9);
    gameFrame.fillRect(x-pw/2, bottom-max(2,h/9)-ph-1, pw, ph, TFT_WHITE);
  }
}
// Rear view of a golf cart: canopy on thin posts, bench seat and a golf bag on the back.
void golfCart(int x, int bottom, int w, bool brakes = false) {
  const int h = w * 9 / 10;
  const uint16_t body=rgb(255,255,255), trim=rgb(182,182,170), canopy=rgb(36,146,85);
  const int wheel=max(2,w/7), roof=bottom-h, deck=bottom-h*2/5;
  GameGfx::darkenEllipse(x, bottom, w/2+w/8, max(2,w/10));
  gameFrame.fillRoundRect(x-w/2+1, bottom-h/4, wheel, h/4, max(1,wheel/3), TFT_BLACK);
  gameFrame.fillRoundRect(x+w/2-wheel-1, bottom-h/4, wheel, h/4, max(1,wheel/3), TFT_BLACK);
  // Canopy roof and its four posts.
  gameFrame.fillRoundRect(x-w/2, roof, w, max(3,h/9), max(1,w/16), canopy);
  gameFrame.drawFastHLine(x-w/2+2, roof, w-4, GameGfx::lighter(canopy));
  gameFrame.fillRect(x-w/2+2, roof, max(1,w/24), deck-roof, trim);
  gameFrame.fillRect(x+w/2-2-max(1,w/24), roof, max(1,w/24), deck-roof, trim);
  // Bench seat back seen through the open frame.
  gameFrame.fillRoundRect(x-w/3, roof+h/4, w*2/3, h/5, max(1,w/16), rgb(146,73,0));
  // Golf bag with club heads poking out.
  const int bagW=max(4,w/5), bagTop=roof+h/4;
  gameFrame.fillRoundRect(x-bagW/2, bagTop, bagW, deck-bagTop+2, max(1,bagW/4), rgb(36,73,170));
  for (int c=-1;c<=1;++c) {
    gameFrame.drawFastVLine(x+c*bagW/3, bagTop-h/8, h/8, trim);
    gameFrame.fillRect(x+c*bagW/3-1, bagTop-h/8-1, 3, 2, TFT_WHITE);
  }
  // Rear body, bumper and small round taillights.
  gameFrame.fillRoundRect(x-w/2+1, deck, w-2, bottom-deck-max(2,h/10), max(1,w/12), body);
  gameFrame.drawFastHLine(x-w/2+3, deck+1, w-6, trim);
  gameFrame.fillRect(x-w/2+2, bottom-max(2,h/10), w-4, max(2,h/10), rgb(73,73,73));
  const uint16_t lit = brakes ? rgb(255,73,85) : rgb(182,0,0);
  const int lr=max(1,w/16);
  gameFrame.fillCircle(x-w/2+3+lr, deck+(bottom-deck)/3, lr, lit);
  gameFrame.fillCircle(x+w/2-3-lr, deck+(bottom-deck)/3, lr, lit);
  if (brakes) {
    GameGfx::glow(x-w/2+3+lr, deck+(bottom-deck)/3, lr+4, TFT_RED);
    GameGfx::glow(x+w/2-3-lr, deck+(bottom-deck)/3, lr+4, TFT_RED);
  }
}
void drawVehicle(int index, int x, int bottom, int w, bool brakes) {
  switch (vehicles[index].body) {
    case TRUCK: truck(x, bottom, w+4, rgb(255,146,0), brakes); break;
    case GOLF_CART: golfCart(x, bottom, w, brakes); break;
    default: car(x, bottom, w, rgb(0,219,170), brakes, true); break;
  }
}
void playerVehicle(int x, int bottom, int w, bool brakes) { drawVehicle(vehicle, x, bottom, w, brakes); }
void heart(int x, int y, uint16_t color) {
  gameFrame.fillCircle(x-3, y, 3, color); gameFrame.fillCircle(x+3, y, 3, color);
  gameFrame.fillTriangle(x-6, y+1, x+6, y+1, x, y+7, color);
}
// Rear-view mirror showing the pursuing cop car, which grows as it closes in.
void mirror(bool flash) {
  const int mx=136, my=3, mw=100, mh=25;
  gameFrame.fillRoundRect(mx-2,my-1,mw+4,mh+3,7,rgb(36,36,36));
  gameFrame.fillRoundRect(mx,my,mw,mh,6,rgb(36,36,85));
  quad(mx+mw/2-6,my+6,mx+mw/2+6,my+6,mx+mw-10,my+mh,mx+10,my+mh,rgb(73,73,85));
  const int w=constrain((int)(44-copGap*0.35f),6,40), cx=mx+mw/2, base=my+mh-2;
  const int h=w/2;
  gameFrame.fillRoundRect(cx-w/2,base-h,w,h,max(1,w/8),TFT_WHITE);
  gameFrame.fillRect(cx-w/2,base-h/2,w,h/2,TFT_BLACK);
  gameFrame.fillRect(cx-w/3,base-h+1,w*2/3,max(1,h/3),rgb(36,73,109));
  const int bar=max(2,w/8);
  gameFrame.fillRect(cx-w/4,base-h-bar,w/4,bar,flash ? TFT_RED : rgb(73,0,0));
  gameFrame.fillRect(cx,base-h-bar,w/4,bar,flash ? rgb(0,0,85) : rgb(36,109,255));
  gameFrame.fillRect(cx-w/2+1,base-h/2-1,max(1,w/6),max(1,h/4),rgb(255,255,170));
  gameFrame.fillRect(cx+w/2-1-max(1,w/6),base-h/2-1,max(1,w/6),max(1,h/4),rgb(255,255,170));
  gameFrame.drawRoundRect(mx-2,my-1,mw+4,mh+3,7,rgb(146,146,170));
}
void statBar(int x, int y, const char *label, float value, float most, uint16_t color, int w) {
  gameFrame.setTextColor(rgb(182,182,170));
  gameFrame.drawString(label, x, y, 1);
  gameFrame.drawRect(x, y+10, w, 7, rgb(109,109,170));
  gameFrame.fillRect(x+1, y+11, (int)((w-2)*value/most), 5, color);
}
// Garage: pick the truck or the sports car before driving.
void renderGarage() {
  GameGfx::gradient(0, 0, 320, 240, rgb(36,36,85), rgb(146,73,85));
  gameFrame.fillRect(0, 31, 320, 1, rgb(255,146,85));
  gameFrame.fillRoundRect(5,4,52,23,5,rgb(73,36,109));
  gameFrame.drawRoundRect(5,4,52,23,5,TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawString("BACK",13,8,2);
  gameFrame.setTextColor(rgb(255,219,85));
  gameFrame.drawCentreString("CHOOSE YOUR RIDE",184,8,2);
  for (int i=0;i<vehicleCount;++i) {
    const Vehicle &v=vehicles[i];
    const TouchButton &b=vehicleCards[i];
    const bool locked=!unlocked(i);
    const int cx=b.x+b.w/2, sx=b.x+8, barW=b.w-16;
    gameFrame.fillRoundRect(b.x,b.y,b.w,b.h,10,rgb(0,0,36));
    gameFrame.drawRoundRect(b.x,b.y,b.w,b.h,10,locked ? rgb(73,73,85) : rgb(255,146,85));
    // Little stretch of road under each vehicle.
    gameFrame.fillRect(b.x+5,b.y+44,b.w-10,26,rgb(73,73,85));
    for (int s=0;s<3;++s) gameFrame.fillRect(b.x+10+s*30,b.y+57,16,2,TFT_WHITE);
    drawVehicle(i, cx, b.y+66, 60, false);
    if (locked) {
      // Silhouette with a padlock until the unlock is earned.
      GameGfx::darkenRect(b.x+4,b.y+4,b.w-8,b.h-8);
      GameGfx::darkenRect(b.x+4,b.y+4,b.w-8,b.h-8);
      gameFrame.fillRoundRect(cx-9,b.y+86,18,15,3,rgb(182,182,170));
      gameFrame.drawRoundRect(cx-6,b.y+78,12,14,5,rgb(182,182,170));
      gameFrame.fillRect(cx-1,b.y+91,3,5,rgb(36,36,85));
      gameFrame.setTextColor(TFT_WHITE);
      gameFrame.drawCentreString("LOCKED", cx, b.y+108, 2);
      gameFrame.setTextColor(rgb(182,182,170));
      gameFrame.drawCentreString("ESCAPE THE", cx, b.y+130, 1);
      gameFrame.drawCentreString("POLICE 3 TIMES", cx, b.y+141, 1);
      char progress[12];
      snprintf(progress, sizeof(progress), "%d / %d", min(totalEscapes,(int)golfCartEscapes), golfCartEscapes);
      gameFrame.setTextColor(rgb(255,219,85));
      gameFrame.drawCentreString(progress, cx, b.y+158, 2);
      continue;
    }
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString(v.name, cx, b.y+74, 2);
    gameFrame.setTextColor(rgb(182,182,170));
    gameFrame.drawString("HEALTH", sx, b.y+94, 1);
    for (int h=0;h<v.health;++h) heart(sx+6+h*15, b.y+107, rgb(255,36,85));
    statBar(sx, b.y+118, "ACCELERATION", v.acceleration, 11.0f, rgb(0,255,85), barW);
    char top[16];
    snprintf(top, sizeof(top), "TOP %d MPH", (int)(v.topSpeed*mph+0.5f));
    statBar(sx, b.y+140, top, v.topSpeed, 50.0f, rgb(255,182,0), barW);
    if (v.boostBonus > 0) {
      char boost[20];
      snprintf(boost, sizeof(boost), "STAR BOOST +%dS", (int)v.boostBonus);
      gameFrame.setTextColor(rgb(255,255,170));
      gameFrame.drawCentreString(boost, cx, b.y+164, 1);
    }
    gameFrame.setTextColor(rgb(255,219,85));
    gameFrame.drawCentreString("TAP TO DRIVE", cx, b.y+182, 1);
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}
void render(bool touching, uint16_t tx, uint16_t ty) {
  if (choosing) { renderGarage(); return; }
  GameGfx::gradient(0, 31, 320, horizon-31, rgb(36,36,85), rgb(255,146,85));
  const int sunX = ((int)(250 + skyShift*0.5f) % 400 + 400) % 400 - 40;
  GameGfx::glow(sunX, 54, 24, rgb(255,182,85));
  gameFrame.fillCircle(sunX, 54, 13, rgb(255,219,85));
  for (int y=54; y<horizon; y+=3) gameFrame.drawFastHLine(sunX-14, y, 28, rgb(255,146,85));
  skyline((int)(skyShift-playerX*2), 7, 10, 16, rgb(109,36,109), false);
  skyline((int)(skyShift*2-playerX*4)+9, 5, 5, 12, rgb(36,36,85), true);
  for (int y=horizon; y<240; ++y) {
    float z=cameraHeight*focal/(y-horizon+1);
    int left=projectX(-5.5f,z), right=projectX(5.5f,z);
    int band=(int)(z+position);
    int curb=max(1,(right-left)/24);
    int walkL=projectX(-7.5f,z), walkR=projectX(7.5f,z);
    gameFrame.drawFastHLine(0,y,320,rgb(36,36,85));
    gameFrame.drawFastHLine(walkL,y,walkR-walkL,(band/3)%2 ? rgb(109,109,170) : rgb(146,146,170));
    gameFrame.drawFastHLine(left-curb,y,right-left+2*curb,(band/2)%2 ? rgb(219,36,0) : rgb(219,219,255));
    gameFrame.drawFastHLine(left,y,right-left,rgb(73,73,85));
    if ((band/4)%2) GameGfx::screenSpan(left,right,y,GameGfx::pack(rgb(36,36,85)));
    if ((band/4)%2 == 0) {
      int stripe=max(1,(y-horizon)/45);
      gameFrame.drawFastHLine(projectX(-1.83f,z)-stripe/2,y,stripe,TFT_WHITE);
      gameFrame.drawFastHLine(projectX(1.83f,z)-stripe/2,y,stripe,TFT_WHITE);
    }
  }
  // Haze where the road meets the skyline hides the low-resolution far end.
  // Drawn before the buildings so it never covers their facades.
  GameGfx::screenRect(0, horizon-3, 320, 6, haze);
  for (int i=12;i>=0;--i) {
    float z=nearZ+i*8+8-fmodf(position,8);
    int block=(int)(position/8)+i;
    building(-10,z,block); building(6.5f,z,block+2);
    lamp(-6.0f,z); lamp(6.0f,z);
  }
  if (star.active && star.z>nearZ) {
    const int r=max(3,(int)(1.1f*focal/star.z)), sy=projectY(star.z)-r-2;
    const int sx=projectX(star.x,star.z);
    GameGfx::glow(sx, sy, r*2, rgb(255,255,170));
    starShape(sx, sy, r, millis()/250.0f, rgb(255,219,0));
    starShape(sx, sy, r/2, millis()/250.0f, rgb(255,255,170));
  }
  // Painter's order is essential when cars in the same lane overlap.
  int order[6]={0,1,2,3,4,5};
  for(int i=0;i<5;++i) for(int j=i+1;j<6;++j)
    if(traffic[order[i]].z<traffic[order[j]].z) { int tmp=order[i]; order[i]=order[j]; order[j]=tmp; }
  for(int i:order) {
    const Traffic &c=traffic[i];
    if(c.z>nearZ) car(projectX(c.x,c.z),projectY(c.z),max(3,(int)(1.7f*focal/c.z)),c.color);
  }
  const int playerScreenX=projectX(playerX,playerZ);
  if (starPower>0) GameGfx::glow(playerScreenX, playerBottom-14, 34, (millis()/120)%2 ? rgb(255,219,0) : rgb(255,255,170));
  if(invulnerable<=0 || ((int)(invulnerable*10)%2)==0)
    playerVehicle(playerScreenX,playerBottom,40,braking && !finished);
  if (starPower>0) {
    // Orbiting sparkles show the shield is still up.
    for (int i=0;i<4;++i) {
      const float a=millis()/180.0f+i*PI/2;
      starShape(playerScreenX+(int)(30*cosf(a)), playerBottom-16+(int)(12*sinf(a)), 4, a, rgb(255,255,170));
    }
  }
  if (invulnerable>1.4f) GameGfx::screenRect(0,31,320,209,TFT_RED);
  const bool flash=(millis()/200)%2;
  if (chased && !finished && copGap<40) {
    // Red and blue light washes the screen edges as the cop closes in.
    gameFrame.fillRect(0,31,5,209,flash ? TFT_RED : rgb(36,109,255));
    gameFrame.fillRect(315,31,5,209,flash ? rgb(36,109,255) : TFT_RED);
  }
  GameGfx::gradient(0,0,320,31,rgb(36,36,85),rgb(0,0,0));
  gameFrame.drawFastHLine(0,30,320,rgb(255,146,85));
  gameFrame.fillRoundRect(5,4,52,23,5,rgb(73,36,109));
  gameFrame.drawRoundRect(5,4,52,23,5,TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE,rgb(73,36,109));
  gameFrame.drawString("BACK",13,8,2);
  char hud[64];
  snprintf(hud,sizeof(hud),"%d",(int)(speed*mph));
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawRightString(hud,98,6,2);
  gameFrame.setTextColor(rgb(182,182,170));
  gameFrame.drawString("mph",101,12,1);
  // Speed bar shifts from green to red as the car approaches top speed.
  gameFrame.drawRect(66,22,60,5,rgb(109,109,170));
  int fill=min(58,(int)(speed/topSpeed()*58));
  // Tick at 75 mph marks where speeding draws the police.
  gameFrame.drawFastVLine(67+(int)(copTriggerSpeed/topSpeed()*58),20,9,rgb(255,255,170));
  gameFrame.fillRect(67,23,fill,3,GameGfx::mix565(rgb(0,255,85),rgb(255,36,0),fill,58));
  if (chased && !finished) mirror(flash);
  else {
    snprintf(hud,sizeof(hud),"%.2f mi",distance/1609.34f);
    gameFrame.setTextColor(rgb(255,219,85));
    gameFrame.drawCentreString(hud,186,8,2);
  }
  // Five truck hearts pack tighter than the sports car's three.
  const int hp=ride().health, gap=hp>3 ? 14 : 19;
  for (int i=0;i<hp;++i) heart(314-6-(hp-1-i)*gap,12,i<hp-damage ? rgb(255,36,85) : rgb(73,73,85));
  if (starPower>0) {
    // Star timer under the hearts drains over the 3 seconds.
    starShape(254,25,4,0,rgb(255,219,0));
    gameFrame.fillRect(262,23,(int)(52*starPower/starTime()),4,rgb(255,219,0));
  }
  if (unlockBanner>0 && !finished) {
    gameFrame.fillRoundRect(50,40,220,26,8,rgb(0,0,36));
    gameFrame.drawRoundRect(50,40,220,26,8,rgb(255,219,0));
    gameFrame.setTextColor(rgb(255,219,0));
    gameFrame.drawCentreString("GOLF CART UNLOCKED!",160,45,2);
  }
  if(finished) {
    GameGfx::darkenRect(0,31,320,209);
    gameFrame.fillRoundRect(35,67,250,108,10,rgb(36,36,85));
    gameFrame.drawRoundRect(35,67,250,108,10,rgb(255,146,85));
    gameFrame.drawRoundRect(36,68,248,106,9,rgb(146,73,170));
    gameFrame.setTextColor(busted ? rgb(255,73,85) : COLOR_GOLD,rgb(36,36,85));
    gameFrame.drawCentreString(busted ? "BUSTED!" : "DRIVE COMPLETE",160,77,4);
    gameFrame.setTextColor(TFT_WHITE,rgb(36,36,85));
    snprintf(hud,sizeof(hud),"%.2f mi - %d passed - %d escapes",distance/1609.34f,passed,escapes);
    gameFrame.drawCentreString(hud,160,109,2);
    gameFrame.fillRoundRect(90,133,140,38,6,0x0357);
    gameFrame.drawRoundRect(90,133,140,38,6,rgb(146,219,255));
    gameFrame.setTextColor(TFT_WHITE,0x0357);
    gameFrame.drawCentreString("TRY AGAIN",160,143,2);
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}
} // namespace CityDrive

// Starts a drive with the chosen vehicle; TRY AGAIN reuses it.
void startCityDrive(int chosen) {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  using namespace CityDrive;
  gameState=CITY_DRIVING;
  vehicle=chosen; choosing=false;
  position=playerX=speed=distance=invulnerable=0;
  damage=passed=escapes=0; finished=busted=chased=false;
  curve=curveTarget=skyShift=0; nextBend=150; nextCopAt=copTriggerStep;
  star.active=false; nextStarAt=250; starPower=0; unlockBanner=0;
  for(int i=0;i<6;++i) resetTraffic(i,25+i*15);
  lastFrame=millis();
  render(false,0,0);
}

void beginCityDrive() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  using namespace CityDrive;
  gameState=CITY_DRIVING;
  choosing=true; finished=false; speed=0;
  loadEscapes();
  render(false,0,0);
}

void updateCityDrive(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y, uint8_t buttons = 0) {
  using namespace CityDrive;
  if(tapped && exitButton.contains(x,y)) { drawGameList(); return; }
  if(choosing) {
    for(int i=0;i<vehicleCount;++i)
      if(tapped && vehicleCards[i].contains(x,y)) {
        if(unlocked(i)) startCityDrive(i);
        else GameAudio::play(GameAudio::Click);
      }
    return;
  }
  if(finished) {
    if(tapped && retryButton.contains(x,y)) startCityDrive(vehicle);
    return;
  }
  if(now-lastFrame<33) return;
  float dt=min(0.1f,(now-lastFrame)/1000.0f); lastFrame=now;
  float steering=0, acceleration=-2.0f;
  bool left=buttons & 0x01;
  bool right=buttons & 0x02;
  bool gas=buttons & 0x04;
  bool brake=buttons & 0x08;
  braking=brake;
  steering=(int)right-(int)left;
  if(brake) acceleration=-18.0f;
  // Pull fades toward top speed; star power gives a strong, steady push.
  else if(gas) acceleration=starPower>0 ? ride().acceleration+5 : ride().acceleration*(1-0.6f*speed/topSpeed());
  starPower=max(0.0f,starPower-dt);
  unlockBanner=max(0.0f,unlockBanner-dt);
  // Above the normal top speed after a boost, ease back down instead of snapping.
  const float limit=starPower>0 ? boostSpeed() : topSpeed();
  if(speed>limit) speed=max(limit,speed-8.0f*dt);
  else speed=constrain(speed+acceleration*dt,0.0f,limit);
  // New bends every 120-260 m; about a third of the road stays straight.
  if(distance>=nextBend) {
    const float bends[]={-1.0f,-0.6f,0,0,0.6f,1.0f};
    curveTarget=bends[random(0,6)];
    nextBend=distance+random(120,260);
  }
  curve+=(curveTarget-curve)*min(1.0f,dt*0.8f);
  skyShift-=curve*speed*dt*0.6f;
  // Curves push the car toward the outside; steer into them to stay on the road.
  playerX=constrain(playerX+steering*dt*(2.4f+speed*0.15f)-curve*speed*dt*0.09f,-4.6f,4.6f);
  distance+=speed*dt;
  if(!star.active && distance>=nextStarAt) {
    star={(float)((random(0,3)-1)*3),120,true};
    nextStarAt=distance+random(300,550);
  }
  if(star.active) {
    star.z-=speed*dt;
    if(star.z<5.2f && fabsf(star.x-playerX)<1.8f) {
      star.active=false; starPower=starTime();
      speed=min(boostSpeed(),speed+12.0f);
      GameAudio::play(GameAudio::Happy);
    } else if(star.z<2) star.active=false;
  }
  if(!chased && distance>=nextCopAt && speed>copTriggerSpeed) {
    chased=true; copGap=70; sirenTimer=0;
    GameAudio::play(GameAudio::Siren);
  }
  if(chased) {
    copGap+=(speed-copSpeed())*dt;
    sirenTimer-=dt;
    if(sirenTimer<=0) { GameAudio::play(GameAudio::Siren); sirenTimer=0.7f; }
    if(copGap<=0) { finished=busted=true; GameAudio::play(GameAudio::Lose); }
    else if(copGap>130) {
      chased=false; ++escapes; nextCopAt=distance+copTriggerStep;
      GameAudio::play(GameAudio::Win);
      if(++totalEscapes==golfCartEscapes) unlockBanner=3.0f;
      saveEscapes();
    }
  }
  position=fmodf(position+speed*dt,8000.0f);
  invulnerable=max(0.0f,invulnerable-dt);
  for(int i=0;i<6;++i) {
    Traffic &c=traffic[i];
    float oldZ=c.z;
    c.z+=(c.speed-speed)*dt;
    if(starPower>0 && c.z<5.2f && oldZ>2.0f && fabsf(c.x-playerX)<1.5f) {
      // Star power knocks cars aside without damage.
      GameAudio::play(GameAudio::Pop);
      ++passed; resetTraffic(i,100+i*9);
    } else if(invulnerable<=0 && c.z<5.2f && oldZ>2.0f && fabsf(c.x-playerX)<1.5f) {
      GameAudio::play(GameAudio::Crash);
      DamageLed::flash(now);
      ++damage; speed*=0.3f; invulnerable=1.6f;
      resetTraffic(i,100+i*9);
      if(damage>=ride().health) { finished=true; GameAudio::play(GameAudio::Lose); }
    } else if(c.z<2) { ++passed; resetTraffic(i,100+i*9); }
    else if(c.z>170) resetTraffic(i,100+i*9);
  }
  render(touching,x,y);
}
