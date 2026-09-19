#pragma once
#include <math.h>

// A small pseudo-3D renderer: world coordinates project toward a fixed horizon.
// Reuses the existing 8-bit framebuffer and calibrated, single-touch input.
namespace CityDrive {
constexpr int horizon = 63;
constexpr float focal = 100.0f;
constexpr float nearZ = 3.0f;
struct Traffic { float x, z, speed; uint16_t color; };
Traffic traffic[6];
float position = 0, playerX = 0, speed = 0, distance = 0;
float invulnerable = 0;
uint32_t lastFrame = 0;
int damage = 0, passed = 0;
bool finished = false;
const TouchButton pad = {8, 194, 304, 42};
const TouchButton exitButton = {5, 4, 52, 26};
const TouchButton retryButton = {90, 133, 140, 38};

int projectX(float x, float z) { return 160 + (int)((x - playerX * 0.65f) * focal / z); }
int projectY(float z) { return horizon + (int)(4.0f * focal / z); }
void quad(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy, uint16_t color) {
  gameFrame.fillTriangle(ax, ay, bx, by, cx, cy, color);
  gameFrame.fillTriangle(ax, ay, cx, cy, dx, dy, color);
}
void resetTraffic(int i, float z) {
  traffic[i] = {(float)((i % 3 - 1) * 3), z, (float)random(5, 12),
                i % 2 ? (uint16_t)0xFE20 : (uint16_t)0xB19F};
}
void car(int x, int bottom, int w, uint16_t color) {
  int h = w * 3 / 4;
  gameFrame.fillEllipse(x, bottom, w / 2 + 3, max(2, w / 8), 0x2104);
  gameFrame.fillRect(x - w / 2, bottom - h / 2, max(2, w / 7), h / 2, TFT_BLACK);
  gameFrame.fillRect(x + w / 2 - max(2, w / 7), bottom - h / 2, max(2, w / 7), h / 2, TFT_BLACK);
  quad(x-w/2, bottom-2, x-w/2+2, bottom-h/2, x+w/2-2, bottom-h/2, x+w/2, bottom-2, color);
  quad(x-w/3, bottom-h/2, x-w/4, bottom-h, x+w/4, bottom-h, x+w/3, bottom-h/2, color);
  quad(x-w/4, bottom-h/2-2, x-w/5, bottom-h+3, x+w/5, bottom-h+3, x+w/4, bottom-h/2-2, 0x3250);
  gameFrame.drawFastHLine(x-w/3, bottom-5, w*2/3, TFT_WHITE);
  gameFrame.fillRect(x-w/2+3, bottom-h/3, max(2,w/6), max(2,h/7), TFT_RED);
  gameFrame.fillRect(x+w/2-3-max(2,w/6), bottom-h/3, max(2,w/6), max(2,h/7), TFT_RED);
}
void building(float x, float z, int index) {
  const float width = 3.5f, height = 5.0f + (index % 4) * 2;
  int a = projectX(x, z), b = projectX(x + width, z);
  int base = projectY(z), top = base - (int)(height * focal / z);
  int rear = projectX(x < 0 ? x : x+width, z + 4);
  int rearBase = projectY(z + 4), rearTop = rearBase - (int)(height*focal/(z+4));
  int edge = x < 0 ? b : a;
  quad(edge,top,rear,rearTop,rear,rearBase,edge,base,0x2949);
  gameFrame.fillRect(a, top, max(1,b-a), base-top, index%2 ? 0x52D1 : 0x6474);
  if (b-a > 7) {
    int step = max(4,(b-a)/4);
    for (int wy=max(32,top+4); wy<min(190,base-3); wy+=step+2)
      for (int wx=max(0,a+3); wx<min(320,b-2); wx+=step)
        gameFrame.fillRect(wx,wy,max(1,step/2),2,0xFFE9);
  }
}
void render(bool touching, uint16_t tx, uint16_t ty) {
  gameFrame.fillSprite(0x8E7D);
  gameFrame.fillCircle(257,48,16,0xFFE9);
  for (int i=0;i<20;++i) {
    int h=12+(i*17)%29;
    gameFrame.fillRect(i*17,horizon-h,14,h,0x7474);
  }
  gameFrame.fillRect(0,horizon,320,131,0x7BCF);
  for (int y=horizon+1;y<194;++y) {
    float z=400.0f/(y-horizon);
    int left=projectX(-5.5f,z), right=projectX(5.5f,z);
    gameFrame.drawFastHLine(left-5,y,right-left+10,0xDEDB);
    gameFrame.drawFastHLine(left,y,right-left,0x3186);
    if (((int)(z+position)/4)%2 == 0) {
      int stripe=max(1,(y-horizon)/45);
      gameFrame.drawFastHLine(projectX(-1.83f,z),y,stripe,TFT_WHITE);
      gameFrame.drawFastHLine(projectX(1.83f,z),y,stripe,TFT_WHITE);
    }
  }
  for (int i=12;i>=0;--i) {
    float z=nearZ+i*8+8-fmodf(position,8);
    int block=(int)(position/8)+i;
    building(-10,z,block); building(6.5f,z,block+2);
  }
  // Painter's order is essential when cars in the same lane overlap.
  int order[6]={0,1,2,3,4,5};
  for(int i=0;i<5;++i) for(int j=i+1;j<6;++j)
    if(traffic[order[i]].z<traffic[order[j]].z) { int tmp=order[i]; order[i]=order[j]; order[j]=tmp; }
  for(int i:order) {
    const Traffic &c=traffic[i];
    if(c.z>nearZ) car(projectX(c.x,c.z),projectY(c.z),max(3,(int)(1.7f*focal/c.z)),c.color);
  }
  if(invulnerable<=0 || ((int)(invulnerable*10)%2)==0)
    car(projectX(playerX,nearZ),187,38,0x07F0);
  gameFrame.fillRect(0,0,320,31,0x1084);
  gameFrame.drawRoundRect(5,4,52,26,5,TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE,0x1084);
  gameFrame.drawString("BACK",13,9,2);
  char hud[64];
  snprintf(hud,sizeof(hud),"%d km/h  %dm  %d/3",(int)(speed*3.6f),(int)distance,damage);
  gameFrame.drawRightString(hud,315,9,2);
  gameFrame.fillRoundRect(pad.x,pad.y,pad.w,pad.h,6,0x1928);
  gameFrame.setTextColor(TFT_WHITE,0x1928);
  gameFrame.drawCentreString("< STEER    UP: GAS    STEER >",160,197,2);
  gameFrame.drawCentreString("DOWN: BRAKE / RELEASE: COAST",160,216,1);
  if(touching && pad.contains(tx,ty)) gameFrame.drawCircle(tx,ty,8,COLOR_GOLD);
  if(finished) {
    gameFrame.fillRoundRect(35,67,250,108,10,0x1084);
    gameFrame.setTextColor(COLOR_GOLD,0x1084);
    gameFrame.drawCentreString("DRIVE COMPLETE",160,77,4);
    snprintf(hud,sizeof(hud),"%dm driven - %d cars passed",(int)distance,passed);
    gameFrame.drawCentreString(hud,160,109,2);
    gameFrame.fillRoundRect(90,133,140,38,6,0x0357);
    gameFrame.setTextColor(TFT_WHITE,0x0357);
    gameFrame.drawCentreString("TRY AGAIN",160,143,2);
  }
  gameFrame.pushSprite(0,0);
}
} // namespace CityDrive

void beginCityDrive() {
  using namespace CityDrive;
  gameState=CITY_DRIVING;
  position=playerX=speed=distance=invulnerable=0;
  damage=passed=0; finished=false;
  for(int i=0;i<6;++i) resetTraffic(i,25+i*15);
  lastFrame=millis();
  render(false,0,0);
}

void updateCityDrive(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y) {
  using namespace CityDrive;
  if(tapped && exitButton.contains(x,y)) { drawGameList(); return; }
  if(finished) {
    if(tapped && retryButton.contains(x,y)) beginCityDrive();
    return;
  }
  if(now-lastFrame<33) return;
  float dt=min(0.1f,(now-lastFrame)/1000.0f); lastFrame=now;
  float steering=0, acceleration=-2.0f;
  if(touching && pad.contains(x,y)) {
    steering=constrain(((float)x-160)/115,-1.0f,1.0f);
    acceleration=y<215 ? 9.0f : -18.0f;
  }
  speed=constrain(speed+acceleration*dt,0.0f,30.0f);
  playerX=constrain(playerX+steering*dt*(1.0f+speed*0.12f),-4.6f,4.6f);
  distance+=speed*dt;
  position=fmodf(position+speed*dt,8000.0f);
  invulnerable=max(0.0f,invulnerable-dt);
  for(int i=0;i<6;++i) {
    Traffic &c=traffic[i];
    float oldZ=c.z;
    c.z+=(c.speed-speed)*dt;
    if(invulnerable<=0 && c.z<5.2f && oldZ>2.0f && fabsf(c.x-playerX)<1.5f) {
      ++damage; speed*=0.3f; invulnerable=1.6f;
      resetTraffic(i,100+i*9);
      if(damage>=3) finished=true;
    } else if(c.z<2) { ++passed; resetTraffic(i,100+i*9); }
    else if(c.z>170) resetTraffic(i,100+i*9);
  }
  render(touching,x,y);
}
