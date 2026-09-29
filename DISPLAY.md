# ST7796S display

The default PlatformIO environment is `st7796s`, for a 320×480 SPI ST7796S panel on an ESP32. The firmware rotates the panel to 480×320 landscape and scales all menus, games, and touch targets to fill it. The original 320×240 game canvas stays in memory. The renderer compares it with the last displayed frame, skips unchanged regions, and scales changed eight-row bands into two DMA buffers. It does not require PSRAM.

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

## Refresh performance

The renderer uses exact pixel comparisons to update only changed regions, including the erased positions of moving objects. Up to 30 bands replace the previous 320 individual scanline writes. RGB332-to-RGB565 conversion and coordinate maps are precomputed. Two DMA buffers let the CPU prepare the next band while SPI sends the current one; all transfers finish before the touchscreen reuses the shared bus.

This adds a 76,800-byte previous-frame buffer and two 10,560-byte DMA buffers. If those allocations fail, rendering falls back to full updates or synchronous band transfers. Startup serial output reports which optimizations are enabled. Games target approximately 30 updates per second; the main loop yields for 1 ms instead of adding a 10 ms delay to each frame.

The SPI clock remains at 40 MHz. A full 480×320 RGB565 transfer still needs at least 61.44 ms at that speed, so large scrolling scenes cannot reach 30 fps merely through DMA. These changes reduce transfer overhead and unchanged pixel traffic; they do not guarantee tear-free refresh. Hardware synchronization would need access to the panel's TE signal, which is not configured on this board. See [Espressif's tearing explanation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/lcd_screen_tearing.html).
