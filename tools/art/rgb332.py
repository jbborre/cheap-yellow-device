"""The display's 256 colors (RGB332, as TFT_eSPI shows them) and quantizing to them."""
import numpy as np

BLUE5 = [0, 11, 21, 31]


def palette():
    """256x3 array of the exact RGB each RGB332 index looks like on screen."""
    out = np.zeros((256, 3), dtype=np.float32)
    for c in range(256):
        r3, g3, b2 = c >> 5, (c >> 2) & 7, c & 3
        r5 = (r3 << 2) | (r3 >> 1)
        g6 = (g3 << 3) | g3
        out[c] = (r5 * 255 / 31, g6 * 255 / 63, BLUE5[b2] * 255 / 31)
    return out


PALETTE = palette()
WEIGHTS = np.array([3.0, 4.0, 2.0], dtype=np.float32)  # Rough perceptual weighting.


def nearest(rgb):
    """Index of the nearest display color for each pixel of an (..., 3) array."""
    flat = rgb.reshape(-1, 3).astype(np.float32)
    best = np.zeros(len(flat), dtype=np.uint8)
    for start in range(0, len(flat), 8192):
        chunk = flat[start:start + 8192]
        d = (((chunk[:, None, :] - PALETTE[None, :, :]) ** 2) * WEIGHTS).sum(axis=2)
        best[start:start + 8192] = d.argmin(axis=1)
    return best.reshape(rgb.shape[:-1])


BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16 - 0.47


def dithered(rgb, strength=1.0):
    """Ordered dither: smooth gradients become fine stable patterns, not bands."""
    h, w = rgb.shape[:2]
    t = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w][..., None]
    step = np.array([36.0, 36.0, 85.0], dtype=np.float32)  # Size of one RGB332 step per channel.
    return nearest(np.clip(rgb.astype(np.float32) + t * step * strength, 0, 255))


def to_rgb(index):
    return PALETTE[index].round().astype(np.uint8)
