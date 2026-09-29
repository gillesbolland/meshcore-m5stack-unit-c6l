#pragma once

#include <helpers/ui/DisplayDriver.h>
#include "boot_logo_data.h"

class BootLogo {
public:
  /** Scroll logo RTL for duration_ms at max SPI refresh (no artificial delay). */
  static void play(DisplayDriver* d, uint32_t duration_ms = 1000) {
    if (!d) return;
    d->turnOn();
    const int W = d->width();
    const int H = d->height();
    const int travel = W + BOOT_LOGO_W;
    uint32_t t0 = millis();
    while (true) {
      uint32_t el = millis() - t0;
      if (el >= duration_ms) break;
      int x0 = W - (int)((el * (uint32_t)travel) / duration_ms);
      drawFrame(d, x0, W, H);
    }
    d->startFrame();
    d->endFrame();
  }

private:
  static bool pixelOn(int row, int col) {
    if (row < 0 || row >= BOOT_LOGO_H || col < 0 || col >= BOOT_LOGO_W) return false;
    uint8_t byte = pgm_read_byte(&BOOT_LOGO_BITS[row][col >> 3]);
    return (byte >> (col & 7)) & 1;
  }

  static void drawFrame(DisplayDriver* d, int x0, int W, int H) {
    d->startFrame();
    d->setColor(1);
    int y0 = (H - BOOT_LOGO_H) / 2;
    int lc0 = x0 < 0 ? -x0 : 0;
    int lc1 = BOOT_LOGO_W;
    if (x0 + BOOT_LOGO_W > W) lc1 = W - x0;
    if (lc1 <= lc0) {
      d->endFrame();
      return;
    }
    for (int r = 0; r < BOOT_LOGO_H; r++) {
      int sy = y0 + r;
      if (sy < 0 || sy >= H) continue;
      int lc = lc0;
      while (lc < lc1) {
        while (lc < lc1 && !pixelOn(r, lc)) lc++;
        int seg = lc;
        while (lc < lc1 && pixelOn(r, lc)) lc++;
        if (lc > seg) {
          d->fillRect(x0 + seg, sy, lc - seg, 1);
        }
      }
    }
    d->endFrame();
  }
};
