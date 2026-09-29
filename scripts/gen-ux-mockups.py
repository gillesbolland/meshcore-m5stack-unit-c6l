#!/usr/bin/env python3
"""Render 1-bit OLED docs mockups with Adafruit Tom Thumb (no antialiasing).

Matches MeshCore C6L TinyListDraw layout: 64x48, ROW_H=8, FONT_BASELINE=5.
Output is nearest-neighbor scaled; ON pixels = OLED cyan, OFF = black.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image

W, H = 64, 48
ROW_H = 8
MAX_ROWS = H // ROW_H
FONT_BASELINE = 5
TEXT_PAD = 2
TRI_H = 5
SCALE = 8
ON = (0x00, 0xE8, 0xE0)  # match docs accent / OLED cyan
OFF = (0x00, 0x00, 0x00)


def parse_tomthumb(path: Path):
    text = path.read_text()
    # Strip C comments so "/* 0x20 space */" does not pollute byte lists.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    bm = re.search(
        r"TomThumbBitmaps\[\][^=]*=\s*\{(.*?)\};", text, re.S
    )
    if not bm:
        raise SystemExit(f"no TomThumbBitmaps in {path}")
    bm_body = bm.group(1).split("#if")[0]
    bitmaps = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+", bm_body)]

    gl = re.search(r"TomThumbGlyphs\[\][^=]*=\s*\{(.*?)\};", text, re.S)
    if not gl:
        raise SystemExit(f"no TomThumbGlyphs in {path}")
    gl_body = gl.group(1).split("#if")[0]
    glyphs = []
    for m in re.finditer(
        r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}",
        gl_body,
    ):
        glyphs.append(tuple(int(g) for g in m.groups()))
    if len(glyphs) < 95:
        raise SystemExit(f"expected >=95 ASCII glyphs, got {len(glyphs)}")
    # First bitmap byte must be space (0x00); exclam follows (0xE8)
    if bitmaps[0] != 0x00 or bitmaps[1] != 0xE8:
        raise SystemExit(
            f"unexpected TomThumb bitmap start: {bitmaps[:4]!r} (comment pollution?)"
        )
    return bitmaps, glyphs[:95]


class FB:
    def __init__(self):
        self.pix = [0] * (W * H)
        self.color = 1
        self.cx = 0
        self.cy = 0

    def clear(self):
        self.pix = [0] * (W * H)

    def set_color(self, c: int):
        self.color = 1 if c else 0

    def pixel(self, x: int, y: int):
        if 0 <= x < W and 0 <= y < H:
            self.pix[y * W + x] = self.color

    def fill_rect(self, x: int, y: int, w: int, h: int):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.pixel(xx, yy)

    def draw_rect(self, x: int, y: int, w: int, h: int):
        for xx in range(x, x + w):
            self.pixel(xx, y)
            self.pixel(xx, y + h - 1)
        for yy in range(y, y + h):
            self.pixel(x, yy)
            self.pixel(x + w - 1, yy)

    def set_cursor(self, x: int, y: int):
        self.cx, self.cy = x, y


class Font:
    def __init__(self, bitmaps, glyphs):
        self.bitmaps = bitmaps
        self.glyphs = glyphs  # index 0 = 0x20
        self.first = 0x20
        self.last = 0x7E
        self.y_advance = 6

    def glyph(self, ch: int):
        if ch < self.first or ch > self.last:
            ch = ord("?")
        return self.glyphs[ch - self.first]

    def text_width(self, s: str) -> int:
        w = 0
        for c in s:
            w += self.glyph(ord(c))[3]  # xAdvance
        return w

    def draw_char(self, fb: FB, ch: int):
        bitmapOffset, w, h, xAdvance, xOffset, yOffset = self.glyph(ch)
        xx = fb.cx + xOffset
        yy = fb.cy + yOffset
        bo = bitmapOffset
        bits = 0
        bit = 0
        for j in range(h):
            for i in range(w):
                if bit == 0:
                    bits = self.bitmaps[bo]
                    bo += 1
                    bit = 0x80
                if bits & bit:
                    fb.pixel(xx + i, yy + j)
                bit >>= 1
        fb.cx += xAdvance

    def print(self, fb: FB, s: str):
        for c in s:
            self.draw_char(fb, ord(c))


def fill_iso_triangle(fb: FB, tip_x: int, top_y: int, h: int, point_left: bool):
    mid = h // 2
    for row in range(h):
        half = row if row <= mid else (h - 1 - row)
        width = half + 1
        x = tip_x if point_left else (tip_x - half)
        fb.fill_rect(x, top_y + row, width, 1)


def draw_row(fb: FB, font: Font, text: str, row_idx: int, selected: bool):
    y = row_idx * ROW_H
    ty = y + FONT_BASELINE
    if selected:
        fb.set_color(1)
        fb.fill_rect(0, y, W, ROW_H)
        fb.set_color(0)
        fb.set_cursor(TEXT_PAD, ty)
        font.print(fb, text)
        fb.set_color(1)
    else:
        fb.set_color(1)
        fb.set_cursor(TEXT_PAD, ty)
        font.print(fb, text)


def draw_unread_row(fb: FB, font: Font, text: str, row_idx: int, selected: bool):
    y = row_idx * ROW_H
    ty = y + FONT_BASELINE
    tri_y = y + (ROW_H - TRI_H) // 2
    tip_x = TEXT_PAD + (TRI_H // 2)
    text_x = TEXT_PAD + TRI_H + 2
    if selected:
        fb.set_color(1)
        fb.fill_rect(0, y, W, ROW_H)
        fb.set_color(0)
        fill_iso_triangle(fb, tip_x, tri_y, TRI_H, False)
        fb.set_cursor(text_x, ty)
        font.print(fb, text)
        fb.set_color(1)
    else:
        fb.set_color(1)
        fill_iso_triangle(fb, tip_x, tri_y, TRI_H, False)
        fb.set_cursor(text_x, ty)
        font.print(fb, text)


def draw_button_row(fb: FB, font: Font, label: str, row_idx: int, selected: bool, point_left: bool):
    y = row_idx * ROW_H
    bx, by = 1, y + 1
    bw = W - 2 - 2
    bh = ROW_H - 2
    tw = font.text_width(label)
    content_w = TRI_H + 2 + tw
    content_x = bx + (bw - content_w) // 2
    if content_x < bx + 2:
        content_x = bx + 2
    tri_y = by + (bh - TRI_H) // 2
    text_y = by + FONT_BASELINE
    if selected:
        fb.set_color(1)
        fb.fill_rect(bx, by, bw, bh)
        fb.set_color(0)
    else:
        fb.set_color(1)
        fb.draw_rect(bx, by, bw, bh)
    if point_left:
        tip_x = content_x
        text_x = content_x + TRI_H + 2
    else:
        tip_x = content_x + (TRI_H // 2)
        text_x = content_x + TRI_H + 2
    fill_iso_triangle(fb, tip_x, tri_y, TRI_H, point_left)
    fb.set_cursor(text_x, text_y)
    font.print(fb, label)
    fb.set_color(1)


def draw_modal_box(fb: FB, bx: int, by: int, bw: int, bh: int):
    fb.set_color(0)
    fb.fill_rect(bx, by, bw, bh)
    fb.set_color(1)
    fb.fill_rect(bx + 2, by + bh, bw, 2)
    fb.fill_rect(bx + bw, by + 2, 2, bh)
    fb.draw_rect(bx, by, bw, bh)


def draw_pick_list(fb: FB, font: Font, items: list[str], cursor: int):
    n = min(len(items), 4)
    box_h = n * ROW_H + 4
    if box_h > H - 4:
        box_h = H - 4
    box_w = W - 6
    bx = 2
    by = (H - box_h) // 2
    if by < 1:
        by = 1
    draw_modal_box(fb, bx, by, box_w, box_h)
    for i in range(n):
        sel = i == cursor
        t = items[i]
        tw = font.text_width(t)
        tx = bx + (box_w - tw) // 2
        if tx < bx + 2:
            tx = bx + 2
        row_y = by + 2 + i * ROW_H
        if sel:
            fb.set_color(1)
            fb.fill_rect(bx + 1, row_y, box_w - 2, ROW_H)
            fb.set_color(0)
        else:
            fb.set_color(1)
        fb.set_cursor(tx, row_y + FONT_BASELINE)
        font.print(fb, t)


def render_list(fb: FB, font: Font, items: list[str], cursor: int, styles=None):
    fb.clear()
    n = len(items)
    top = 0
    if n > MAX_ROWS:
        top = max(0, min(n - MAX_ROWS, cursor - (MAX_ROWS - 1)))
    for i in range(MAX_ROWS):
        idx = top + i
        if idx >= n:
            break
        st = styles[idx] if styles else "plain"
        sel = idx == cursor
        if st == "unread":
            draw_unread_row(fb, font, items[idx], i, sel)
        elif st == "back":
            draw_button_row(fb, font, items[idx], i, sel, True)
        else:
            draw_row(fb, font, items[idx], i, sel)
    if n > MAX_ROWS:
        thumb = max(3, H * MAX_ROWS // n)
        y = H * top // n
        fb.set_color(1)
        fb.fill_rect(W - 2, y, 2, thumb)


def render_lines(fb: FB, font: Font, lines: list[str]):
    fb.clear()
    for i, line in enumerate(lines[:MAX_ROWS]):
        draw_row(fb, font, line, i, False)


def save(fb: FB, path: Path):
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = ON if fb.pix[y * W + x] else OFF
    out = img.resize((W * SCALE, H * SCALE), Image.NEAREST)
    path.parent.mkdir(parents=True, exist_ok=True)
    out.save(path)
    print(f"wrote {path} ({out.size[0]}x{out.size[1]})")


def main():
    root = Path(__file__).resolve().parents[1]
    out_dir = root / "docs" / "assets" / "ux"
    font_candidates = [
        Path(
            "/Volumes/alpha/Projects/Developpement/openhop_modem-c6l/firmware/"
            ".pio/libdeps/m5stack_unit_c6l/Adafruit GFX Library/Fonts/TomThumb.h"
        ),
        Path.home()
        / ".platformio/packages"
        / "framework-arduinoespressif32"
        / "libraries"
        / "Adafruit_GFX"
        / "Fonts"
        / "TomThumb.h",
    ]
    font_path = next((p for p in font_candidates if p.exists()), None)
    if font_path is None:
        # search under workspace MeshCore .pio if present
        for p in Path("/Volumes/alpha/Projects/Developpement").rglob("TomThumb.h"):
            font_path = p
            break
    if font_path is None:
        raise SystemExit("TomThumb.h not found")
    bitmaps, glyphs = parse_tomthumb(font_path)
    font = Font(bitmaps, glyphs)
    fb = FB()

    # Menu (Messages selected) — matches C6L_MENU
    render_list(
        fb,
        font,
        ["Messages", "Contacts", "Nodes", "Status", "Settings"],
        0,
    )
    save(fb, out_dir / "menu.png")

    # Messages list with unread markers
    render_list(
        fb,
        font,
        ["BACK", "Alice", "Bob", "Cara", "#general"],
        1,
        styles=["back", "unread", "unread", "plain", "unread"],
    )
    save(fb, out_dir / "messages.png")

    # Status
    render_lines(
        fb,
        font,
        ["node 4f3a", "MyC6L", "3 peers", "869.5/7", "2 DM unread"],
    )
    save(fb, out_dir / "status.png")

    # Settings root
    render_list(
        fb,
        font,
        ["LoRa", "Interfaces", "Path hash", "Local advert", "Factory reset"],
        0,
    )
    save(fb, out_dir / "settings.png")

    # Mode pick (first-boot / Mode modal) — USB selected
    fb.clear()
    # faint underlay like real UI (menu peek)
    for i, t in enumerate(["Messages", "Contacts", "Nodes"]):
        draw_row(fb, font, t, i, False)
    draw_pick_list(fb, font, ["BLE", "USB", "WiFi"], 1)
    save(fb, out_dir / "mode.png")


if __name__ == "__main__":
    main()
