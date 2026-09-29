#pragma once
#include <stdint.h>

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

#ifndef GAME_DISPLAY_HOST_TEST
// Keep the 76,800-byte RGB332 canvas; scaling needs only one extra scanline.
// This avoids a 153,600-byte contiguous allocation on ESP32 boards without PSRAM.
class Frame : public TFT_eSprite {
 public:
  explicit Frame(TFT_eSPI *panel) : TFT_eSprite(panel), panel_(panel) {}
  void pushSprite(int32_t x, int32_t y) {
    if (!getPointer()) return;
    if (panel_->width()==GameDisplay::width && panel_->height()==GameDisplay::height) {
      TFT_eSprite::pushSprite(x,y);
      return;
    }
    const int panelWidth=panel_->width(), panelHeight=panel_->height();
    if (panelWidth>maxPanelWidth) return;
    const auto *pixels=static_cast<const uint8_t *>(getPointer());
    uint8_t row[maxPanelWidth];
    int previousRow=-1;
    panel_->startWrite();
    for (int py=0; py<panelHeight; ++py) {
      int sy=logicalCoordinate(py,panelHeight,GameDisplay::height);
      if (sy!=previousRow) {
        scaleRow(pixels+sy*GameDisplay::width,row,panelWidth);
        previousRow=sy;
      }
      panel_->pushImage(x,y+py,panelWidth,1,row,true);
    }
    panel_->endWrite();
  }
 private:
  TFT_eSPI *panel_;
};
#endif
} // namespace GameDisplay
