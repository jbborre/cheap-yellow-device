# Display build and upload targets

Choose an environment for the connected board. Both use the same game code; no source edits are needed when switching boards.

| Display | Environment | Build | Upload |
| --- | --- | --- | --- |
| 4-inch ST7796S, 480×320 landscape | `st7796s` | `pio run -e st7796s` | `pio run -e st7796s -t upload` |
| 2.8-inch ILI9341, 320×240 landscape | `ili9341` | `pio run -e ili9341` | `pio run -e ili9341 -t upload` |
| Alternate dual-USB 2.8-inch CYD | `cyd2usb` | `pio run -e cyd2usb` | `pio run -e cyd2usb -t upload` |

In VS Code, open **PlatformIO → Project Tasks**, expand the desired environment, and choose **Build** or **Upload**. The default toolbar target is controlled by `default_envs` in `platformio.ini`, currently `st7796s`. Set it to `ili9341` if that is the board you normally use. `cyd` remains an alias for `ili9341`.

If both boards are plugged in, select the destination explicitly, for example:

```sh
pio run -e ili9341 -t upload --upload-port /dev/cu.usbserial-10
```

Use the actual port reported by `pio device list`. To compile both primary targets without uploading: `pio run -e st7796s -e ili9341`.

The 320×240 game canvas displays at native size on ILI9341 and scales to fill ST7796S. Rendering and touch coordinates use the selected panel dimensions automatically. Neither target requires PSRAM.

Touch calibration runs on the first boot after this update. Tap both crosshairs. Calibration records the physical screen dimensions and runs again if those dimensions change.

## Wiring assumptions

ESP32-32E identifies the processor module, not the display board's pinout. The profiles use the following wiring with XPT2046 resistive touch:

| Signal | 4-inch ST7796S | 2.8-inch ILI9341 |
| --- | --- | --- |
| Display MOSI / MISO / SCLK | 13 / 12 / 14 | 13 / 12 / 14 |
| Display CS / DC | 15 / 2 | 15 / 2 |
| Display reset | -1 (no GPIO reset) | -1 (no GPIO reset) |
| Backlight, active high | 27 | 21 |
| Touch MOSI / MISO / SCLK | 13 / 12 / 14 (shared SPI) | 32 / 39 / 25 (separate VSPI) |
| Touch CS / IRQ | 33 / 36 | 33 / 36 |
| Display SPI clock | 40 MHz | 55 MHz |

If the board uses different wiring, update the display flags in `platformio.ini` and the touch configuration near the top of `auggie_game_saucer.ino` to match its schematic. A capacitive touch controller needs a different touch implementation. SPI runs at 40 MHz for the ST7796S target.

PlatformIO supplies the driver configuration through build flags; `include/User_Setup.h` is not used by these builds. Existing ILI9341 hardware can still use `pio run -e cyd` or `pio run -e cyd2usb`.

A completely dark screen with the earlier ST7796S build can be caused by its old GPIO21 backlight setting. Rebuild and upload the corrected `st7796s` environment. The serial monitor at 115200 baud prints the selected display pins, touch bus, calibration stage, and menu-ready status. The original `cyd` and `cyd2usb` targets retain GPIO21 and the separate touch bus.

## Refresh performance

The renderer uses exact pixel comparisons to update only changed regions, including the erased positions of moving objects. Up to 30 bands replace the previous 320 individual scanline writes. RGB332-to-RGB565 conversion and coordinate maps are precomputed. Two DMA buffers let the CPU prepare the next band while SPI sends the current one; all transfers finish before the touchscreen reuses the shared bus.

This adds a 76,800-byte previous-frame buffer and two 10,560-byte DMA buffers. If those allocations fail, rendering falls back to full updates or synchronous band transfers. Startup serial output reports which optimizations are enabled. Games target approximately 30 updates per second; the main loop yields for 1 ms instead of adding a 10 ms delay to each frame.

The SPI clock remains at 40 MHz. A full 480×320 RGB565 transfer still needs at least 61.44 ms at that speed, so large scrolling scenes cannot reach 30 fps merely through DMA. These changes reduce transfer overhead and unchanged pixel traffic; they do not guarantee tear-free refresh. Hardware synchronization would need access to the panel's TE signal, which is not configured on this board. See [Espressif's tearing explanation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/lcd_screen_tearing.html).
