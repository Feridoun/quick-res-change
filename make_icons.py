"""Generate icon_base.ico (dim) and icon_chosen.ico (bright).

A monitor with a gradient frame, a dark screen and a white double-headed
diagonal "resize" arrow. Each size is drawn separately at 8x and downsampled,
so the 16/20/24 px tray sizes stay crisp. Requires Pillow:

    python make_icons.py
"""
import math

from PIL import Image, ImageDraw

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SS = 8  # supersampling factor

THEMES = {
    "icon_chosen.ico": dict(frame=((34, 211, 238), (124, 58, 237)),   # cyan -> violet
                            screen=(15, 23, 42), arrow=(255, 255, 255)),
    "icon_base.ico":   dict(frame=((156, 163, 175), (100, 116, 139)),  # grey -> slate
                            screen=(51, 65, 85), arrow=(203, 213, 225)),
}


def gradient(w, h, c0, c1):
    """Diagonal gradient from top-left (c0) to bottom-right (c1)."""
    g = Image.new("RGB", (w, h))
    px = g.load()
    for y in range(h):
        for x in range(w):
            t = (x + y) / (w + h - 2)
            px[x, y] = tuple(round(a + (b - a) * t) for a, b in zip(c0, c1))
    return g


def draw(size, theme):
    n = size * SS
    u = n / 32  # work on a 32-unit grid

    # Shape mask for the frame + stand, filled with the gradient.
    mask = Image.new("L", (n, n), 0)
    m = ImageDraw.Draw(mask)
    m.rounded_rectangle([1 * u, 3 * u, 31 * u, 24 * u], radius=3.5 * u, fill=255)
    m.rectangle([13 * u, 23 * u, 19 * u, 27.5 * u], fill=255)
    m.rounded_rectangle([8 * u, 26.5 * u, 24 * u, 30 * u], radius=1.75 * u, fill=255)

    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    img.paste(gradient(n, n, *theme["frame"]), (0, 0), mask)

    d = ImageDraw.Draw(img)
    # Screen: thinner bezel at small sizes would vanish, so keep it ~2.5 units.
    d.rounded_rectangle([3.5 * u, 5.5 * u, 28.5 * u, 21.5 * u],
                        radius=1.5 * u, fill=theme["screen"])

    # Double-headed arrow from bottom-left to top-right of the screen.
    a = theme["arrow"]
    x0, y0, x1, y1 = 8.5 * u, 18 * u, 23.5 * u, 9 * u
    w = 2.2 * u
    head = 5.2 * u
    # Shorten the shaft so its square ends hide under the heads.
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    px, py = -uy, ux
    k = head * 0.6
    d.line([(x0 + ux * k, y0 + uy * k), (x1 - ux * k, y1 - uy * k)], fill=a, width=round(w))
    for (tx, ty, sx) in ((x1, y1, 1), (x0, y0, -1)):
        bx, by = tx - sx * ux * head, ty - sx * uy * head
        hw = head * 0.62
        d.polygon([(tx, ty), (bx + px * hw, by + py * hw), (bx - px * hw, by - py * hw)], fill=a)

    return img.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    for name, theme in THEMES.items():
        frames = [draw(s, theme) for s in SIZES]
        frames[-1].save(name, format="ICO", sizes=[(s, s) for s in SIZES],
                        append_images=frames[:-1])
        print("wrote", name)
