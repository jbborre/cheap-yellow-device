#pragma once
#include <stdint.h>
#include <string.h>
#ifndef GAME_DISPLAY_HOST_TEST
#include <esp_heap_caps.h>
#endif

// Every game and menu uses the same logical canvas. Touch input uses the exact
// inverse of the nearest-neighbor display mapping, including the last pixel.
namespace GameDisplay {
constexpr int width = 320;
constexpr int height = 240;
constexpr int maxPanelWidth = 480;
inline int logicalCoordinate(int pixel, int physicalSize, int logicalSize) {
  if (pixel < 0) pixel = 0;
  if (pixel >= physicalSize) pixel = physicalSize - 1;
  return pixel * logicalSize / physicalSize;
}
inline void scaleRow(const uint8_t *source, uint8_t *output, int panelWidth) {
  for (int x=0; x<panelWidth; ++x)
    output[x]=source[logicalCoordinate(x,panelWidth,width)];
}

// Half-open bounds in logical pixels for one eight-row band. Exact comparison
// avoids hash collisions and catches both the old and new positions of sprites.
constexpr int bandRows=8;
struct Span { int left, right; };
inline Span changedSpan(const uint8_t *current, const uint8_t *previous, int rows) {
  if (!previous) return {0,width};
  Span span={width,0};
  for (int y=0;y<rows;++y) {
    const uint8_t *a=current+y*width, *b=previous+y*width;
    if (memcmp(a,b,width)==0) continue;
    int left=0, right=width;
    while (left<right && a[left]==b[left]) ++left;
    while (right>left && a[right-1]==b[right-1]) --right;
    if (left<span.left) span.left=left;
    if (right>span.right) span.right=right;
  }
  return span;
}
inline int physicalBoundary(int logical, int physicalSize, int logicalSize) {
  return (logical*physicalSize+logicalSize-1)/logicalSize;
}
// Match TFT_eSPI's RGB332 expansion exactly, stored in SPI wire byte order.
inline uint16_t wireColor(uint8_t color) {
  const uint8_t blue[]={0,11,21,31};
  uint8_t high=(color & 0x1C)>>2 | (color & 0xC0)>>3 | (color & 0xE0);
  uint8_t low=(color & 0x1C)<<3 | blue[color & 3];
  return (uint16_t(low)<<8)|high;
}

#if !defined(GAME_DISPLAY_HOST_TEST) || defined(GAME_DISPLAY_RENDER_TEST)
class Frame : public TFT_eSprite {
 public:
  explicit Frame(TFT_eSPI *panel) : TFT_eSprite(panel), panel_(panel) {}
  // Direct writes to the panel (e.g. calibration) invalidate the saved image.
  void invalidate() { valid_=false; }
  void pushSprite(int32_t x, int32_t y) {
    if (!getPointer()) return;
    const int pw=panel_->width(), ph=panel_->height();
    if (pw>maxPanelWidth || ph>320 || x!=0 || y!=0) {
      invalidate(); TFT_eSprite::pushSprite(x,y); return;
    }
    if (!initialized_) initialize();
    const auto *pixels=static_cast<const uint8_t *>(getPointer());
    bool swap=panel_->getSwapBytes();
    panel_->setSwapBytes(false); // buffers already contain wire-order RGB565
    panel_->startWrite();
    int buffer=0;
    for (int sy=0; sy<GameDisplay::height; sy+=bandRows) {
      Span span=changedSpan(pixels+sy*GameDisplay::width,
          valid_ && previous_ ? previous_+sy*GameDisplay::width : nullptr,bandRows);
      if (span.left>=span.right) continue;
      int left=physicalBoundary(span.left,pw,GameDisplay::width);
      int right=physicalBoundary(span.right,pw,GameDisplay::width);
      int top=physicalBoundary(sy,ph,GameDisplay::height);
      int bottom=physicalBoundary(sy+bandRows,ph,GameDisplay::height);
      if (dma_) {
        // The other buffer may still be transferring while this one is filled.
        uint16_t *out=buffers_[buffer];
        for (int py=top;py<bottom;++py) {
          const uint8_t *row=pixels+yMap_[py]*GameDisplay::width;
          for (int px=left;px<right;++px) *out++=colors_[row[xMap_[px]]];
        }
        panel_->dmaWait();
        panel_->setAddrWindow(left,top,right-left,bottom-top);
        panel_->pushPixelsDMA(buffers_[buffer],(right-left)*(bottom-top));
        buffer^=1;
      } else {
        // Low-memory fallback still skips unchanged regions and streams each
        // band through a single address window instead of resetting every row.
        uint16_t row[maxPanelWidth];
        panel_->setAddrWindow(left,top,right-left,bottom-top);
        for (int py=top;py<bottom;++py) {
          const uint8_t *source=pixels+yMap_[py]*GameDisplay::width;
          for (int px=left;px<right;++px) row[px-left]=colors_[source[xMap_[px]]];
          panel_->pushPixels(row,right-left);
        }
      }
    }
    // Finish before touch reads reuse the SPI bus or the next frame is drawn.
    if (dma_) panel_->dmaWait();
    panel_->endWrite();
    panel_->setSwapBytes(swap);
    if (previous_) memcpy(previous_,pixels,GameDisplay::width*GameDisplay::height);
    valid_=true;
  }
 private:
  void initialize() {
    initialized_=true;
    for (int x=0;x<panel_->width();++x)
      xMap_[x]=logicalCoordinate(x,panel_->width(),GameDisplay::width);
    for (int y=0;y<panel_->height();++y)
      yMap_[y]=logicalCoordinate(y,panel_->height(),GameDisplay::height);
    for (int c=0;c<256;++c) colors_[c]=wireColor(c);
    previous_=static_cast<uint8_t *>(malloc(GameDisplay::width*GameDisplay::height));
    // Eight logical rows expand to at most eleven physical rows at 480x320.
    constexpr int bufferBytes=maxPanelWidth*11*sizeof(uint16_t);
    for (int i=0;i<2;++i)
      buffers_[i]=static_cast<uint16_t *>(heap_caps_malloc(bufferBytes,MALLOC_CAP_DMA));
    if (buffers_[0] && buffers_[1]) dma_=panel_->initDMA();
    if (!dma_) {
      for (auto &b:buffers_) { heap_caps_free(b); b=nullptr; }
    }
    Serial.printf("Renderer: changed regions %s, DMA %s\n",
                  previous_ ? "on" : "off (low memory)",dma_ ? "on" : "off");
  }
  TFT_eSPI *panel_;
  uint8_t *previous_=nullptr;
  uint16_t *buffers_[2]={nullptr,nullptr};
  uint16_t xMap_[maxPanelWidth], yMap_[320], colors_[256];
  bool initialized_=false, valid_=false, dma_=false;
};
#endif
} // namespace GameDisplay
