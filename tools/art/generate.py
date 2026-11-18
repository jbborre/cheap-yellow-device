"""Generate all pre-rendered game art into art/ (PNGs plus art/manifest.json).

Run from the project root:
    python3 tools/art/generate.py      # draw the art (needs Pillow and numpy)
    python3 tools/art/png2header.py    # convert it to include/art/*.h

To use hand-drawn art instead, edit or replace a PNG in art/ and run only
png2header.py. Running generate.py again overwrites the PNGs it makes.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import chars  # noqa: E402
import draw  # noqa: E402
import scenes  # noqa: E402

try:
    import roadtrip  # noqa: E402
except ImportError:
    roadtrip = None

if __name__ == "__main__":
    chars.generate()
    scenes.generate()
    if roadtrip:
        roadtrip.generate()
    draw.write_manifest()
    print("Art written to art/")
