# Game art

Pre-rendered images for **Black Knight** and **Road Trip Rally**. The games draw
these instead of building every character and building out of circles and lines.

```
art/blackknight/*.png   fighters, the shipwreck and the city street
art/roadtrip/*.png      sky panoramas, roadside scenery and cars
art/manifest.json       each image's C name and anchor point
tools/art/              the scripts that draw and convert the art
include/art/*.h         generated C headers the games include (don't edit)
```

## Changing the art

1. Edit or replace a PNG in `art/` with any paint program. [Aseprite](https://www.aseprite.org/)
   and [Piskel](https://www.piskelapp.com/) are good for pixel art.
2. Convert the PNGs to headers, then build and upload as usual:

   ```sh
   python3 -m pip install -r tools/art/requirements.txt   # once
   python3 tools/art/png2header.py
   ```

Keep the same file name and roughly the same size. If you change where a
character's feet are, update that image's `anchor` in `art/manifest.json`
(the pixel that sits on the ground, counted from the top-left corner).

`python3 tools/art/generate.py` redraws all of the original art from code. It
**overwrites** the PNGs, so don't run it after hand-editing images you want to keep.

## Rules for images

- **Colors:** the screen shows 256 fixed colors (3 bits red, 3 green, 2 blue).
  Other colors are snapped to the nearest one, so soft gradients band. Flat
  colors and dithering look best.
- **Transparency:** pixels less than half opaque are transparent.
- **Facing:** characters face right. The game mirrors them to face left.
- **Recolorable parts:** car bodies and neon sign tubes are painted in four
  marker colors that the game replaces with each car's color. Use exactly these
  RGB values, darkest to lightest: `(109,0,85)`, `(182,0,170)`, `(255,0,255)`,
  `(255,73,255)`. Car tail lights use `(0,146,0)` (dim) and `(0,255,0)` (bright)
  so they can glow when braking. Avoid these colors anywhere else on those images.
- **Wide strips** (skies, deck, street, railing) repeat sideways as the screen
  scrolls, so their left and right edges should line up. They can be at most 512
  pixels wide.

## Flash space

All the art takes about 740 KB of the ESP32's flash. The whole program,
art included, is about 1.3 MB of the 1.9 MB available.
