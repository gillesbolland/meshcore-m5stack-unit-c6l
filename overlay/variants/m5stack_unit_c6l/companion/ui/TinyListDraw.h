#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

enum class TinyRowStyle : uint8_t {
  Plain = 0,
  BackBtn,      // inset chip, left triangle
  ForwardBtn,   // inset chip, right triangle (e.g. REPLY)
  Unread,       // plain row with right triangle glyph
};

class TinyListDraw {
public:
  static constexpr int W = 64;
  static constexpr int H = 48;
  static constexpr int ROW_H = 8;
  static constexpr int MAX_ROWS = H / ROW_H;  // 6
  static constexpr int TRI_H = 5;
  static constexpr int BTN_PAD_X = 1;
  static constexpr int BTN_PAD_Y = 1;
  // Tom Thumb GFXfont: cursor y is baseline (glyph yOffset ≈ -5)
  static constexpr int FONT_BASELINE = 5;
  static constexpr int TEXT_PAD = 2;

  static void clip(char* dest, size_t dest_len, const char* src, int max_chars) {
    if (!dest || dest_len == 0) return;
    if (!src) { dest[0] = 0; return; }
    int n = (int)strlen(src);
    if (n <= max_chars) {
      strncpy(dest, src, dest_len - 1);
      dest[dest_len - 1] = 0;
      return;
    }
    if (max_chars <= 1) {
      dest[0] = '~';
      dest[1] = 0;
      return;
    }
    int copy = max_chars - 1;
    if (copy >= (int)dest_len) copy = (int)dest_len - 1;
    memcpy(dest, src, copy);
    dest[copy] = '~';
    if (copy + 1 < (int)dest_len) dest[copy + 1] = 0;
    else dest[dest_len - 1] = 0;
  }

  /** Filled ISO triangle via horizontal scanlines. tip at left if point_left. */
  static void fillIsoTriangle(DisplayDriver* d, int tip_x, int top_y, int h, bool point_left) {
    if (!d || h < 1) return;
    int mid = h / 2;
    for (int row = 0; row < h; row++) {
      int half = (row <= mid) ? row : (h - 1 - row);
      int width = half + 1;
      int x = point_left ? tip_x : (tip_x - half);
      d->fillRect(x, top_y + row, width, 1);
    }
  }

  static void drawRow(DisplayDriver* d, const char* text, int row_idx, bool selected) {
    if (!d || row_idx < 0 || row_idx >= MAX_ROWS) return;
    int y = row_idx * ROW_H;
    int ty = y + FONT_BASELINE;
    if (selected) {
      d->setColor(1);
      d->fillRect(0, y, W, ROW_H);
      d->setColor(0);
      d->setCursor(TEXT_PAD, ty);
      d->print(text ? text : "");
      d->setColor(1);
    } else {
      d->setColor(1);
      d->setCursor(TEXT_PAD, ty);
      d->print(text ? text : "");
    }
  }

  /** Plain row with a right-pointing ISO triangle before the label (unread). */
  static void drawUnreadRow(DisplayDriver* d, const char* text, int row_idx, bool selected) {
    if (!d || row_idx < 0 || row_idx >= MAX_ROWS) return;
    int y = row_idx * ROW_H;
    int ty = y + FONT_BASELINE;
    int tri_y = y + (ROW_H - TRI_H) / 2;
    int tip_x = TEXT_PAD + (TRI_H / 2);
    int text_x = TEXT_PAD + TRI_H + 2;

    if (selected) {
      d->setColor(1);
      d->fillRect(0, y, W, ROW_H);
      d->setColor(0);
      fillIsoTriangle(d, tip_x, tri_y, TRI_H, false);
      d->setCursor(text_x, ty);
      d->print(text ? text : "");
      d->setColor(1);
    } else {
      d->setColor(1);
      fillIsoTriangle(d, tip_x, tri_y, TRI_H, false);
      d->setCursor(text_x, ty);
      d->print(text ? text : "");
    }
  }

  /**
   * Inset button chip with ISO triangle + label.
   * point_left=true for BACK; false for forward (REPLY).
   */
  static void drawButtonRow(DisplayDriver* d, const char* label, int row_idx, bool selected,
                            bool point_left) {
    if (!d || row_idx < 0 || row_idx >= MAX_ROWS) return;
    int y = row_idx * ROW_H;
    int bx = BTN_PAD_X;
    int by = y + BTN_PAD_Y;
    int bw = W - 2 * BTN_PAD_X - 2;  // leave room for scrollbar
    int bh = ROW_H - 2 * BTN_PAD_Y;
    if (bh < 6) bh = ROW_H;

    const char* lab = label ? label : "";
    int text_w = (int)d->getTextWidth(lab);
    int content_w = TRI_H + 2 + text_w;
    int content_x = bx + (bw - content_w) / 2;
    if (content_x < bx + 2) content_x = bx + 2;
    int tri_y = by + (bh - TRI_H) / 2;
    int text_y = by + FONT_BASELINE;
    if (text_y > by + bh - 1) text_y = by + bh - 1;

    if (selected) {
      d->setColor(1);
      d->fillRect(bx, by, bw, bh);
      d->setColor(0);
    } else {
      d->setColor(1);
      d->drawRect(bx, by, bw, bh);
    }

    int tip_x;
    int text_x;
    if (point_left) {
      tip_x = content_x;
      text_x = content_x + TRI_H + 2;
    } else {
      tip_x = content_x + (TRI_H / 2);
      text_x = content_x + TRI_H + 2;
    }
    fillIsoTriangle(d, tip_x, tri_y, TRI_H, point_left);
    d->setCursor(text_x, text_y);
    d->print(lab);
    d->setColor(1);
  }

  /**
   * Horizontal marquee row for overflow text (Message Detail body).
   * scroll_px = how many pixels the text has shifted left.
   */
  static void drawMarqueeRow(DisplayDriver* d, const char* text, int row_idx, bool selected,
                             int scroll_px) {
    if (!d || row_idx < 0 || row_idx >= MAX_ROWS) return;
    int y = row_idx * ROW_H;
    int ty = y + FONT_BASELINE;
    const char* t = text ? text : "";

    if (selected) {
      d->setColor(1);
      d->fillRect(0, y, W, ROW_H);
      d->setColor(0);
    } else {
      d->setColor(1);
    }

    d->setCursor(TEXT_PAD - scroll_px, ty);
    d->print(t);

    // Mask side padding so overflow does not bleed past the inset
    if (selected) {
      d->setColor(1);
      d->fillRect(0, y, TEXT_PAD, ROW_H);
      d->fillRect(W - TEXT_PAD, y, TEXT_PAD, ROW_H);
    } else {
      d->setColor(0);
      d->fillRect(0, y, TEXT_PAD, ROW_H);
      d->fillRect(W - TEXT_PAD, y, TEXT_PAD, ROW_H);
      d->setColor(1);
    }
  }

  /** Visible text width for marquee (full row minus side pads). */
  static int marqueeAvailWidth() { return W - 2 * TEXT_PAD; }

  static void drawStyledRow(DisplayDriver* d, const char* text, int row_idx, bool selected,
                            TinyRowStyle style) {
    switch (style) {
      case TinyRowStyle::BackBtn:
        drawButtonRow(d, text, row_idx, selected, true);
        break;
      case TinyRowStyle::ForwardBtn:
        drawButtonRow(d, text, row_idx, selected, false);
        break;
      case TinyRowStyle::Unread:
        drawUnreadRow(d, text, row_idx, selected);
        break;
      case TinyRowStyle::Plain:
      default:
        drawRow(d, text, row_idx, selected);
        break;
    }
  }

  static void drawScrollbar(DisplayDriver* d, int n, int top) {
    if (!d || n <= MAX_ROWS) return;
    int thumb = H * MAX_ROWS / n;
    if (thumb < 3) thumb = 3;
    int y = H * top / n;
    d->setColor(1);
    d->fillRect(W - 2, y, 2, thumb);
  }

  /** Render list of C strings; cursor is absolute index. All plain rows. */
  static void renderList(DisplayDriver* d, const char* const* items, int n, int cursor,
                         int max_visible = MAX_ROWS, bool cursor_lit = true) {
    renderList(d, items, nullptr, n, cursor, max_visible, cursor_lit);
  }

  /**
   * Render list with optional per-row styles (nullptr => all Plain).
   * styles[i] applies to items[i].
   * max_visible limits how many rows are drawn (leave room for a status strip).
   * cursor_lit=false draws the cursor row unhighlighted (select-blink off phase).
   */
  static void renderList(DisplayDriver* d, const char* const* items, const TinyRowStyle* styles,
                         int n, int cursor, int max_visible = MAX_ROWS, bool cursor_lit = true) {
    if (!d) return;
    if (max_visible < 1) max_visible = 1;
    if (max_visible > MAX_ROWS) max_visible = MAX_ROWS;
    if (n <= 0) {
      drawRow(d, "(empty)", 0, false);
      return;
    }
    int top = 0;
    if (n > max_visible) {
      top = cursor - (max_visible - 1);
      if (top < 0) top = 0;
      if (top > n - max_visible) top = n - max_visible;
    }
    for (int i = 0; i < max_visible; i++) {
      int idx = top + i;
      if (idx >= n) break;
      TinyRowStyle st = styles ? styles[idx] : TinyRowStyle::Plain;
      bool sel = (idx == cursor) && cursor_lit;
      drawStyledRow(d, items[idx], i, sel, st);
    }
    if (n > max_visible) drawScrollbar(d, n, top);
  }

  static void renderLines(DisplayDriver* d, const char* const* lines, int n) {
    if (!d) return;
    int count = n < MAX_ROWS ? n : MAX_ROWS;
    for (int i = 0; i < count; i++) {
      drawRow(d, lines[i], i, false);
    }
  }

  /** Clear box interior, draw a 2px bottom/right drop shadow, then a 1px border. */
  static void drawModalBox(DisplayDriver* d, int bx, int by, int bw, int bh) {
    d->setColor(0);
    d->fillRect(bx, by, bw, bh);              // clear interior (hide UI under the box)
    d->setColor(1);
    d->fillRect(bx + 2, by + bh, bw, 2);      // shadow: bottom (offset right by 2px)
    d->fillRect(bx + bw, by + 2, 2, bh);      // shadow: right (offset down by 2px)
    d->drawRect(bx, by, bw, bh);              // border
  }

  /**
   * Bordered modal drawn on top of existing frame content (no start/endFrame).
   * Wraps text to ~12 chars/line, centered, with a drop shadow. Caller owns the frame.
   */
  static void drawBorderedOverlay(DisplayDriver* d, const char* text) {
    if (!d) return;
    const int max_chars = 12;
    const int max_lines = 4;
    char lines[4][16];
    int nlines = 0;
    const char* p = text ? text : "";
    while (*p && nlines < max_lines) {
      while (*p == ' ') p++;
      if (!*p) break;
      int len = 0;
      const char* start = p;
      const char* last_space = nullptr;
      while (p[len] && len < max_chars) {
        if (p[len] == ' ') last_space = p + len;
        len++;
      }
      int take = len;
      if (p[len] && last_space && last_space > start) {
        take = (int)(last_space - start);
      }
      if (take >= (int)sizeof(lines[0])) take = (int)sizeof(lines[0]) - 1;
      memcpy(lines[nlines], start, take);
      lines[nlines][take] = 0;
      nlines++;
      p = start + take;
    }
    if (nlines == 0) {
      strncpy(lines[0], "?", sizeof(lines[0]));
      nlines = 1;
    }

    int box_h = nlines * ROW_H + 4;
    if (box_h > H - 4) box_h = H - 4;
    const int box_w = W - 6;   // leave 2px for the right shadow + margin
    const int bx = 2;
    int by = (H - box_h) / 2;
    if (by < 1) by = 1;

    drawModalBox(d, bx, by, box_w, box_h);

    d->setColor(1);
    for (int i = 0; i < nlines; i++) {
      int tw = (int)d->getTextWidth(lines[i]);
      int tx = bx + (box_w - tw) / 2;
      if (tx < bx + 2) tx = bx + 2;
      d->setCursor(tx, by + 2 + i * ROW_H + FONT_BASELINE);
      d->print(lines[i]);
    }
  }

  /**
   * Bordered pick-list modal over existing frame (no start/endFrame).
   * Highlights cursor row; lit=false draws inverted off-phase for blink.
   */
  static void drawBorderedPickList(DisplayDriver* d, const char* const* items, int n,
                                   int cursor, bool lit) {
    if (!d || !items || n <= 0) return;
    if (n > 4) n = 4;
    if (cursor < 0) cursor = 0;
    if (cursor >= n) cursor = n - 1;

    int box_h = n * ROW_H + 4;
    if (box_h > H - 4) box_h = H - 4;
    const int box_w = W - 6;
    const int bx = 2;
    int by = (H - box_h) / 2;
    if (by < 1) by = 1;

    drawModalBox(d, bx, by, box_w, box_h);

    for (int i = 0; i < n; i++) {
      bool sel = (i == cursor) && lit;
      const char* t = items[i] ? items[i] : "";
      int tw = (int)d->getTextWidth(t);
      int tx = bx + (box_w - tw) / 2;
      if (tx < bx + 2) tx = bx + 2;
      int row_y = by + 2 + i * ROW_H;
      if (sel) {
        d->setColor(1);
        d->fillRect(bx + 1, row_y, box_w - 2, ROW_H);
        d->setColor(0);
      } else {
        d->setColor(1);
      }
      d->setCursor(tx, row_y + FONT_BASELINE);
      d->print(t);
    }
  }
};
