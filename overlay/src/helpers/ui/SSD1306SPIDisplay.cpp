#include "SSD1306SPIDisplay.h"
#include <Fonts/TomThumb.h>
#include <string.h>

// Check if SPI is ready (set by radio_init in target.cpp)
#if defined(P_LORA_SCLK)
  extern bool spi_initialized;
#else
  static bool spi_initialized = true;  // Assume ready if no custom SPI
#endif

// Color scheme (monochrome OLED)
ColorVal UIColor::window_bkg = SSD1306_BLACK;
ColorVal UIColor::title_bkg = SSD1306_BLACK;
ColorVal UIColor::title_txt = SSD1306_WHITE;
ColorVal UIColor::primary_txt = SSD1306_WHITE;
ColorVal UIColor::secondary_txt = SSD1306_WHITE;
ColorVal UIColor::warning_txt = SSD1306_WHITE;
ColorVal UIColor::popup_bkg = SSD1306_BLACK;
ColorVal UIColor::popup_txt = SSD1306_WHITE;
ColorVal UIColor::corp_blue = SSD1306_WHITE;

bool SSD1306SPIDisplay::begin() {
  // Defer actual initialization - SPI may not be ready yet
  // Real init happens in lazyInit() on first use (after radio_init)
  return true;
}

bool SSD1306SPIDisplay::lazyInit() {
  if (_initialized) return true;
  if (!spi_initialized) {
    Serial.println("SSD1306: SPI not initialized yet");
    return false;
  }

  Serial.println("SSD1306: Attempting display init...");
  #ifdef DISPLAY_ROTATION
  display.setRotation(DISPLAY_ROTATION);
  #endif
  // SPI is now initialized by radio_init()
  // Pass periphBegin=false to skip spi.begin() since radio already did it
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0, true, false)) {
    Serial.println("SSD1306: display.begin() FAILED");
    return false;
  }
  Serial.println("SSD1306: display.begin() OK");

  // Fix for 64x48 displays: Adafruit library lacks this case and defaults
  // to comPins=0x02 (sequential). Displays taller than 32px need 0x12
  // (alternative COM pin config) or the output is garbled.
  #if defined(DISPLAY_WIDTH) && defined(DISPLAY_HEIGHT)
  #if (DISPLAY_WIDTH == 64) && (DISPLAY_HEIGHT == 48)
  display.ssd1306_command(SSD1306_SETCOMPINS);
  display.ssd1306_command(0x12);
  #endif
  #endif

  // Clear any garbage in the display buffer
  display.clearDisplay();
  display.display();
  display.setFont(&TomThumb);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.cp437(true);
  _initialized = true;
  return true;
}

void SSD1306SPIDisplay::turnOn() {
  if (!lazyInit()) return;
  display.ssd1306_command(SSD1306_DISPLAYON);
  _isOn = true;
}

void SSD1306SPIDisplay::turnOff() {
  if (!lazyInit()) return;
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  _isOn = false;
}

void SSD1306SPIDisplay::clear() {
  if (!lazyInit()) return;
  display.clearDisplay();
  display.display();
}

void SSD1306SPIDisplay::startFrame(ColorVal bkg) {
  if (!lazyInit()) return;
  display.clearDisplay();  // TODO: apply 'bkg'
  _color = UIColor::primary_txt;
  display.setTextColor(_color);
  display.setFont(&TomThumb);  // 3x5 Tom Thumb (baseline cursor)
  display.setTextSize(1);
  display.setTextWrap(false);
  display.cp437(true);
}

void SSD1306SPIDisplay::setTextSize(int sz) {
  if (!lazyInit()) return;
  display.setTextSize(sz);
}

void SSD1306SPIDisplay::setColor(ColorVal c) {
  if (!_initialized && !lazyInit()) return;
  _color = c;
  display.setTextColor(_color);
}

void SSD1306SPIDisplay::setCursor(int x, int y) {
  if (!lazyInit()) return;
  display.setCursor(x, y);
}

void SSD1306SPIDisplay::print(const char* str) {
  if (!lazyInit()) return;
  display.print(str);
}

void SSD1306SPIDisplay::fillRect(int x, int y, int w, int h) {
  if (!lazyInit()) return;
  display.fillRect(x, y, w, h, _color);
}

void SSD1306SPIDisplay::drawRect(int x, int y, int w, int h) {
  if (!lazyInit()) return;
  display.drawRect(x, y, w, h, _color);
}

void SSD1306SPIDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  if (!lazyInit()) return;
  display.drawBitmap(x, y, bits, w, h, SSD1306_WHITE);
}

uint16_t SSD1306SPIDisplay::getTextWidth(const char* str) {
  if (!lazyInit()) return 0;
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void SSD1306SPIDisplay::endFrame() {
  if (!_initialized) return;
  display.display();
}

bool SSD1306SPIDisplay::copyBuffer(uint8_t* dest, size_t n) {
  if (!dest || !lazyInit()) return false;
  size_t need = (size_t)width() * (((size_t)height() + 7) / 8);
  if (n < need) return false;
  memcpy(dest, display.getBuffer(), need);
  return true;
}

void SSD1306SPIDisplay::showSlidPair(const uint8_t* from, const uint8_t* to, int offset, bool back) {
  if (!from || !to || !lazyInit()) return;
  const int W = width();
  const int pages = (height() + 7) / 8;
  if (offset < 0) offset = 0;
  if (offset > W) offset = W;

  uint8_t* buf = display.getBuffer();
  memset(buf, 0, (size_t)W * (size_t)pages);

  for (int x = 0; x < W; x++) {
    int src_x = -1;
    const uint8_t* src = nullptr;
    if (!back) {
      // Forward (enter): current slides right, next enters from the left
      int from_col = x - offset;
      int to_col = x - offset + W;
      if (from_col >= 0 && from_col < W) {
        src = from;
        src_x = from_col;
      } else if (to_col >= 0 && to_col < W) {
        src = to;
        src_x = to_col;
      }
    } else {
      // Back: current slides left, parent enters from the right
      int from_col = x + offset;
      int to_col = x - (W - offset);
      if (from_col >= 0 && from_col < W) {
        src = from;
        src_x = from_col;
      } else if (to_col >= 0 && to_col < W) {
        src = to;
        src_x = to_col;
      }
    }
    if (!src || src_x < 0) continue;
    for (int p = 0; p < pages; p++) {
      buf[x + p * W] = src[src_x + p * W];
    }
  }
  display.display();
}
