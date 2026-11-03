#pragma once
#include "GameAudio.h"
#include "DamageLed.h"
#include "GameGfx.h"
#include <math.h>
#ifndef GAME_AUDIO_HOST_TEST
#include <Preferences.h>
#endif

// Fixed-size entity pools and the shared 8-bit sprite keep memory use bounded.
namespace ZombieCrash {
constexpr int ground = 207;
constexpr int weaponBarY = 219;
constexpr float levelEnd = 2600.0f;
constexpr float arenaStart = 2180.0f;
constexpr int maxZombies = 16;
constexpr int maxArrows = 5;
constexpr int maxLife = 5;
enum Stage { CHARACTER_SELECT, WEAPON_SELECT, ACTIVE_LEVEL, ARMORY, LEVEL_COMPLETE, VICTORY, DEFEAT };
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
enum ExtraWeapon { NONE, GRENADES, SPEAR, DOG };
ExtraWeapon extra = NONE;
bool useExtra=false, armoryVisited=false, crownUnlocked=false, wearCrown=false;
int grenades=0, spikeWave=0;
float dogTime=0, dogX=0, dogAttack=0;
bool dogAvailable=false;
struct Thrown { float x,y,vx,vy; bool active, grenade; };
Thrown thrown[5] = {};
const TouchButton switchButton = {144,191,101,44};
const TouchButton outfitButton = {75,53,170,23};
const TouchButton armoryChoices[3] = {{8,85,96,94},{112,85,96,94},{216,85,96,94}};

float terrain(float x) {
  if (level!=3 || x>=1980) return ground;
  // Continuous ramps let the movement controls climb and descend naturally.
  float t=fmodf(max(0.0f,x-180),600.0f);
  if (t<150) return ground-t*0.32f;
  if (t<300) return ground-48;
  if (t<450) return ground-48+(t-300)*0.32f;
  return ground;
}
void loadRewards() {
#ifndef GAME_AUDIO_HOST_TEST
  Preferences prefs;
  if (prefs.begin("zombie-crash",true)) {
    crownUnlocked=prefs.getBool("king-trophy",false); prefs.end();
  }
#endif
}
void saveRewards() {
  crownUnlocked=true; wearCrown=true;
#ifndef GAME_AUDIO_HOST_TEST
  Preferences prefs;
  if (prefs.begin("zombie-crash",false)) {
    prefs.putBool("king-trophy",true); prefs.end();
  }
#endif
}

struct Zombie {
  float x, rising, flash;
  int hp;
  bool spitter;
  float spitTimer, attackTimer;
  bool flying;
  float flightPhase;
};
struct Spit { float x, y, vx, vy; bool active; };
struct Spike { float x, timer; bool active; };
Spit spit[maxSpit] = {};
Spike spikes[maxSpikes] = {};
struct Arrow { float x, remaining; int direction; bool active; float y; };
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
  uint16_t fill = held ? 0x5B8B : GameGfx::rgb(73,36,109);
  gameFrame.fillRoundRect(b.x, b.y+2, b.w, b.h, 7, GameGfx::rgb(0,0,36));
  gameFrame.fillRoundRect(b.x, b.y, b.w, b.h, 7, fill);
  gameFrame.drawFastHLine(b.x+6, b.y+2, b.w-12, GameGfx::lighter(fill));
  gameFrame.drawRoundRect(b.x, b.y, b.w, b.h, 7, held ? COLOR_GOLD : GameGfx::rgb(182,146,255));
  gameFrame.setTextColor(TFT_WHITE, fill);
  gameFrame.drawCentreString(label, b.x+b.w/2, b.y+(b.h-16)/2, 2);
}

float zombieFeet(const Zombie &z) {
  return terrain(z.x)-(z.flying ? 8+64*(0.5f+0.5f*cosf(z.flightPhase)) : 0);
}
void spawnZombie(float x) {
  // If a very long boss fight fills the pool, recycle the most distant zombie.
  int slot = 0;
  for (int i=0; i<maxZombies; ++i) {
    if (zombies[i].hp == 0) { slot=i; break; }
    if (fabsf(zombies[i].x-playerX) > fabsf(zombies[slot].x-playerX)) slot=i;
  }
  zombies[slot] = {constrain(x, 15.0f, levelEnd-15), 0.65f, 0, 2, level==2 && random(0,3)!=0, random(900,1800)/1000.0f, 0, level==3, 0};
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
    stage = level<3 ? LEVEL_COMPLETE : VICTORY;
    if (level==3) saveRewards();
    GameAudio::play(GameAudio::Win);
  }
}

void attack() {
  if (attackCooldown > 0 || stage != ACTIVE_LEVEL) return;
  if (useExtra && extra!=NONE) {
    if (extra==DOG) {
      if (dogAvailable) { dogAvailable=false; dogTime=10; dogX=playerX; dogAttack=0; }
      return;
    }
    if (extra==GRENADES && grenades<=0) return;
    for (auto &p : thrown) if (!p.active) {
      p={playerX+facing*17,terrain(playerX)-30,facing*180.0f,-180,true,extra==GRENADES};
      if (extra==GRENADES) --grenades;
      attackCooldown=0.65f; GameAudio::play(GameAudio::Bow); break;
    }
    return;
  }
  attackCooldown = bow ? 0.48f : 0.36f;
  GameAudio::play(bow ? GameAudio::Bow : GameAudio::Sword);
  swing = 0.18f;
  if (bow) {
    for (auto &a : arrows) {
      if (!a.active) { a = {playerX + facing*17, 460, facing, true, terrain(playerX)-25}; break; }
    }
  } else {
    // A swing damages only the nearest enemy in front, once per attack.
    float nearest = 63;
    int target = -1;
    for (int i=0; i<maxZombies; ++i) {
      float dx = (zombies[i].x-playerX)*facing;
      if (zombies[i].hp>0 && zombies[i].rising<=0 && fabsf(zombieFeet(zombies[i])-terrain(playerX))<42 && dx>=-8 && dx<nearest) {
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
  life=maxLife; kills=0; bossLife=level==3 ? 15 : 10; bossX=2500; bossActive=false;
  attackCooldown=swing=immunity=summonFlash=walkPhase=0;
  spawnTimer=1.6f; summonTimer=5; pendingAttack=false;
  animationTime=bossAttack=0; spikeTimer=2; spikeWave=0;
  extra=NONE; useExtra=false; armoryVisited=false; grenades=0;
  dogTime=dogAttack=0; dogAvailable=false;
  for (auto &p : thrown) p.active=false;
  for (auto &p : spit) p.active=false;
  for (auto &p : spikes) p.active=false;
  for (auto &z : zombies) z = {0,0,0,0};
  for (auto &a : arrows) a = {0,0,1,false};
  lastFrame=now;
}

void hurtPlayer() {
  if (immunity>0 || stage!=ACTIVE_LEVEL) return;
  DamageLed::flash(millis());
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
  ++spikeWave;
  for (int i=0;i<maxSpikes;++i) {
    // Alternate halves of each lane: every wave moves even if the camera stays still.
    float offset=(spikeWave%2) ? 0.18f : 0.82f;
    float x=left+(right-left)*(i+offset)/maxSpikes;
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

void updateExtras(float dt) {
  for (auto &p : thrown) if (p.active) {
    float remaining=dt;
    while (remaining>0 && p.active && stage==ACTIVE_LEVEL) {
      float tick=min(remaining,0.015f); remaining-=tick;
      p.x+=p.vx*tick; p.y+=p.vy*tick; p.vy+=300*tick;
      bool impact=p.y>=terrain(p.x);
      for (int i=0;i<maxZombies;++i) if (zombies[i].hp>0 && zombies[i].rising<=0 &&
          fabsf(p.x-zombies[i].x)<15 && fabsf(p.y-(zombieFeet(zombies[i])-25))<26) {
        impact=true; if (!p.grenade) hurtZombie(i,3); break;
      }
      if (bossActive && fabsf(p.x-bossX)<24 && p.y>terrain(bossX)-100 && p.y<terrain(bossX)) {
        impact=true; if (!p.grenade) hurtBoss(3);
      }
      if (impact) {
        if (p.grenade) {
          for (int i=0;i<maxZombies;++i)
            if (fabsf(zombies[i].x-p.x)<85 && fabsf(zombieFeet(zombies[i])-25-p.y)<85) hurtZombie(i,4);
          if (fabsf(bossX-p.x)<85) hurtBoss(4);
          GameAudio::play(GameAudio::Hit);
        }
        p.active=false;
      }
      if (p.x<0 || p.x>levelEnd) p.active=false;
    }
  }
  if (dogTime>0 && stage==ACTIVE_LEVEL) {
    float activeDt=min(dt,dogTime);
    dogTime=max(0.0f,dogTime-dt); dogAttack=max(0.0f,dogAttack-activeDt);
    float target=bossActive ? bossX : playerX+facing*35;
    int victim=-1; float nearest=10000;
    for (int i=0;i<maxZombies;++i) if (zombies[i].hp>0 && zombies[i].rising<=0 &&
        fabsf(zombieFeet(zombies[i])-terrain(zombies[i].x))<32 && fabsf(zombies[i].x-dogX)<nearest) {
      nearest=fabsf(zombies[i].x-dogX); target=zombies[i].x; victim=i;
    }
    dogX+=constrain(target-dogX,-145*activeDt,145*activeDt);
    if (fabsf(target-dogX)<28 && dogAttack<=0) {
      if (victim>=0) hurtZombie(victim,1);
      else if (bossActive) hurtBoss(1);
      dogAttack=0.8f;
    }
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
  if (level==3 && !armoryVisited && playerX>=2040) {
    playerX=2040; stage=ARMORY; pendingAttack=false; return;
  }
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
    } else if (level==3) {
      summonTimer-=dt;
      if (summonTimer<=0) { summonTimer=5; spawnZombie(playerX+100); spawnZombie(playerX-100); }
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
    if (z.flying) z.flightPhase+=dt*2.4f;
    float distance=fabsf(playerX-z.x);
    if (!z.spitter || distance>180 || distance<60)
      z.x+=(playerX>z.x ? 1 : -1)*(z.flying ? 48 : 24)*dt;
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
      if (zombies[i].hp<=0 || zombies[i].rising>0 || fabsf(a.y-(zombieFeet(zombies[i])-25))>25) continue;
      float along=(zombies[i].x-from)*a.direction;
      if (along>=-12 && along<=travel+12 && max(0.0f,along-12)<closest) {
        closest=max(0.0f,along-12); target=i;
      }
    }
    float bossAlong=(bossX-from)*a.direction;
    if (bossActive && fabsf(a.y-(terrain(bossX)-45))<55 && bossAlong>=-20 && bossAlong<=travel+20 && max(0.0f,bossAlong-20)<closest) {
      hurtBoss(bowDamage); a.active=false;
    } else if (target>=0) { hurtZombie(target,bowDamage); a.active=false; }
    if (a.y>terrain(a.x) || a.remaining<=0 || a.x<0 || a.x>levelEnd) a.active=false;
    if (stage!=ACTIVE_LEVEL) return;
  }
  updateExtras(dt);
  if (stage!=ACTIVE_LEVEL) return;
  for (auto &z : zombies)
    if (fabsf(zombieFeet(z)-terrain(playerX))<32 && z.hp>0 && z.rising<=0 && meleeStrike(z.attackTimer,fabsf(z.x-playerX),26,dt)) hurtPlayer();
  if (bossActive && meleeStrike(bossAttack,fabsf(bossX-playerX),36,dt)) hurtPlayer();
  updateHazards(dt);
}

using GameGfx::rgb;
const uint16_t panel=rgb(36,36,85), panelEdge=rgb(109,73,170);
const uint16_t skin=rgb(255,219,170), skinShade=rgb(219,146,85);
const uint16_t leather=rgb(109,73,0), lavaGlow=rgb(255,146,0);

// Soft contact shadow just below the feet; it shrinks as fliers rise.
void shadow(int x, int y, int rx) {
  for (int dy=0; dy<3; ++dy) GameGfx::darkenSpan(x-rx+dy*rx/4, x+rx-dy*rx/4+1, y+dy);
}
void heart(int x, int y, uint16_t color) {
  gameFrame.fillCircle(x-3, y, 3, color); gameFrame.fillCircle(x+3, y, 3, color);
  gameFrame.fillTriangle(x-6, y+1, x+6, y+1, x, y+7, color);
}

void person(int x, int feet, bool isFemale, int direction, bool walking) {
  uint16_t shirt=wearCrown ? COLOR_GOLD : isFemale ? rgb(219,73,170) : rgb(36,109,255);
  uint16_t hair=isFemale ? rgb(146,73,0) : rgb(73,36,0);
  int stride=walking ? (int)(sinf(walkPhase)*4) : 0;
  int back=-direction;
  // Long hair falls behind the shoulders, then the far arm swings opposite.
  if (isFemale) {
    gameFrame.fillRoundRect(x-9, feet-48, 18, 24, 5, hair);
    gameFrame.fillTriangle(x+back*8, feet-46, x+back*14, feet-26, x+back*4, feet-30, hair);
  }
  gameFrame.drawWideLine(x+back*3, feet-30, x+back*5-stride, feet-17, 4, skinShade);
  gameFrame.drawWideLine(x-3, feet-14, x-4-stride, feet-3, 5, rgb(36,36,109));
  gameFrame.drawWideLine(x+3, feet-14, x+4+stride, feet-3, 5, rgb(36,73,170));
  gameFrame.fillRoundRect(x-7-stride, feet-4, 7, 4, 1, leather);
  gameFrame.fillRoundRect(x+1+stride, feet-4, 7, 4, 1, leather);
  gameFrame.fillRoundRect(x-8, feet-33, 16, 21, 3, shirt);
  gameFrame.fillRect(back>0 ? x+3 : x-8, feet-30, 5, 16, GameGfx::darker(shirt));
  gameFrame.fillRect(x-8, feet-16, 16, 3, leather);
  gameFrame.fillRect(x-2, feet-16, 4, 3, COLOR_GOLD);
  gameFrame.fillRect(x-3, feet-34, 6, 3, skin);
  gameFrame.fillCircle(x, feet-41, 8, skin);
  gameFrame.fillRect(x+back*5, feet-42, 3, 4, skinShade);
  // Hair cap with a fringe on the facing side.
  gameFrame.fillRoundRect(x-8, feet-50, 16, 7, 3, hair);
  gameFrame.fillRect(back>0 ? x+2 : x-8, feet-46, 6, 6, hair);
  gameFrame.fillRect(x+direction*3, feet-43, 3, 3, TFT_WHITE);
  gameFrame.fillRect(x+direction*4+(direction>0 ? 0 : 1), feet-42, 1, 2, TFT_BLACK);
  gameFrame.drawFastHLine(x+direction*2-(direction<0 ? 3 : 0), feet-36, 4, rgb(182,73,85));
  if (wearCrown) {
    gameFrame.fillRect(x-9, feet-54, 18, 5, COLOR_GOLD);
    for (int i=-1;i<=1;++i) gameFrame.fillTriangle(x+i*6-3, feet-54, x+i*6, feet-61, x+i*6+3, feet-54, COLOR_GOLD);
    gameFrame.drawFastHLine(x-9, feet-50, 18, rgb(182,109,0));
    gameFrame.fillRect(x-1, feet-53, 3, 3, TFT_RED);
  }
  gameFrame.drawWideLine(x+direction*4, feet-30, x+direction*10, feet-26, 5, GameGfx::darker(shirt));
  gameFrame.drawWideLine(x+direction*10, feet-26, x+direction*15, feet-24, 4, skin);
}

void drawWeapon(int x, int feet, bool useBow, int direction, bool attacking) {
  int hand=x+direction*16, y=feet-25;
  if (useBow) {
    // Curved limb drawn as short segments, string pulled back to the hand.
    int px=hand, py=y-14;
    for (int i=1;i<=6;++i) {
      int ny=y-14+i*28/6, nx=hand+direction*(int)(7*sinf(i*3.14159f/6));
      gameFrame.drawWideLine(px, py, nx, ny, 3, i==3 || i==4 ? leather : rgb(182,109,0));
      px=nx; py=ny;
    }
    gameFrame.drawLine(hand, y-14, hand-direction*3, y, rgb(219,219,255));
    gameFrame.drawLine(hand-direction*3, y, hand, y+14, rgb(219,219,255));
    gameFrame.drawLine(hand-direction*3, y, hand+direction*14, y, rgb(182,109,0));
    gameFrame.fillTriangle(hand+direction*14, y-2, hand+direction*18, y, hand+direction*14, y+2, TFT_WHITE);
  } else {
    int tipX=hand+direction*(attacking ? 38 : 13), tipY=y-(attacking ? 5 : 27);
    if (attacking) {
      // Motion arc behind the blade.
      for (int r=30;r<=38;r+=4)
        for (int a=-70;a<0;a+=10) {
          float t0=a*0.01745f, t1=(a+10)*0.01745f;
          gameFrame.drawLine(hand+direction*(int)(r*cosf(t0)), y+(int)(r*sinf(t0)),
                             hand+direction*(int)(r*cosf(t1)), y+(int)(r*sinf(t1)),
                             r==38 ? rgb(146,219,255) : TFT_WHITE);
        }
    }
    gameFrame.drawWideLine(hand, y, tipX, tipY, 4, rgb(182,182,170));
    gameFrame.drawLine(hand, y-1, tipX, tipY-1, TFT_WHITE);
    float len=sqrtf((tipX-hand)*(tipX-hand)+(tipY-y)*(tipY-y));
    int gx=(int)((tipY-y)/len*5), gy=(int)(-(tipX-hand)/len*5);
    gameFrame.drawWideLine(hand-gx, y-gy, hand+gx, y+gy, 3, COLOR_GOLD);
    gameFrame.drawWideLine(hand, y, hand-(tipX-hand)/(attacking ? 8 : 4), y-(tipY-y)/(attacking ? 8 : 4), 3, leather);
  }
}

void drawZombie(float worldX, bool chief, float rising, int hp, bool flash,
                float attackTimer, bool spitter = false, float altitude = 0) {
  int x=(int)(worldX-camera);
  if (x<-40 || x>360) return;
  int scale=chief ? 2 : 1;
  int feet=(int)(terrain(worldX)-altitude+rising*45);
  int dir=playerX>worldX ? 1 : -1;
  if (level==3 && !chief) {
    // Bat wings: membrane panels between finger bones.
    int flap=(int)(sinf(animationTime*10)*10);
    for (int side=-1;side<=1;side+=2) {
      gameFrame.fillTriangle(x+side*4, feet-34, x+side*30, feet-52+flap, x+side*24, feet-22, rgb(109,36,36));
      gameFrame.fillTriangle(x+side*4, feet-34, x+side*24, feet-22, x+side*12, feet-20, rgb(73,0,36));
      gameFrame.drawLine(x+side*4, feet-34, x+side*30, feet-52+flap, rgb(182,73,85));
      gameFrame.drawLine(x+side*10, feet-38+flap/3, x+side*24, feet-22, rgb(182,73,85));
    }
  }
  uint16_t skinTone=flash ? TFT_WHITE : level==2 ? rgb(182,219,255) : (chief ? rgb(109,182,36) : rgb(146,182,73));
  uint16_t skinDark=flash ? TFT_WHITE : GameGfx::mix565(skinTone, TFT_BLACK, 1, 3);
  uint16_t cloth=level==2 ? rgb(73,109,170) : chief ? rgb(146,36,36) : rgb(109,73,36);
  uint16_t pants=level==2 ? rgb(36,73,109) : rgb(73,73,85);
  // Ragged trousers and shirt hems.
  gameFrame.fillRect(x-7*scale, feet-17*scale, 5*scale, 15*scale, pants);
  gameFrame.fillRect(x+2*scale, feet-17*scale, 5*scale, 15*scale, GameGfx::mix565(pants,TFT_BLACK,1,4));
  gameFrame.fillTriangle(x-7*scale, feet-3*scale, x-2*scale, feet-3*scale, x-5*scale, feet, pants);
  gameFrame.fillRect(x-8*scale+dir*scale, feet-2*scale, 6*scale, 2*scale, skinDark);
  gameFrame.fillRect(x+2*scale+dir*scale, feet-2*scale, 6*scale, 2*scale, skinDark);
  gameFrame.fillRoundRect(x-9*scale, feet-34*scale, 18*scale, 20*scale, 3, cloth);
  gameFrame.fillRect(dir>0 ? x-9*scale : x+4*scale, feet-31*scale, 5*scale, 15*scale, GameGfx::darker(cloth));
  for (int i=0;i<3;++i)
    gameFrame.fillTriangle(x+(-9+i*6)*scale, feet-15*scale, x+(-3+i*6)*scale, feet-15*scale,
                           x+(-6+i*6)*scale, feet-(11+i%2*2)*scale, cloth);
  gameFrame.fillRect(x+(dir>0 ? 1 : -4)*scale, feet-27*scale, 3*scale, 4*scale, skinDark);
  // Head with sunken eyes, jaw shadow and teeth.
  gameFrame.fillRoundRect(x-8*scale, feet-49*scale, 16*scale, 16*scale, 3, skinTone);
  gameFrame.fillRect(x-8*scale, feet-37*scale, 16*scale, 4*scale, skinDark);
  gameFrame.fillRect(x+(dir>0 ? 0 : -6)*scale, feet-37*scale, 6*scale, 2*scale, rgb(36,0,0));
  gameFrame.drawFastHLine(x+(dir>0 ? 1 : -5)*scale, feet-37*scale, 4*scale, TFT_WHITE);
  // Both arms wind back and sweep forward with the melee damage timing.
  float phase=attackTimer>0 ? (0.85f-attackTimer)/0.85f : 0;
  int reach=attackTimer>0 ? (int)(8+15*sinf(phase*3.14159f)) : 13;
  int lift=attackTimer>0 ? (int)(-13*cosf(phase*3.14159f)) : (int)(sinf(animationTime*4+worldX)*2);
  for (int arm=0;arm<2;++arm) {
    int shoulder=x+(arm ? 5 : -5)*scale;
    int y=feet-(arm ? 28 : 31)*scale;
    gameFrame.drawWideLine(shoulder,y,x+dir*reach*scale,y+lift*scale,3*scale,arm ? skinTone : skinDark);
    gameFrame.fillCircle(x+dir*reach*scale,y+lift*scale,2*scale,arm ? skinTone : skinDark);
  }
  gameFrame.fillRect(x-6*scale, feet-44*scale, 5*scale, 4*scale, rgb(36,0,0));
  gameFrame.fillRect(x+2*scale, feet-44*scale, 5*scale, 4*scale, rgb(36,0,0));
  gameFrame.fillRect(x-5*scale+dir*scale, feet-43*scale, 2*scale, 2*scale, TFT_RED);
  gameFrame.fillRect(x+3*scale+dir*scale, feet-43*scale, 2*scale, 2*scale, TFT_RED);
  // Broken skull rim, pink brain lobes and dark folds remain visible on ice zombies.
  gameFrame.fillRect(x-7*scale,feet-50*scale,14*scale,3*scale,0xEF7B);
  gameFrame.fillCircle(x-3*scale,feet-51*scale,4*scale,0xFBB6);
  gameFrame.fillCircle(x+3*scale,feet-51*scale,4*scale,0xFBB6);
  gameFrame.fillCircle(x-4*scale,feet-53*scale,scale,TFT_WHITE);
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
    gameFrame.drawFastVLine(x+dir*3,feet-34,4+(int)(animationTime*6)%3,0x07E0);
  }
  if (chief && level==3) {
    // Smoky genie tail, gold cuffs, and a three-point royal crown.
    gameFrame.fillTriangle(x-20,feet-35,x+20,feet-35,x+(int)(sinf(animationTime*4)*14),feet,0xA29F);
    gameFrame.fillTriangle(x-12,feet-35,x+12,feet-35,x+(int)(sinf(animationTime*4+0.6f)*10),feet-8,rgb(219,146,255));
    gameFrame.fillRect(x-18,feet-101,36,8,COLOR_GOLD);
    gameFrame.drawFastHLine(x-18,feet-94,36,rgb(182,109,0));
    for (int i=-1;i<=1;++i) gameFrame.fillTriangle(x+i*13-6,feet-101,x+i*13,feet-115,x+i*13+6,feet-101,COLOR_GOLD);
    gameFrame.fillCircle(x,feet-98,3,TFT_RED);
    gameFrame.fillCircle(x-11,feet-97,2,rgb(36,146,255));
    gameFrame.fillCircle(x+11,feet-97,2,rgb(0,219,85));
  } else if (chief) {
    // Side horns frame the exposed brain instead of covering it.
    for (int side=-1;side<=1;side+=2) {
      uint16_t horn=level==2 ? 0xBFFF : COLOR_GOLD;
      gameFrame.fillTriangle(x+side*12,feet-98,x+side*24,feet-116,x+side*21,feet-96,horn);
      gameFrame.drawLine(x+side*14,feet-99,x+side*23,feet-113,GameGfx::lighter(horn));
    }
    gameFrame.fillRect(x-15,feet-98,30,3,level==2 ? TFT_WHITE : COLOR_GOLD);
  } else {
    gameFrame.fillRect(x-10,feet-62,20,5,TFT_BLACK);
    gameFrame.fillRect(x-9,feet-61,hp*9,3,COLOR_MINT);
  }
}

void drawHazards() {
  for (const auto &p : spikes) if (p.active) {
    int x=(int)(p.x-camera);
    if (p.timer>spikeDuration) {
      // Pulsing warning ring.
      bool pulse=((int)(p.timer*8))%2;
      GameGfx::screenRect(x-23,ground-3,47,7,TFT_RED);
      gameFrame.drawEllipse(x,ground,23,4,pulse ? TFT_WHITE : TFT_RED);
      gameFrame.drawLine(x-7,ground-10,x+7,ground-3,TFT_WHITE);
      gameFrame.drawLine(x+7,ground-10,x-7,ground-3,TFT_WHITE);
    } else {
      gameFrame.fillEllipse(x,ground,23,4,0x4355);
      int height=(int)(34*min(1.0f,(spikeDuration-p.timer)*12+0.15f));
      for (int i=-1;i<=1;++i) {
        int peak=ground-height+(i==0 ? -7 : 0);
        gameFrame.fillTriangle(x+i*13-7,ground,x+i*13,peak,x+i*13+7,ground,0xBFFF);
        gameFrame.fillTriangle(x+i*13,peak,x+i*13+7,ground,x+i*13+2,ground,rgb(109,182,255));
        gameFrame.drawLine(x+i*13-1,peak+2,x+i*13-4,ground-2,TFT_WHITE);
      }
    }
  }
  for (const auto &p : spit) if (p.active) {
    int x=(int)(p.x-camera), y=(int)p.y;
    gameFrame.drawWideLine(x-p.vx*0.035f,y-p.vy*0.035f,x,y,3,0x5F6A);
    GameGfx::glow(x,y,7,rgb(146,255,85));
    gameFrame.fillCircle(x,y,4,0x07E0);
    gameFrame.fillCircle(x-1,y-1,1,TFT_WHITE);
  }
}

void card(const TouchButton &b) {
  GameGfx::gradient(b.x+2,b.y+2,b.w-4,b.h-4,rgb(73,73,170),rgb(36,36,85));
  gameFrame.drawRoundRect(b.x,b.y,b.w,b.h,8,panelEdge);
  gameFrame.drawRoundRect(b.x+1,b.y+1,b.w-2,b.h-2,7,rgb(36,0,85));
  gameFrame.drawFastHLine(b.x+8,b.y+2,b.w-16,rgb(146,146,255));
}
// Moonlit graveyard backdrop shared by the menu screens.
void graveyard() {
  GameGfx::gradient(0,0,320,240,rgb(36,0,85),rgb(73,36,109));
  GameGfx::glow(280,40,22,rgb(219,219,255));
  gameFrame.fillCircle(280,40,13,rgb(255,255,170));
  gameFrame.fillCircle(276,36,3,rgb(219,219,170));
  for (int i=0;i<9;++i) {
    int x=i*38+(i*17)%13, h=12+(i*7)%9;
    gameFrame.fillRoundRect(x,232-h,14,h+8,6,rgb(36,36,85));
    if (i%3==0) {
      gameFrame.fillRect(x+6,226-h,2,8,rgb(36,36,85));
      gameFrame.fillRect(x+3,228-h,8,2,rgb(36,36,85));
    }
  }
}

void drawSelection() {
  graveyard();
  button(exitButton,"BACK");
  gameFrame.setTextColor(rgb(36,0,0));
  gameFrame.drawCentreString("ZOMBIE CRASH",162,38,4);
  gameFrame.setTextColor(rgb(146,255,0));
  gameFrame.drawCentreString("ZOMBIE CRASH",160,36,4);
  bool choosingCharacter=stage==CHARACTER_SELECT;
  if (choosingCharacter && crownUnlocked) {
    button(outfitButton,wearCrown ? "CROWN: ON" : "CROWN: OFF");
    gameFrame.setTextColor(COLOR_GOLD);
    gameFrame.drawRightString("TROPHY WON",313,12,1);
  }
  card(firstChoice); card(secondChoice);
  if (choosingCharacter) {
    shadow(86,145,14); shadow(234,145,14);
    person(86,145,false,1,false); person(234,145,true,-1,false);
  } else {
    drawWeapon(70,147,false,1,true); drawWeapon(220,147,true,1,false);
  }
  gameFrame.setTextColor(TFT_WHITE);
  gameFrame.drawCentreString(choosingCharacter ? "MALE" : "SWORD",86,155,2);
  gameFrame.drawCentreString(choosingCharacter ? "FEMALE" : "BOW",234,155,2);
  gameFrame.drawCentreString(choosingCharacter ? "Choose your hero" : "Choose your weapon",160,186,2);
  gameFrame.setTextColor(rgb(182,182,255));
  gameFrame.drawCentreString("Hold < > to move. Hold A to attack.",160,210,1);
  gameFrame.drawCentreString("Sword: 2 damage. Bow: 1 damage.",160,224,1);
}

// Two-tone peaks: lit west face, shaded east face, optional snow cap.
void peak(int left, int top, int right, int apex, uint16_t lit, uint16_t shade, uint16_t cap) {
  gameFrame.fillTriangle(left,ground,apex,top,apex,ground,lit);
  gameFrame.fillTriangle(apex,top,right,ground,apex,ground,shade);
  if (cap) {
    int h=(ground-top)/4;
    int l=apex-(apex-left)*h/(ground-top), r=apex+(right-apex)*h/(ground-top);
    gameFrame.fillTriangle(l,top+h,apex,top,r,top+h,cap);
    gameFrame.fillTriangle(l,top+h,(l+apex)/2,top+h+4,apex,top+h,cap);
    gameFrame.fillTriangle(apex,top+h,(r+apex)/2,top+h+5,r,top+h,cap);
  }
}
void tree(int x) {
  if (level==2) {
    gameFrame.fillRect(x,180,5,ground-180,rgb(73,36,0));
    for (int t=0;t<3;++t) {
      int top=112+t*20, w=14+t*6;
      gameFrame.fillTriangle(x+2-w,top+28,x+2,top,x+2+w,top+28,rgb(0,73,85));
      gameFrame.fillTriangle(x+2,top,x+2+w,top+28,x+2,top+28,rgb(0,36,85));
      gameFrame.fillTriangle(x+2-w*2/3,top+12,x+2,top,x+2+w/3,top+8,TFT_WHITE);
    }
  } else if (level==3) {
    // Obsidian spire with a molten seam that flickers.
    gameFrame.fillTriangle(x-22,ground,x+2,ground-65,x+2,ground,rgb(73,36,85));
    gameFrame.fillTriangle(x+2,ground-65,x+27,ground,x+2,ground,rgb(36,0,36));
    uint16_t seam=((int)(animationTime*6)+x)%4 ? rgb(255,73,0) : rgb(255,219,0);
    gameFrame.drawWideLine(x+2,ground-50,x+8,ground-30,2,seam);
    gameFrame.drawWideLine(x+8,ground-30,x+4,ground-12,2,seam);
  } else {
    gameFrame.fillRect(x-1,125,7,ground-125,rgb(109,73,0));
    gameFrame.fillRect(x+3,125,3,ground-125,rgb(73,36,0));
    gameFrame.fillCircle(x-7,129,11,rgb(0,109,0));
    gameFrame.fillCircle(x+12,129,11,rgb(0,109,0));
    gameFrame.fillCircle(x+2,118,15,rgb(36,146,0));
    gameFrame.fillCircle(x-3,113,8,rgb(73,182,0));
    gameFrame.fillCircle(x-5,111,3,rgb(146,219,0));
  }
}
// Ground band from 'from' down to the weapon bar; textures follow the camera.
void groundStrip(int from) {
  int scroll=(int)camera;
  if (level==2) {
    gameFrame.fillRect(0,from,320,weaponBarY-from,rgb(146,219,255));
    gameFrame.fillRect(0,from,320,max(0,ground+4-from),TFT_WHITE);
    for (int i=-1;i<12;++i) {
      int x=i*31-scroll%31;
      gameFrame.drawLine(x,ground+6,x+9,ground+9,rgb(109,182,255));
    }
  } else {
    gameFrame.fillRect(0,from,320,weaponBarY-from,rgb(146,73,0));
    gameFrame.fillRect(0,from,320,max(0,ground+4-from),rgb(73,182,0));
    if (from<=ground+4) gameFrame.drawFastHLine(0,ground+4,320,rgb(36,109,0));
    for (int i=-1;i<16;++i) {
      int wx=(scroll/23+i)*23, x=wx-scroll+(wx*7)%11;
      gameFrame.fillEllipse(x,ground+8+(wx/23)%3,2+(wx/23)%2,1,rgb(182,146,85));
    }
  }
}
void scenery() {
  const uint16_t skyTop=level==3 ? rgb(36,0,0) : level==2 ? rgb(109,146,219) : rgb(36,109,255);
  const uint16_t skyLow=level==3 ? rgb(219,73,0) : level==2 ? rgb(219,255,255) : rgb(182,219,255);
  GameGfx::gradient(0,52,320,ground-52,skyTop,skyLow);
  int sunX=267-(int)(camera*0.02f);
  if (level==3) {
    GameGfx::glow(sunX,82,30,rgb(255,73,0));
    gameFrame.fillCircle(sunX,82,20,rgb(219,36,0));
    gameFrame.fillCircle(sunX-6,77,4,rgb(182,36,0));
  } else {
    GameGfx::glow(sunX,71,23,rgb(255,255,170));
    gameFrame.fillCircle(sunX,71,17,level==2 ? rgb(255,255,170) : rgb(255,255,85));
  }
  if (level!=3) {
    for (int i=0;i<4;++i) {
      int cx=(i*97+(int)(animationTime*5)-(int)(camera*0.08f))%400;
      cx=(cx+400)%400-40;
      int cy=64+(i*23)%28;
      gameFrame.fillEllipse(cx,cy+4,22,6,rgb(219,219,255));
      gameFrame.fillCircle(cx-8,cy,7,TFT_WHITE);
      gameFrame.fillCircle(cx+5,cy-3,9,TFT_WHITE);
    }
  }
  for (int i=-1;i<5;++i) {
    int x=i*110-(int)(camera*0.18f)%110;
    if (level==3) peak(x-70,80,x+140,x+45,rgb(109,36,36),rgb(73,0,36),rgb(255,73,0));
    else if (level==2) peak(x-70,80,x+140,x+45,rgb(182,219,255),rgb(109,146,219),TFT_WHITE);
    else peak(x-70,80,x+140,x+45,rgb(109,146,219),rgb(73,109,170),TFT_WHITE);
    if (level==3) peak(x-30,112,x+180,x+100,rgb(73,36,36),rgb(36,0,36),0);
    else if (level==2) peak(x-30,112,x+180,x+100,rgb(146,182,255),rgb(109,146,219),0);
    else peak(x-30,112,x+180,x+100,rgb(73,182,85),rgb(36,146,85),0);
  }
  if (level==3) {
    // Embers drift upward from the lava.
    for (int i=0;i<14;++i) {
      int x=(i*53-(int)(camera*0.4f))%330; x=(x+330)%330-5;
      int y=ground-(int)fmodf(animationTime*(25+i%4*8)+i*37,150);
      gameFrame.fillRect(x+(int)(sinf(animationTime*3+i)*3),y,2,2,i%3 ? lavaGlow : rgb(255,255,85));
    }
  }
  for (int i=-1;i<6;++i) tree(i*85-(int)(camera*0.55f)%85);
  if (level==3) {
    // Lava sea glows beneath and around the rock causeway.
    GameGfx::gradient(0,ground-17,320,weaponBarY-ground+17,rgb(255,219,0),rgb(219,36,0));
    GameGfx::screenRect(0,ground-23,320,6,lavaGlow);
    for (int i=0;i<7;++i) {
      int x=(i*47+(int)(animationTime*20))%330-5;
      int r=1+(int)(animationTime*3+i)%3;
      gameFrame.drawCircle(x,ground-14+i%3*4,r,rgb(255,255,85));
    }
    for (int x=0;x<320;++x) {
      int y=(int)terrain(camera+x);
      gameFrame.drawFastVLine(x,y,weaponBarY-y,rgb(73,36,36));
      gameFrame.drawFastVLine(x,y+2,5,rgb(109,36,36));
      gameFrame.drawFastVLine(x,y,2,rgb(255,146,0));
      if (((x+(int)camera)*7)%13==0) gameFrame.drawPixel(x,y+9+((x+(int)camera)%5),rgb(146,73,85));
    }
  } else {
    groundStrip(ground);
    uint16_t tuft=level==2 ? TFT_WHITE : rgb(36,146,0);
    for (int i=0;i<25;++i) {
      int x=i*15-(int)camera%15;
      gameFrame.drawLine(x,ground,x+3,ground-5,tuft);
      gameFrame.drawLine(x+3,ground,x+5,ground-3,level==2 ? rgb(182,219,255) : rgb(146,219,0));
    }
  }
}
void hudPanel(int y, int h) {
  GameGfx::gradient(0,y,320,h,rgb(36,36,85),rgb(0,0,36));
}

void render(bool touching, uint16_t tx, uint16_t ty, int movement) {
  if (stage==CHARACTER_SELECT || stage==WEAPON_SELECT) drawSelection();
  else if (stage==ARMORY) {
    GameGfx::gradient(0,0,320,240,rgb(73,0,0),rgb(36,0,36));
    GameGfx::gradient(0,200,320,40,rgb(73,0,0),rgb(219,73,0));
    button(exitButton,"BACK");
    gameFrame.setTextColor(COLOR_GOLD);
    gameFrame.drawCentreString("LAVA REALM ARMORY",160,40,4);
    button(armoryChoices[0],"5 GRENADES"); button(armoryChoices[1],"SPEAR"); button(armoryChoices[2],"DOG");
    gameFrame.setTextColor(TFT_WHITE);
    gameFrame.drawCentreString("Choose one item for the King battle",160,65,1);
    gameFrame.drawCentreString("Arc throws / reusable spear / dog: 10s",160,194,1);
    gameFrame.drawCentreString("Tap weapon button to switch, A to use",160,216,1);
  } else {
    scenery();
    if (level==3 && !armoryVisited) {
      int ax=(int)(2040-camera);
      gameFrame.fillRect(ax-25,ground-43,50,43,rgb(109,73,36));
      for (int r=ground-38;r<ground;r+=8) gameFrame.drawFastHLine(ax-25,r,50,rgb(73,36,36));
      gameFrame.fillRoundRect(ax-8,ground-20,16,20,6,rgb(36,0,0));
      GameGfx::glow(ax,ground-8,7,lavaGlow);
      gameFrame.fillTriangle(ax-31,ground-43,ax,ground-62,ax+31,ground-43,rgb(73,36,85));
      gameFrame.setTextColor(COLOR_GOLD);
      gameFrame.drawCentreString("ARMORY",ax,ground-35,1);
    }
    int signX=(int)(2100-camera);
    if(signX>-50 && signX<340) {
      gameFrame.fillRect(signX,145,4,ground-145,rgb(109,73,0));
      gameFrame.fillRoundRect(signX-22,125,58,23,3,rgb(109,73,0));
      gameFrame.fillRoundRect(signX-20,127,54,19,2,rgb(146,109,36));
      gameFrame.setTextColor(rgb(73,0,0));
      gameFrame.drawString("BOSS >",signX-15,131,1);
    }
    for (const auto &z : zombies) if(z.hp>0) drawZombie(z.x,false,z.rising,z.hp,z.flash>0,z.attackTimer,z.spitter,terrain(z.x)-zombieFeet(z));
    if(bossActive && bossLife>0) drawZombie(bossX,true,0,bossLife,summonFlash>0,bossAttack);
    // Cover the part of rising zombies below the grass surface.
    if (level!=3) groundStrip(ground+1);
    for (const auto &z : zombies) if (z.hp>0 && z.rising<=0) {
      float air=terrain(z.x)-zombieFeet(z);
      shadow((int)(z.x-camera),(int)terrain(z.x),max(4,13-(int)(air/8)));
    }
    if (bossActive && bossLife>0) shadow((int)(bossX-camera),(int)terrain(bossX),24);
    if(immunity<=0 || ((int)(immunity*12)%2)==0) {
      int px=(int)(playerX-camera);
      int feet=(int)terrain(playerX);
      shadow(px,feet,13);
      person(px,feet,female,facing,movement!=0);
      if (!useExtra) drawWeapon(px,feet,bow,facing,swing>0);
      else if (extra==SPEAR) {
        gameFrame.drawWideLine(px+facing*15,feet-19,px+facing*36,feet-52,3,0xDDA6);
        gameFrame.fillTriangle(px+facing*32,feet-50,px+facing*40,feet-60,px+facing*39,feet-47,TFT_WHITE);
      } else if (extra==GRENADES && grenades>0) {
        gameFrame.fillCircle(px+facing*17,feet-25,5,0x5E07);
        gameFrame.fillCircle(px+facing*16,feet-27,1,TFT_WHITE);
      }
    }
    for(const auto &a : arrows) if(a.active) {
      int x=(int)(a.x-camera), y=(int)a.y;
      gameFrame.drawLine(x-a.direction*13,y,x,y,rgb(182,109,0));
      gameFrame.fillTriangle(x,y-2,x+a.direction*4,y,x,y+2,TFT_WHITE);
      gameFrame.drawLine(x-a.direction*13,y,x-a.direction*16,y-3,TFT_RED);
      gameFrame.drawLine(x-a.direction*13,y,x-a.direction*16,y+3,TFT_RED);
    }
    for (const auto &p : thrown) if (p.active) {
      int x=(int)(p.x-camera), y=(int)p.y;
      if (p.grenade) {
        gameFrame.fillCircle(x,y,5,0x5E07); gameFrame.fillCircle(x-1,y-2,1,TFT_WHITE);
        gameFrame.drawLine(x,y-5,x+3,y-9,COLOR_GOLD);
        GameGfx::glow(x+3,y-9,3,rgb(255,255,85));
      } else { float len=sqrtf(p.vx*p.vx+p.vy*p.vy); int dx=(int)(p.vx/len*19),dy=(int)(p.vy/len*19);
        gameFrame.drawWideLine(x-dx,y-dy,x,y,3,0xDDA6); gameFrame.fillCircle(x,y,3,TFT_WHITE); }
    }
    if (dogTime>0) {
      int x=(int)(dogX-camera),y=(int)terrain(dogX);
      uint16_t fur=rgb(219,146,85);
      int gait=(int)(sinf(animationTime*14)*3);
      shadow(x,y,13);
      for (int i=-1;i<=1;i+=2) gameFrame.drawWideLine(x+i*9,y-8,x+i*9+gait*i,y,3,rgb(182,109,36));
      gameFrame.drawWideLine(x-12,y-14,x-18,y-22+gait,3,fur);
      gameFrame.fillRoundRect(x-13,y-18,26,12,4,fur);
      gameFrame.fillRect(x-6,y-18,8,5,rgb(255,255,255));
      gameFrame.fillCircle(x+10,y-20,7,fur);
      gameFrame.fillRoundRect(x+12,y-20,7,5,2,rgb(255,219,170));
      gameFrame.fillCircle(x+18,y-19,1,TFT_BLACK);
      gameFrame.fillTriangle(x+5,y-24,x+6,y-32,x+11,y-24,rgb(146,73,36));
      gameFrame.fillCircle(x+13,y-22,1,TFT_BLACK);
      gameFrame.fillRect(x+4,y-15,6,2,TFT_RED);
    }
    drawHazards();
    hudPanel(0,33);
    button(exitButton,"BACK");
    for (int i=0;i<maxLife;++i) heart(72+i*17,14,i<life ? rgb(255,36,85) : rgb(73,73,109));
    gameFrame.setTextColor(TFT_WHITE);
    char label[40];
    // Level progress track with a marker for the hero.
    int progress=bossActive ? 100 : (int)(playerX/(arenaStart+40)*100);
    gameFrame.drawFastHLine(170,16,110,rgb(109,109,170));
    gameFrame.fillRect(170,15,progress*110/100,3,COLOR_MINT);
    gameFrame.fillCircle(170+progress*110/100,16,3,TFT_WHITE);
    gameFrame.fillTriangle(284,10,284,22,292,16,TFT_RED);
    snprintf(label,sizeof(label),"%d%%",progress);
    gameFrame.drawRightString(label,316,12,1);
    GameGfx::gradient(0,33,320,19,rgb(73,36,109),rgb(36,36,85));
    gameFrame.drawFastHLine(0,51,320,panelEdge);
    gameFrame.setTextColor(TFT_WHITE);
    if(bossActive) {
      gameFrame.setTextColor(COLOR_GOLD);
      gameFrame.drawString(level==3 ? "KING" : level==2 ? "WARLORD" : "CHIEFTAIN",5,38,1);
      gameFrame.fillRect(66,37,102,10,TFT_BLACK);
      gameFrame.drawRect(66,37,102,10,TFT_WHITE);
      int bossFill=bossLife*100/(level==3 ? 15 : 10);
      GameGfx::gradient(67,38,bossFill,8,rgb(255,109,85),rgb(146,0,0));
      gameFrame.setTextColor(TFT_WHITE);
      snprintf(label,sizeof(label),"%d/%d %s:%ds",bossLife,level==3 ? 15 : 10,level==2 ? "SPIKE" : "RAISE",(int)ceilf(level==2 ? spikeTimer : summonTimer));
      gameFrame.drawRightString(label,315,38,1);
      if(summonFlash>0) {
        gameFrame.setTextColor(rgb(36,0,0));
        gameFrame.drawCentreString("THE DEAD RISE!",161,59,2);
        gameFrame.setTextColor(COLOR_GOLD);
        gameFrame.drawCentreString("THE DEAD RISE!",160,58,2);
      }
    } else gameFrame.drawCentreString(level==3 ? "LEVEL 3: LAVA REALM  >>" : level==2 ? "LEVEL 2: ICELAND  >>" : "LEVEL 1: GRASSLANDS  >>",160,38,1);
    if (level==2 && animationTime<6) {
      gameFrame.setTextColor(rgb(0,0,85));
      gameFrame.drawCentreString("Dodge spit. Avoid red spike marks!",160,56,1);
    }
    hudPanel(weaponBarY,240-weaponBarY);
    gameFrame.drawFastHLine(0,weaponBarY,320,panelEdge);
    gameFrame.setTextColor(TFT_WHITE);
    if (!useExtra || extra==NONE) snprintf(label,sizeof(label),"WEAPON: %s",bow ? "BOW" : "SWORD");
    else if (extra==GRENADES) snprintf(label,sizeof(label),"WEAPON: BOMBS (%d)",grenades);
    else if (extra==SPEAR) snprintf(label,sizeof(label),"WEAPON: SPEAR");
    else snprintf(label,sizeof(label),"WEAPON: DOG (%ds)",dogAvailable ? 10 : (int)ceilf(dogTime));
    gameFrame.drawCentreString(label,160,weaponBarY+3,2);
    if(stage==LEVEL_COMPLETE || stage==VICTORY || stage==DEFEAT) {
      GameGfx::darkenRect(0,52,320,weaponBarY-52);
      GameGfx::gradient(26,58,268,124,rgb(73,36,109),panel);
      gameFrame.drawRoundRect(24,56,272,128,10,stage==DEFEAT ? TFT_RED : COLOR_GOLD);
      gameFrame.drawRoundRect(25,57,270,126,9,panelEdge);
      gameFrame.setTextColor(stage==DEFEAT ? rgb(255,73,85) : COLOR_GOLD);
      gameFrame.drawCentreString(stage==DEFEAT ? "YOU FELL!" : stage==LEVEL_COMPLETE ? "LEVEL COMPLETE!" : "KING DEFEATED!",160,69,4);
      gameFrame.setTextColor(TFT_WHITE);
      if (stage==VICTORY) {
        GameGfx::glow(47,104,14,COLOR_GOLD);
        gameFrame.fillRect(39,94,16,12,COLOR_GOLD);
        gameFrame.fillRect(45,106,4,7,COLOR_GOLD);
        gameFrame.fillRect(39,113,16,3,COLOR_GOLD);
        gameFrame.drawCircle(38,99,5,COLOR_GOLD); gameFrame.drawCircle(56,99,5,COLOR_GOLD);
        gameFrame.drawFastVLine(42,96,8,TFT_WHITE);
        snprintf(label,sizeof(label),"Trophy + crown unlocked!");
      } else snprintf(label,sizeof(label),"%d zombies defeated",kills);
      gameFrame.drawCentreString(label,160,102,2);
      button(retryButton,stage==LEVEL_COMPLETE ? (level==1 ? "NEXT: ICELAND" : "NEXT: LAVA REALM") : stage==VICTORY ? "PLAY AGAIN" : "RETRY LEVEL");
      button(menuButton,"CHOOSE HERO / WEAPON");
    }
  }
  UiNavigation::draw();
  gameFrame.pushSprite(0,0);
}
} // namespace ZombieCrash

void beginZombieCrash() {
  GameAudio::stop();
  GameAudio::play(GameAudio::Click);
  gameState=ZOMBIE_CRASH;
  ZombieCrash::loadRewards();
  ZombieCrash::stage=ZombieCrash::CHARACTER_SELECT;
  ZombieCrash::pendingAttack=false;
  ZombieCrash::render(false,0,0,0);
}

void updateZombieCrash(uint32_t now, bool touching, bool tapped, uint16_t x, uint16_t y, uint8_t buttons = 0, uint8_t pressed = 0) {
  using namespace ZombieCrash;
  if(tapped && exitButton.contains(x,y)) {
    if(stage==WEAPON_SELECT) { stage=CHARACTER_SELECT; render(false,0,0,0); }
    else drawGameList();
    return;
  }
  if (stage==ARMORY) {
    if (tapped) for (int i=0;i<3;++i) if (armoryChoices[i].contains(x,y)) {
      extra=(ExtraWeapon)(i+1); grenades=extra==GRENADES ? 5 : 0; dogAvailable=extra==DOG;
      armoryVisited=true; useExtra=false; stage=ACTIVE_LEVEL; lastFrame=now;
      render(false,0,0,0); return;
    }
    return;
  }
  if (stage==CHARACTER_SELECT && crownUnlocked && tapped && outfitButton.contains(x,y)) {
    wearCrown=!wearCrown; render(false,0,0,0); return;
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
      int nextLevel=stage==LEVEL_COMPLETE ? level+1 : stage==VICTORY ? 1 : level;
      startLevel(now,nextLevel); render(false,0,0,0);
    }
    else if(tapped && menuButton.contains(x,y)) beginZombieCrash();
    return;
  }
  if (extra!=NONE && (pressed & 0x08)) useExtra=!useExtra;
  // Preserve short A taps that arrive between rendering frames.
  if(pressed & 0x04) pendingAttack=true;
  if(now-lastFrame<33) return;
  float dt=min(0.1f,(now-lastFrame)/1000.0f); lastFrame=now;
  bool left=buttons & 0x01;
  bool right=buttons & 0x02;
  int movement=(int)right-(int)left;
  step(dt,movement,pendingAttack || (buttons & 0x04));
  pendingAttack=false;
  render(touching,x,y,movement);
}
