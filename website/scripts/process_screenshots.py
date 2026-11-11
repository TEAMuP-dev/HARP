"""Cuts the images of the website out of screenshots. See docs/WEBSITE.md.

Window captures (Cmd+Shift+4, then Space) are published as captured, with the shadow macOS draws
around the window as transparency. `window` cuts a window out of a full-screen capture, with the
32 px corner radius of a macOS window at Retina scale, and surrounds it with the same shadow,
fitted to a window capture: a Gaussian blur of the window shape, offset 30 px down, 45% black.
`crop` cuts a detail, such as a card or a track, out of an image, or re-encodes the whole image,
which makes a capture about a third smaller, `stack` puts images one above another, and
`annotate` draws outlines and tags over an image. `measure` prints the rows and
columns spanned by the longest runs of a color in a capture, which locates a title bar, and with
it the edges of a window: the HARP title bar is white, the inactive title bar of REAPER is
(230, 230, 230), and that of Brave is (224, 224, 225).

A box is left,top,right,bottom, with the right and bottom edges exclusive, and every coordinate is
relative to the opaque part of the image: the window in a window capture, or the whole of a
full-screen capture.

    process_screenshots.py measure <capture> [--color 255,255,255] [--tol 2] [--min-run 1500]
    process_screenshots.py window <capture> <output> --box L,T,R,B [--box L,T,R,B ...] [--no-shadow]
    process_screenshots.py crop <image> <output> [--box L,T,R,B]
    process_screenshots.py stack <output> <image> [<image> ...]
    process_screenshots.py annotate <image> <output> [--outline L,T,R,B ...] [--tag X,Y,Text ...]
                                    [--color "#7fb8e6"] [--font <TrueType file>]

Requires Pillow, and NumPy for `measure`.
"""

import argparse
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw, ImageFilter, ImageFont


RADIUS = 32  # macOS window corners at 2x
MARGIN = (112, 76, 112, 148)  # left, top, right, bottom: how far the shadow reaches
SHADOW = (36, 30, 0.45)  # sigma, downward offset, opacity

# For annotate: the stroke and corner radius of an outline, the padding and corner radius of a tag,
# the size of its text, and the fonts to try for it, by platform
STROKE, OUTLINE_RADIUS = 5, 10
PAD, TAG_RADIUS, TEXT_SIZE = 10, 8, 36
INK = (28, 28, 28)
FONTS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
]


def load(path):
    return Image.open(path).convert("RGBA")


def save(im, path):
    """Writes the image, without an alpha channel if nothing in it is transparent."""
    if im.getchannel("A").getextrema()[0] == 255:
        im = im.convert("RGB")
    im.save(path, optimize=True)
    print(f"{path}: {im.size[0]} x {im.size[1]}, {Path(path).stat().st_size // 1024} KB")


def origin(im):
    """The top left of the opaque part of the image: the window in a window capture."""
    return im.getchannel("A").point(lambda v: 255 if v == 255 else 0).getbbox()[:2]


def box(text):
    left, top, right, bottom = (int(v) for v in text.split(","))
    return left, top, right, bottom


def offset(b, dx, dy):
    return b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy


# --- window ---------------------------------------------------------------------------------------


def rounded_mask(size, rects, radius=RADIUS, scale=4):
    """An antialiased mask that is opaque inside the union of the rounded rects."""
    w, h = size
    m = Image.new("L", (w * scale, h * scale), 0)
    d = ImageDraw.Draw(m)
    for left, top, right, bottom in rects:
        d.rounded_rectangle(
            (left * scale, top * scale, right * scale - 1, bottom * scale - 1),
            radius=radius * scale,
            fill=255,
        )
    return m.resize((w, h), Image.LANCZOS)


def with_shadow(win):
    """The window on a transparent canvas, with the shadow of a window capture around it."""
    ml, mt, mr, mb = MARGIN
    sigma, dy, opacity = SHADOW
    w, h = win.size
    size = (w + ml + mr, h + mt + mb)
    shape = Image.new("L", size, 0)
    shape.paste(win.getchannel("A"), (ml, mt + dy))
    alpha = shape.filter(ImageFilter.GaussianBlur(sigma)).point(lambda v: round(v * opacity))
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.putalpha(alpha)
    out.alpha_composite(win, (ml, mt))
    return out


def window(args):
    """The union of the boxes cut out of the capture, transparent elsewhere, with the shadow."""
    im = load(args.capture)
    ox, oy = origin(im)
    boxes = [offset(b, ox, oy) for b in args.box]
    left = min(b[0] for b in boxes)
    top = min(b[1] for b in boxes)
    right = max(b[2] for b in boxes)
    bottom = max(b[3] for b in boxes)
    out = im.crop((left, top, right, bottom))
    out.putalpha(rounded_mask(out.size, [offset(b, -left, -top) for b in boxes]))
    save(out if args.no_shadow else with_shadow(out), args.output)


# --- crop and stack -------------------------------------------------------------------------------


def crop(args):
    """The box cut out of the image, or the whole image re-encoded, which shrinks a capture."""
    im = load(args.image)
    save(im.crop(offset(args.box, *origin(im))) if args.box else im, args.output)


def stack(args):
    """The images one above another, left aligned, on the color of the first top left pixel."""
    ims = [load(p) for p in args.image]
    out = Image.new(
        "RGBA",
        (max(im.size[0] for im in ims), sum(im.size[1] for im in ims)),
        ims[0].getpixel((0, 0)),
    )
    y = 0
    for im in ims:
        out.paste(im, (0, y))
        y += im.size[1]
    save(out, args.output)


# --- annotate -------------------------------------------------------------------------------------


def load_font(path):
    for p in [path] if path else FONTS:
        if Path(p).exists():
            return ImageFont.truetype(p, TEXT_SIZE)
    return ImageFont.load_default(TEXT_SIZE)


def annotate(args):
    im = load(args.image)
    ox, oy = origin(im)
    color = ImageColor.getrgb(args.color)
    font = load_font(args.font)
    draw = ImageDraw.Draw(im)
    for b in args.outline:
        left, top, right, bottom = offset(b, ox, oy)
        inset = 3
        draw.rounded_rectangle(
            (left + inset, top + inset, right - 1 - inset, bottom - 1 - inset),
            radius=OUTLINE_RADIUS,
            outline=color,
            width=STROKE,
        )
    for text in args.tag:
        x, y, label = text.split(",", 2)
        x, y = int(x) + ox, int(y) + oy
        left, top, right, bottom = font.getbbox(label)
        draw.rounded_rectangle(
            (x, y, x + right - left + 2 * PAD, y + bottom - top + 2 * PAD),
            radius=TAG_RADIUS,
            fill=color,
        )
        draw.text((x + PAD - left, y + PAD - top), label, font=font, fill=INK)
    save(im, args.output)


# --- measure --------------------------------------------------------------------------------------


def bands(arr, color, tol, min_run):
    """Rows whose longest run of `color` is at least min_run, grouped into bands.

    Each band is (y0, y1, x0, x1): the row range and the extent of its longest run.
    """
    import numpy as np

    match = np.abs(arr - np.array(color)).max(axis=2) <= tol
    out, cur = [], None
    for y in range(arr.shape[0]):
        row = match[y]
        best = (0, 0, 0)
        if row.any():
            d = np.diff(np.concatenate(([0], row.astype(int), [0])))
            starts, ends = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
            i = int(np.argmax(ends - starts))
            best = (int(ends[i] - starts[i]), int(starts[i]), int(ends[i]))
        if best[0] >= min_run:
            if cur and y == cur[1] + 1:
                cur[1] = y
                if best[0] > cur[4]:
                    cur[2], cur[3], cur[4] = best[1], best[2], best[0]
            else:
                if cur:
                    out.append(tuple(cur[:4]))
                cur = [y, y, best[1], best[2], best[0]]
    if cur:
        out.append(tuple(cur[:4]))
    return out


def measure(args):
    import numpy as np

    color = tuple(int(v) for v in args.color.split(","))
    arr = np.asarray(Image.open(args.capture).convert("RGB")).astype(int)
    print(f"{args.capture}: runs of {color} within {args.tol}, at least {args.min_run} px wide")
    for y0, y1, x0, x1 in bands(arr, color, args.tol, args.min_run):
        print(
            f"  rows {y0:5d}-{y1:5d} ({y1 - y0 + 1:4d} tall)"
            f"  cols {x0:5d}-{x1:5d} ({x1 - x0:4d} wide)"
        )


# --- Command line ---------------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("measure", help="print where a color runs across a capture")
    p.add_argument("capture")
    p.add_argument("--color", default="255,255,255", help="R,G,B, default %(default)s")
    p.add_argument("--tol", type=int, default=2, help="per channel, default %(default)s")
    p.add_argument("--min-run", type=int, default=1500, help="pixels, default %(default)s")
    p.set_defaults(run=measure)

    p = sub.add_parser("window", help="cut windows out of a full-screen capture, with a shadow")
    p.add_argument("capture")
    p.add_argument("output")
    p.add_argument("--box", type=box, action="append", required=True, help="L,T,R,B of a window")
    p.add_argument("--no-shadow", action="store_true")
    p.set_defaults(run=window)

    p = sub.add_parser("crop", help="cut a detail out of an image, or re-encode the image")
    p.add_argument("image")
    p.add_argument("output")
    p.add_argument("--box", type=box, help="L,T,R,B, the whole image without")
    p.set_defaults(run=crop)

    p = sub.add_parser("stack", help="put images one above another")
    p.add_argument("output")
    p.add_argument("image", nargs="+")
    p.set_defaults(run=stack)

    p = sub.add_parser("annotate", help="draw outlines and tags over an image")
    p.add_argument("image")
    p.add_argument("output")
    p.add_argument("--outline", type=box, action="append", default=[], help="L,T,R,B")
    p.add_argument("--tag", action="append", default=[], help="X,Y,Text of a tag")
    p.add_argument("--color", default="#7fb8e6", help="of the outlines and tags, default HARP blue")
    p.add_argument("--font", help="a TrueType file for the tags")
    p.set_defaults(run=annotate)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
