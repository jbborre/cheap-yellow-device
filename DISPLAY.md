# ST7796S display

The default PlatformIO environment is `st7796s`, for a 320×480 SPI ST7796S panel on an ESP32. The firmware rotates the panel to 480×320 landscape and scales all menus, games, and touch targets to fill it. The original 320×240 game canvas stays in memory; scaling uses a scanline buffer and does not require PSRAM.

Build: `pio run -e st7796s`

Upload: `pio run -e st7796s -t upload`

Touch calibration runs on the first boot after this update. Tap both crosshairs. Calibration records the physical screen dimensions and runs again if those dimensions change.

## Wiring assumptions

ESP32-32E identifies the processor module, not the display board's pinout. The `st7796s` target now uses the [LCDWIKI ESP32-32E pinout](https://www.lcdwiki.com/3.5inch_ESP32-32E_Display#ESP32_Pin_Assignment) and XPT2046 resistive touch controller:

| Signal | GPIO |
| --- | --- |
| Display MOSI / MISO / SCLK | 13 / 12 / 14 |
| Display CS / DC | 15 / 2 |
| Display reset | -1 (no GPIO-controlled reset) |
| Backlight | 27, active high |
| Touch MOSI / MISO / SCLK | 13 / 12 / 14 (shared display SPI) |
| Touch CS / IRQ | 33 / 36 |

If the board uses different wiring, update the display flags in `platformio.ini` and the touch configuration near the top of `auggie_game_saucer.ino` to match its schematic. A capacitive touch controller needs a different touch implementation. SPI runs at 40 MHz for the ST7796S target.

PlatformIO supplies the driver configuration through build flags; `include/User_Setup.h` is not used by these builds. Existing ILI9341 hardware can still use `pio run -e cyd` or `pio run -e cyd2usb`.

A completely dark screen with the earlier ST7796S build can be caused by its old GPIO21 backlight setting. Rebuild and upload the corrected `st7796s` environment. The serial monitor at 115200 baud prints the selected display pins, touch bus, calibration stage, and menu-ready status. The original `cyd` and `cyd2usb` targets retain GPIO21 and the separate touch bus.
