#pragma once
#include <stdint.h>
#include <math.h>

// Effects that write the shared 8-bit frame directly. RGB332 has only 256
// colors, so a 4x4 ordered dither turns gradients into stable fine patterns;
// unchanged pixels between frames still let the renderer skip them.
// Shadows and glows modify existing pixels for cheap translucency.
namespace GameGfx {
const uint8_t bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};

// RGB332 keeps the top 3/3/2 bits, so pick channels from 0,36,73,109,146,
// 182,219,255 (red, green) and 0,85,170,255 (blue) for exact colors.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((r&0xF8)<<8 | (g&0xFC)<<3 | b>>3);
}
inline uint16_t mix565(uint16_t a, uint16_t b, int num, int den) {
  int r=(a>>11)+(((b>>11)-(a>>11))*num)/den;
  int g=((a>>5)&63)+((((b>>5)&63)-((a>>5)&63))*num)/den;
  int bl=(a&31)+(((b&31)-(a&31))*num)/den;
  return (uint16_t)(r<<11 | g<<5 | bl);
}
inline uint16_t darker(uint16_t c) { return (c>>1)&0x7BEF; }
inline uint16_t lighter(uint16_t c) { return darker(c)+0x7BEF; }

inline uint8_t *pixels() { return static_cast<uint8_t *>(gameFrame.getPointer()); }
// Matches TFT_eSprite's RGB565-to-RGB332 conversion.
inline uint8_t pack(uint16_t c) { return (c&0xE000)>>8 | (c&0x0700)>>6 | (c&0x0018)>>3; }
inline bool clip(int &x, int &y, int &w, int &h) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x+w>GameDisplay::width) w=GameDisplay::width-x;
  if (y+h>GameDisplay::height) h=GameDisplay::height-y;
  return w>0 && h>0 && pixels();
}

// Channels in sixteenths of an RGB332 step: red/green 0..112, blue 0..48.
struct Level { int r, g, b; };
inline Level level(uint16_t c) {
  return {((c>>11)&31)*112/31, ((c>>5)&63)*112/63, (c&31)*48/31};
}
inline uint8_t dither(const Level &l, int t) {
  return ((l.r+t)>>4)<<5 | ((l.g+t)>>4)<<2 | ((l.b+t)>>4);
}
inline Level mix(const Level &a, const Level &b, int num, int den) {
  return {a.r+(b.r-a.r)*num/den, a.g+(b.g-a.g)*num/den, a.b+(b.b-a.b)*num/den};
}

// Vertical gradient from top to bottom color.
inline void gradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  const int y0=y, h0=h;
  if (!clip(x,y,w,h)) return;
  Level a=level(top), b=level(bottom);
  for (int row=y; row<y+h; ++row) {
    Level l=mix(a,b,row-y0,h0>1 ? h0-1 : 1);
    uint8_t pattern[4];
    for (int i=0;i<4;++i) pattern[i]=dither(l,bayer[row&3][i]);
    uint8_t *p=pixels()+row*GameDisplay::width;
    for (int col=x; col<x+w; ++col) p[col]=pattern[col&3];
  }
}

// Halve brightness: cast shadows, tinted glass and depth fog against dark sky.
inline uint8_t half(uint8_t p) { return (p>>1)&0x6D; }
inline void darkenSpan(int x0, int x1, int y) {
  if (y<0 || y>=GameDisplay::height || !pixels()) return;
  if (x0<0) x0=0;
  if (x1>GameDisplay::width) x1=GameDisplay::width;
  uint8_t *p=pixels()+y*GameDisplay::width;
  for (int x=x0;x<x1;++x) p[x]=half(p[x]);
}
inline void darkenRect(int x, int y, int w, int h) {
  if (!clip(x,y,w,h)) return;
  for (int row=y;row<y+h;++row) darkenSpan(x,x+w,row);
}
inline void darkenEllipse(int cx, int cy, int rx, int ry) {
  if (rx<1 || ry<1) return;
  for (int dy=-ry; dy<=ry; ++dy) {
    int span=(int)(rx*sqrtf(1.0f-(float)(dy*dy)/(ry*ry)));
    darkenSpan(cx-span,cx+span+1,cy+dy);
  }
}

// Checkerboard overlay reads as a 50% blend: haze, glows and light beams.
inline void screenSpan(int x0, int x1, int y, uint8_t c) {
  if (y<0 || y>=GameDisplay::height || !pixels()) return;
  if (x0<0) x0=0;
  if (x1>GameDisplay::width) x1=GameDisplay::width;
  uint8_t *p=pixels()+y*GameDisplay::width;
  for (int x=x0+((x0+y)&1);x<x1;x+=2) p[x]=c;
}
inline void screenRect(int x, int y, int w, int h, uint16_t color) {
  if (!clip(x,y,w,h)) return;
  for (int row=y;row<y+h;++row) screenSpan(x,x+w,row,pack(color));
}
// Dithered coverage fades from 75% at the center to nothing at the radius.
inline void glow(int cx, int cy, int r, uint16_t color) {
  if (r<1 || !pixels()) return;
  const uint8_t c=pack(color);
  const int r2=r*r;
  for (int y=max(0,cy-r); y<=min(GameDisplay::height-1,cy+r); ++y) {
    uint8_t *p=pixels()+y*GameDisplay::width;
    const int dy2=(y-cy)*(y-cy);
    for (int x=max(0,cx-r); x<=min(GameDisplay::width-1,cx+r); ++x) {
      int d2=dy2+(x-cx)*(x-cx);
      if (d2<r2 && bayer[y&3][x&3] < 12*(r2-d2)/r2) p[x]=c;
    }
  }
}
} // namespace GameGfx
