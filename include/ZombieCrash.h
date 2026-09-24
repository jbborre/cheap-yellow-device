#pragma once
#include "GameAudio.h"
#include <math.h>

// Fixed-size entity pools and the shared 8-bit sprite keep memory use bounded.
namespace ZombieCrash {
constexpr int ground = 177;
constexpr float levelEnd = 2600.0f;
constexpr float arenaStart = 2180.0f;
constexpr int maxZombies = 16;
constexpr int maxArrows = 5;
constexpr int maxLife = 5;
enum Stage { CHARACTER_SELECT, WEAPON_SELECT, ACTIVE_LEVEL, LEVEL_COMPLETE, VICTORY, DEFEAT };
Stage stage = CHARACTER_SELECT;
bool female = false, bow = false, bossActive = false, pendingAttack = false;
float playerX = 60, camera = 0, bossX = 2500;
float attackCooldown = 0, swing = 0, immunity = 0, spawnTimer = 2;
float summonTimer = 5, summonFlash = 0, walkPhase = 0;
constexpr int swordDamage = 2, bowDamage = 1;
constexpr int maxSpit = 8, maxSpikes = 3;
constexpr float spikeWarning = 1.2f, spikeDuration = 0.7f;
int level = 1;
float animationTime = 0, bossAttack = 0, spikeTimer = 2;
int facing = 1, life = maxLife, bossLife = 10, kills = 0;
uint32_t lastFrame = 0;
struct Zombie {
  float x, rising, flash;
  int hp;
  bool spitter;
  float spitTimer, attackTimer;
};
struct Spit { float x, y, vx, vy; bool active; };
struct Spike { float x, timer; bool active; };
Spit spit[maxSpit] = {};
Spike spikes[maxSpikes] = {};
struct Arrow { float x, remaining; int direction; bool active; };
Zombie zombies[maxZombies] = {};
Arrow arrows[maxArrows] = {};
const TouchButton exitButton = {5, 4, 51, 25};
const TouchButton leftButton = {8, 191, 60, 44};
const TouchButton rightButton = {76, 191, 60, 44};
const TouchButton attackButton = {253, 190, 59, 46};
const TouchButton firstChoice = {18, 78, 136, 101};
const TouchButton secondChoice = {166, 78, 136, 101};
const TouchButton retryButton = {58, 125, 204, 41};
const TouchButton menuButton = {58, 178, 204, 38};

void button(const TouchButton &b, const char *label, bool held = false) {
  uint16_t fill = held ? 0x5B8B : 0x2126;
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 7, fill);
  gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 7, held ? COLOR_GOLD : TFT_WHITE);
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.drawCentreString(label, b.x+b.w/2, b.y+(b.h-16)/2, 2);
}

void spawnZombie(float x) {
  // If a very long boss fight fills the pool, recycle the most distant zombie.
  int slot = 0;
  for (int i=0; i<maxZombies; ++i) {
    if (zombies[i].hp == 0) { slot=i; break; }
    if (fabsf(zombies[i].x-playerX) > fabsf(zombies[slot].x-playerX)) slot=i;
  }
  zombies[slot] = {constrain(x, 15.0f, levelEnd-15), 0.65f, 0, 2, level==2 && random(0,3)!=0, random(900,1800)/1000.0f, 0};
}

void hurtZombie(int i, int damage) {
  if (zombies[i].hp <= 0 || zombies[i].rising > 0) return;
  GameAudio::play(GameAudio::Hit);
  zombies[i].flash = 0.15f;
  zombies[i].hp=max(0,zombies[i].hp-damage);
  if (zombies[i].hp == 0) ++kills;
}

void hurtBoss(int damage) {
  if (!bossActive || bossLife <= 0) return;
  GameAudio::play(GameAudio::Hit);
  bossLife=max(0,bossLife-damage);
  if (bossLife == 0) {
    stage = level==1 ? LEVEL_COMPLETE : VICTORY;
    GameAudio::play(GameAudio::Win);
  }
}

void attack() {
  if (attackCooldown > 0 || stage != ACTIVE_LEVEL) return;
  attackCooldown = bow ? 0.48f : 0.36f;
  GameAudio::play(bow ? GameAudio::Bow : GameAudio::Sword);
  swing = 0.18f;
  if (bow) {
    for (auto &a : arrows) {
      if (!a.active) { a = {playerX + facing*17, 460, facing, true}; break; }
    }
  } else {
    // A swing damages only the nearest enemy in front, once per attack.
    float nearest = 63;
    int target = -1;
    for (int i=0; i<maxZombies; ++i) {
      float dx = (zombies[i].x-playerX)*facing;
      if (zombies[i].hp>0 && zombies[i].rising<=0 && dx>=-8 && dx<nearest) {
        nearest = dx; target = i;
      }
    }
    float bossDx = (bossX-playerX)*facing;
    if (bossActive && bossDx>=-8 && bossDx<nearest+8 && bossDx<71) hurtBoss(swordDamage);
    else if (target>=0) hurtZombie(target,swordDamage);
  }
}

void startLevel(uint32_t now, int selectedLevel = 1) {
  GameAudio::stop();
  GameAudio::play(GameAudio::Start);
  level=selectedLevel;
  stage=ACTIVE_LEVEL; playerX=60; camera=0; facing=1;
  life=maxLife; kills=0; bossLife=10; bossX=2500; bossActive=false;
  attackCooldown=swing=immunity=summonFlash=walkPhase=0;
  spawnTimer=1.6f; summonTimer=5; pendingAttack=false;
  animationTime=bossAttack=0; spikeTimer=2;
  for (auto &p : spit) p.active=false;
  for (auto &p : spikes) p.active=false;
  for (auto &z : zombies) z = {0,0,0,0};
  for (auto &a : arrows) a = {0,0,1,false};
  lastFrame=now;
}

void hurtPlayer() {
  if (immunity>0 || stage!=ACTIVE_LEVEL) return;
  GameAudio::play(GameAudio::Hurt);
  life=max(0,life-1); immunity=1.15f;
  if (life==0) { stage=DEFEAT; GameAudio::play(GameAudio::Lose); }
}

// Wind up, swipe, then recover. A moving player can evade the actual swipe.
bool meleeStrike(float &timer, float distance, float reach, float dt) {
  if (timer<=0) {
    if (distance<reach+12) timer=0.85f;
    return false;
  }
  float before=timer;
  timer=max(0.0f,timer-dt);
  return before>0.25f && timer<=0.25f && distance<reach;
}

void launchSpit(const Zombie &z) {
  for (auto &p : spit) if (!p.active) {
    GameAudio::play(GameAudio::Spit);
    // Aim once at the player's current position, never home in afterwards.
    constexpr float flight=1.1f, gravity=220.0f;
    p={z.x,ground-40.0f,(playerX-z.x)/flight,
       (15.0f-0.5f*gravity*flight*flight)/flight,true};
    return;
  }
}

void raiseSpikeWarnings() {
  GameAudio::play(GameAudio::Warning);
  // Stay on screen and separate markers so there are always safe gaps.
  float left=max(arenaStart+22,camera+22);
  float right=min(levelEnd-22,camera+298);
  for (int i=0;i<maxSpikes;++i) {
    float x=left+(right-left)*(i+0.5f)/maxSpikes+random(-12,13);
    spikes[i]={x,spikeWarning+spikeDuration,true};
  }
}

void updateHazards(float dt) {
  for (auto &p : spit) if (p.active) {
    // Small physics steps avoid tunneling through the hero on slow frames.
    float remaining=dt;
    while (remaining>0 && p.active) {
      float tick=min(remaining,0.02f); remaining-=tick;
      p.x+=p.vx*tick;
      p.y+=p.vy*tick+110.0f*tick*tick; p.vy+=220.0f*tick;
      if (fabsf(p.x-playerX)<13 && p.y>ground-49 && p.y<ground+3) {
        hurtPlayer(); p.active=false;
      } else if (p.y>ground+4 || p.x<0 || p.x>levelEnd) p.active=false;
    }
  }
  for (auto &p : spikes) if (p.active) {
    float before=p.timer;
    p.timer=max(0.0f,p.timer-dt);
    // Warning markers are harmless; only the erupted spikes deal damage.
    if (before>0 && p.timer<=spikeDuration && fabsf(playerX-p.x)<23) hurtPlayer();
    if (p.timer<=0) p.active=false;
  }
}

void step(float dt, int movement, bool attacking) {
  if (stage!=ACTIVE_LEVEL) return;
  animationTime+=dt;
  attackCooldown=max(0.0f,attackCooldown-dt);
  swing=max(0.0f,swing-dt); immunity=max(0.0f,immunity-dt);
  summonFlash=max(0.0f,summonFlash-dt);
  playerX=constrain(playerX+movement*86.0f*dt, bossActive ? arenaStart : 15.0f, levelEnd-20);
  if (movement) { facing=movement; walkPhase+=dt*12; }
  if (!bossActive && playerX>=arenaStart+40) {
    bossActive=true; summonTimer=5; summonFlash=level==1 ? 1.0f : 0; spikeTimer=1.5f; bossAttack=0;
    // The arena starts with the Chieftain; his first reinforcements rise in 5s.
    for (auto &z : zombies) z.hp=0;
    for (auto &a : arrows) a.active=false;
    for (auto &p : spit) p.active=false;
  }
  camera=constrain(playerX-105,0.0f,levelEnd-320);
  if (bossActive) {
    bossX += (playerX>bossX ? 1 : -1)*19*dt;
    bossX=constrain(bossX,arenaStart+15,levelEnd-20);
    if (level==1) {
      summonTimer-=dt;
      if (summonTimer<=0) {
        GameAudio::play(GameAudio::Warning);
        summonTimer+=5; summonFlash=1.0f;
        spawnZombie(playerX-90); spawnZombie(playerX+100);
      }
    } else {
      spikeTimer-=dt;
      if (spikeTimer<=0) { spikeTimer+=3.6f; raiseSpikeWarnings(); }
    }
  } else {
    spawnTimer-=dt;
    if (spawnTimer<=0) {
      spawnTimer=random(1800,3401)/1000.0f;
      float x = random(0,4)==0 && playerX>220 ? camera-25 : camera+345;
      spawnZombie(x);
    }
  }
  for (auto &z : zombies) {
    if (z.hp<=0) continue;
    z.flash=max(0.0f,z.flash-dt);
    if (z.rising>0) { z.rising=max(0.0f,z.rising-dt); continue; }
    float distance=fabsf(playerX-z.x);
    if (!z.spitter || distance>180 || distance<60)
      z.x+=(playerX>z.x ? 1 : -1)*24*dt;
    if (z.spitter) {
      z.spitTimer=max(0.0f,z.spitTimer-dt);
      if (z.spitTimer<=0 && distance>55 && distance<270) {
        launchSpit(z); z.spitTimer=random(2200,3201)/1000.0f;
      }
    }
    if (!bossActive && fabsf(z.x-playerX)>520) z.hp=0;
  }
  if (attacking) attack();
  if (stage!=ACTIVE_LEVEL) return;
  for (auto &a : arrows) {
    if (!a.active) continue;
    float from=a.x;
    float travel=min(a.remaining,280*dt);
    a.x+=a.direction*travel; a.remaining-=travel;
    // Swept collision prevents fast arrows skipping a target between frames.
    int target=-1;
    float closest=travel+1;
    for (int i=0; i<maxZombies; ++i) {
      if (zombies[i].hp<=0 || zombies[i].rising>0) continue;
      float along=(zombies[i].x-from)*a.direction;
      if (along>=-12 && along<=travel+12 && max(0.0f,along-12)<closest) {
        closest=max(0.0f,along-12); target=i;
      }
    }
    float bossAlong=(bossX-from)*a.direction;
    if (bossActive && bossAlong>=-20 && bossAlong<=travel+20 && max(0.0f,bossAlong-20)<closest) {
      hurtBoss(bowDamage); a.active=false;
    } else if (target>=0) { hurtZombie(target,bowDamage); a.active=false; }
    if (a.remaining<=0 || a.x<0 || a.x>levelEnd) a.active=false;
    if (stage!=ACTIVE_LEVEL) return;
  }
  for (auto &z : zombies)
    if (z.hp>0 && z.rising<=0 && meleeStrike(z.attackTimer,fabsf(z.x-playerX),26,dt)) hurtPlayer();
  if (bossActive && meleeStrike(bossAttack,fabsf(bossX-playerX),36,dt)) hurtPlayer();
  updateHazards(dt);
}

void person(int x, int feet, bool isFemale, int direction, bool walking) {
  uint16_t shirt=isFemale ? 0xC99B : 0x249F;
  int stride=walking ? (int)(sinf(walkPhase)*4) : 0;
  gameFrame.fillEllipse(x,feet,13,3,0x3285);
  gameFrame.drawWideLine(x-4,feet-14,x-5-stride,feet,5,0x2128);
  gameFrame.drawWideLine(x+4,feet-14,x+5+stride,feet,5,0x2128);
  gameFrame.fillRoundRect(x-8,feet-33,16,21,3,shirt);
  if (isFemale) gameFrame.fillRoundRect(x-10,feet-49,20,26,5,0x5143);
  gameFrame.fillCircle(x,feet-41,8,0xFED2);
  gameFrame.fillRect(x-8,feet-49,16,5,0x5143);
  gameFrame.fillCircle(x+direction*4,feet-42,1,TFT_BLACK);
  gameFrame.drawWideLine(x+direction*6,feet-29,x+direction*15,feet-24,4,0xFED2);
}

void drawWeapon(int x, int feet, bool useBow, int direction, bool attacking) {
  int hand=x+direction*16, y=feet-25;
  if (useBow) {
    gameFrame.drawLine(hand,y-13,hand+direction*8,y,0xDDA6);
    gameFrame.drawLine(hand+direction*8,y,hand,y+13,0xDDA6);
    gameFrame.drawFastVLine(hand,y-13,27,TFT_WHITE);
    gameFrame.drawLine(hand-direction*7,y,hand+direction*13,y,TFT_WHITE);
  } else {
    int tipX=hand+direction*(attacking ? 38 : 13), tipY=y-(attacking ? 5 : 27);
    gameFrame.drawWideLine(hand,y,tipX,tipY,4,TFT_WHITE);
    gameFrame.drawLine(hand-4,y-4,hand+4,y+4,COLOR_GOLD);
    if (attacking) gameFrame.drawLine(tipX,tipY-9,tipX+direction*3,tipY+10,COLOR_GOLD);
  }
}

void drawZombie(float worldX, bool chief, float rising, int hp, bool flash,
                float attackTimer, bool spitter = false) {
  int x=(int)(worldX-camera);
  if (x<-40 || x>360) return;
  int scale=chief ? 2 : 1;
  int feet=ground+(int)(rising*45);
  uint16_t skin=flash ? TFT_WHITE : level==2 ? 0xAEBF : (chief ? 0x65E5 : 0x8E88);
  gameFrame.fillEllipse(x,ground,chief ? 22 : 13,4,level==2 ? 0x7495 : 0x3285);
  gameFrame.fillRect(x-7*scale,feet-17*scale,5*scale,17*scale,0x39C6);
  gameFrame.fillRect(x+2*scale,feet-17*scale,5*scale,17*scale,0x39C6);
  gameFrame.fillRoundRect(x-9*scale,feet-34*scale,18*scale,20*scale,3,
                         level==2 ? 0x4355 : chief ? 0x8208 : 0x7308);
  gameFrame.fillRoundRect(x-8*scale,feet-49*scale,16*scale,16*scale,3,skin);
  int dir=playerX>worldX ? 1 : -1;
  // Both arms wind back and sweep forward with the melee damage timing.
  float phase=attackTimer>0 ? (0.85f-attackTimer)/0.85f : 0;
  int reach=attackTimer>0 ? (int)(8+15*sinf(phase*3.14159f)) : 13;
  int lift=attackTimer>0 ? (int)(-13*cosf(phase*3.14159f)) : (int)(sinf(animationTime*4+worldX)*2);
  for (int arm=0;arm<2;++arm) {
    int shoulder=x+(arm ? 5 : -5)*scale;
    int y=feet-(arm ? 28 : 31)*scale;
    gameFrame.drawWideLine(shoulder,y,x+dir*reach*scale,y+lift*scale,3*scale,skin);
    gameFrame.fillCircle(x+dir*reach*scale,y+lift*scale,2*scale,skin);
  }
  gameFrame.fillRect(x-5*scale,feet-43*scale,3*scale,3*scale,TFT_RED);
  gameFrame.fillRect(x+3*scale,feet-43*scale,3*scale,3*scale,TFT_RED);
  // Broken skull rim, pink brain lobes and dark folds remain visible on ice zombies.
  gameFrame.fillRect(x-7*scale,feet-50*scale,14*scale,3*scale,0xEF7B);
  gameFrame.fillCircle(x-3*scale,feet-51*scale,4*scale,0xFBB6);
  gameFrame.fillCircle(x+3*scale,feet-51*scale,4*scale,0xFBB6);
  gameFrame.drawLine(x,feet-54*scale,x,feet-48*scale,0xA90D);
  gameFrame.drawLine(x-5*scale,feet-52*scale,x-2*scale,feet-50*scale,0xA90D);
  gameFrame.drawLine(x+2*scale,feet-53*scale,x+5*scale,feet-51*scale,0xA90D);
  if (level==2) {
    gameFrame.fillEllipse(x-7*scale,feet-34*scale,5*scale,2*scale,TFT_WHITE);
    gameFrame.fillEllipse(x+6*scale,feet-34*scale,5*scale,2*scale,TFT_WHITE);
    gameFrame.fillTriangle(x-9*scale,feet-32*scale,x-5*scale,feet-32*scale,x-7*scale,feet-24*scale,0xBFFF);
    gameFrame.fillRect(x+2*scale,feet-7*scale,5*scale,3*scale,TFT_WHITE);
  }
  if (spitter) {
    gameFrame.fillCircle(x+dir*5,feet-37,4,0x07E0);
    gameFrame.fillCircle(x+dir*6,feet-37,2,0x2126);
  }
  if (chief) {
    // Side horns frame the exposed brain instead of covering it.
    for (int side=-1;side<=1;side+=2)
      gameFrame.fillTriangle(x+side*12,feet-98,x+side*24,feet-116,x+side*21,feet-96,
                             level==2 ? 0xBFFF : COLOR_GOLD);
    gameFrame.fillRect(x-15,feet-98,30,3,level==2 ? TFT_WHITE : COLOR_GOLD);
  } else {
    gameFrame.fillRect(x-9,feet-61,18,3,0x2126);
    gameFrame.fillRect(x-9,feet-61,hp*9,3,COLOR_MINT);
  }
}

void drawHazards() {
  for (const auto &p : spikes) if (p.active) {
    int x=(int)(p.x-camera);
    if (p.timer>spikeDuration) {
      gameFrame.fillEllipse(x,ground-1,23,4,TFT_RED);
      gameFrame.drawLine(x-7,ground-10,x+7,ground-3,TFT_WHITE);
      gameFrame.drawLine(x+7,ground-10,x-7,ground-3,TFT_WHITE);
    } else {
      gameFrame.fillEllipse(x,ground,23,4,0x4355);
      int height=(int)(34*min(1.0f,(spikeDuration-p.timer)*12+0.15f));
      for (int i=-1;i<=1;++i) {
        gameFrame.fillTriangle(x+i*13-7,ground,x+i*13,ground-height+(i==0 ? -7 : 0),x+i*13+7,ground,0xBFFF);
        gameFrame.drawLine(x+i*13,ground-height,x+i*13+6,ground,TFT_WHITE);
      }
    }
  }
  for (const auto &p : spit) if (p.active) {
    int x=(int)(p.x-camera), y=(int)p.y;
    gameFrame.drawWideLine(x-p.vx*0.035f,y-p.vy*0.035f,x,y,3,0x5F6A);
    gameFrame.fillCircle(x,y,4,0x07E0);
    gameFrame.fillCircle(x-1,y-1,1,TFT_WHITE);
  }
}

void drawSelection() {
  gameFrame.fillSprite(0x1104);
  button(exitButton,"BACK");
  gameFrame.setTextColor(COLOR_GOLD,0x1104);
  gameFrame.drawCentreString("ZOMBIE CRASH",160,36,4);
  bool choosingCharacter=stage==CHARACTER_SELECT;
  gameFrame.fillRoundRect(18,78,136,101,8,0x2948);
  gameFrame.fillRoundRect(166,78,136,101,8,0x2948);
  if (choosingCharacter) {
    person(86,145,false,1,false); person(234,145,true,-1,false);
  } else {
    drawWeapon(70,147,false,1,true); drawWeapon(220,147,true,1,false);
  }
  gameFrame.setTextColor(TFT_WHITE,0x2948);
  gameFrame.drawCentreString(choosingCharacter ? "MALE" : "SWORD",86,155,2);
  gameFrame.drawCentreString(choosingCharacter ? "FEMALE" : "BOW",234,155,2);
  gameFrame.setTextColor(TFT_WHITE,0x1104);
  gameFrame.drawCentreString(choosingCharacter ? "Choose your hero" : "Choose your weapon",160,186,2);
  gameFrame.drawCentreString("Hold < > to move. Hold A to attack.",160,210,1);
  gameFrame.drawCentreString("Sword: 2 damage. Bow: 1 damage.",160,224,1);
}

void render(bool touching, uint16_t tx, uint16_t ty, int movement) {
  if (stage==CHARACTER_SELECT || stage==WEAPON_SELECT) drawSelection();
  else {
    gameFrame.fillSprite(level==2 ? 0xB73F : 0x967E);
    gameFrame.fillCircle(267-(int)(camera*0.02f),61,17,0xFFE9);
    for(int i=-1;i<5;++i) {
      int x=i*110-(int)(camera*0.18f)%110;
      gameFrame.fillTriangle(x-70,ground,x+45,80,x+140,ground,level==2 ? 0x8D3A : 0x750E);
      gameFrame.fillTriangle(x-30,ground,x+100,112,x+180,ground,level==2 ? 0xBE7D : 0x4C6A);
    }
    for(int i=-1;i<6;++i) {
      int x=i*85-(int)(camera*0.55f)%85;
      gameFrame.fillRect(x,125,5,52,0x82A5);
      if (level==2) {
        gameFrame.fillTriangle(x-19,147,x+2,107,x+23,147,0x4355);
        gameFrame.fillTriangle(x-12,131,x+2,107,x+16,131,TFT_WHITE);
      } else gameFrame.fillCircle(x+2,123,17,0x3487);
    }
    gameFrame.fillRect(0,ground,320,14,level==2 ? 0x7495 : 0x9326);
    gameFrame.fillRect(0,ground,320,5,level==2 ? TFT_WHITE : 0x5E07);
    for(int i=0;i<25;++i) {
      int x=i*15-(int)camera%15;
      gameFrame.drawLine(x,ground,x+3,ground-5,level==2 ? TFT_WHITE : 0x3CA3);
    }
    int signX=(int)(2100-camera);
    if(signX>-50 && signX<340) {
      gameFrame.fillRect(signX,145,4,32,0x82A5);
      gameFrame.fillRect(signX-22,125,58,23,0x2126);
      gameFrame.setTextColor(COLOR_GOLD,0x2126);
      gameFrame.drawString("BOSS >",signX-18,131,1);
    }
    for (const auto &z : zombies) if(z.hp>0) drawZombie(z.x,false,z.rising,z.hp,z.flash>0,z.attackTimer,z.spitter);
    if(bossActive && bossLife>0) drawZombie(bossX,true,0,bossLife,summonFlash>0,bossAttack);
    // Cover the part of rising zombies below the grass surface.
    gameFrame.fillRect(0,ground+1,320,10,level==2 ? 0xBE7D : 0x9326);
    if(immunity<=0 || ((int)(immunity*12)%2)==0) {
      int px=(int)(playerX-camera);
      person(px,ground,female,facing,movement!=0);
      drawWeapon(px,ground,bow,facing,swing>0);
    }
    for(const auto &a : arrows) if(a.active) {
      int x=(int)(a.x-camera), y=ground-25;
      gameFrame.drawLine(x-a.direction*13,y,x,y,TFT_WHITE);
      gameFrame.drawLine(x-a.direction*4,y-3,x,y,TFT_WHITE);
      gameFrame.drawLine(x-a.direction*4,y+3,x,y,TFT_WHITE);
    }
    drawHazards();
    gameFrame.fillRect(0,0,320,33,0x1104);
    button(exitButton,"BACK");
    gameFrame.setTextColor(TFT_WHITE,0x1104);
    gameFrame.drawString("LIFE",63,10,1);
    gameFrame.drawRect(91,8,104,15,TFT_WHITE);
    gameFrame.fillRect(93,10,life*20,11,life>2 ? 0x5E07 : TFT_RED);
    char label[40];
    snprintf(label,sizeof(label),"%d/5  %d%%",life,bossActive ? 100 : (int)(playerX/(arenaStart+40)*100));
    gameFrame.drawRightString(label,314,10,1);
    gameFrame.setTextColor(TFT_WHITE,0x2948);
    gameFrame.fillRect(0,33,320,19,0x2948);
    if(bossActive) {
      gameFrame.drawString(level==2 ? "WARLORD" : "CHIEFTAIN",5,38,1);
      gameFrame.drawRect(66,37,102,10,TFT_WHITE);
      gameFrame.fillRect(67,38,bossLife*10,8,0xE986);
      snprintf(label,sizeof(label),"%d/10 %s:%ds",bossLife,level==2 ? "SPIKE" : "RAISE",(int)ceilf(level==2 ? spikeTimer : summonTimer));
      gameFrame.drawRightString(label,315,38,1);
      if(summonFlash>0) {
        gameFrame.setTextColor(COLOR_GOLD,0x2948);
        gameFrame.drawCentreString("THE DEAD RISE!",160,58,2);
      }
    } else gameFrame.drawCentreString(level==2 ? "LEVEL 2: ICELAND  >>" : "LEVEL 1: GRASSLANDS  >>",160,38,1);
    if (level==2 && animationTime<6) {
      gameFrame.setTextColor(0x1104,0xB73F);
      gameFrame.drawCentreString("Dodge spit. Avoid red spike marks!",160,56,1);
    }
    gameFrame.fillRect(0,189,320,51,0x1104);
    button(leftButton,"<",touching && leftButton.contains(tx,ty));
    button(rightButton,">",touching && rightButton.contains(tx,ty));
    button(attackButton,"A",touching && attackButton.contains(tx,ty));
    gameFrame.setTextColor(TFT_WHITE,0x1104);
    gameFrame.drawCentreString(bow ? "BOW" : "SWORD",193,201,2);
    gameFrame.drawCentreString("HOLD A",193,222,1);
    if(stage==LEVEL_COMPLETE || stage==VICTORY || stage==DEFEAT) {
      gameFrame.fillRoundRect(24,56,272,128,10,0x1104);
      gameFrame.setTextColor(COLOR_GOLD,0x1104);
      gameFrame.drawCentreString(stage==DEFEAT ? "YOU FELL!" : stage==LEVEL_COMPLETE ? "LEVEL COMPLETE!" : "ICELAND CLEARED!",160,69,4);
      snprintf(label,sizeof(label),"%d zombies defeated",kills);
      gameFrame.drawCentreString(label,160,102,2);
      button(retryButton,stage==LEVEL_COMPLETE ? "NEXT: ICELAND" : stage==VICTORY ? "PLAY AGAIN" : "RETRY LEVEL");
      button(menuButton,"CHOOSE HERO / WEAPON");
    }
  }
  gameFrame.pushSprite(0,0);
}
} // namespace ZombieCrash

void beginZombieCrash() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  gameState=ZOMBIE_CRASH;
  ZombieCrash::stage=ZombieCrash::CHARACTER_SELECT;
  ZombieCrash::pendingAttack=false;
  ZombieCrash::render(false,0,0,0);
}

void updateZombieCrash(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y) {
  using namespace ZombieCrash;
  if(tapped && exitButton.contains(x,y)) {
    if(stage==WEAPON_SELECT) { stage=CHARACTER_SELECT; render(false,0,0,0); }
    else drawGameList();
    return;
  }
  if(stage==CHARACTER_SELECT || stage==WEAPON_SELECT) {
    if(!tapped || (!firstChoice.contains(x,y) && !secondChoice.contains(x,y))) return;
    GameAudio::play(GameAudio::Click);
    bool second=secondChoice.contains(x,y);
    if(stage==CHARACTER_SELECT) { female=second; stage=WEAPON_SELECT; }
    else { bow=second; startLevel(now); }
    render(false,0,0,0);
    return;
  }
  if(stage==LEVEL_COMPLETE || stage==VICTORY || stage==DEFEAT) {
    if(tapped && retryButton.contains(x,y)) {
      int nextLevel=stage==LEVEL_COMPLETE ? 2 : stage==VICTORY ? 1 : level;
      startLevel(now,nextLevel); render(false,0,0,0);
    }
    else if(tapped && menuButton.contains(x,y)) beginZombieCrash();
    return;
  }
  // Preserve short A taps that arrive between rendering frames.
  if(tapped && attackButton.contains(x,y)) pendingAttack=true;
  if(now-lastFrame<33) return;
  float dt=min(0.1f,(now-lastFrame)/1000.0f); lastFrame=now;
  int movement=touching && leftButton.contains(x,y) ? -1 :
               touching && rightButton.contains(x,y) ? 1 : 0;
  step(dt,movement,pendingAttack || (touching && attackButton.contains(x,y)));
  pendingAttack=false;
  render(touching,x,y,movement);
}
