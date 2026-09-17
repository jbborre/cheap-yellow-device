#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // TFT instance
}

void loop() {
  uint16_t x = 0, y = 0;    // Variables to store touch coordinates

  // Check for touch
  if (tft.getTouch(&x, &y)) {
    if (btn.contains(x, y)) {
      tft.fillScreen(TFT_GREEN);
      delay(500);
      tft.fillScreen(TFT_BLACK);
      btn.drawButton();
    }
  }

  delay(50); // Small delay to debounce
}