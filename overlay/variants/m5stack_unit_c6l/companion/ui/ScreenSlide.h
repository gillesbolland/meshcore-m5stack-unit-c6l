#pragma once

#include <Arduino.h>
#include <string.h>
#include <helpers/ui/SSD1306SPIDisplay.h>

/**
 * Two-frame buffer for iPod-style horizontal screen slides on 64x48 OLED.
 */
class ScreenSlide {
public:
  static constexpr size_t FRAME_BYTES = SSD1306SPIDisplay::FRAME_BYTES;

  uint8_t from_fb[FRAME_BYTES];
  uint8_t to_fb[FRAME_BYTES];

  bool captureFrom(SSD1306SPIDisplay* d) {
    return d && d->copyBuffer(from_fb, FRAME_BYTES);
  }

  bool captureTo(SSD1306SPIDisplay* d) {
    return d && d->copyBuffer(to_fb, FRAME_BYTES);
  }

  /** progress in [0..1]; back=false slides right (enter), true slides left (go back). */
  void blit(SSD1306SPIDisplay* d, float progress, bool back) {
    if (!d) return;
    if (progress < 0.f) progress = 0.f;
    if (progress > 1.f) progress = 1.f;
    int offset = (int)(progress * (float)d->width() + 0.5f);
    if (offset > d->width()) offset = d->width();
    d->showSlidPair(from_fb, to_fb, offset, back);
  }

  /** Blocking forward (right) slide over duration_ms. */
  void playForward(SSD1306SPIDisplay* d, uint32_t duration_ms = 280) {
    if (!d) return;
    const int frames = 10;
    uint32_t t0 = millis();
    for (int i = 1; i <= frames; i++) {
      float p = (float)i / (float)frames;
      blit(d, p, false);
      uint32_t target = t0 + (duration_ms * (uint32_t)i) / (uint32_t)frames;
      while ((int32_t)(millis() - target) < 0) { /* spin */ }
    }
    blit(d, 1.f, false);
  }
};
