#pragma once

#include <Arduino.h>
#include <math.h>

/**
 * Unread-DM NeoPixel rainbow breath: one pulse per minute, 1.5s fade.
 * Suppressed when stealth or unread_dm_count == 0.
 */
class DmNotifyLed {
public:
  static constexpr uint32_t PULSE_PERIOD_MS = 60000;
  static constexpr uint32_t PULSE_DUR_MS = 1500;

  void begin(int pin) {
    _pin = pin;
    _last_pulse = millis();
    off();
  }

  void setStealth(bool s) {
    _stealth = s;
    if (_stealth) off();
  }

  bool stealth() const { return _stealth; }

  void tick(uint32_t now, int unread_dm) {
    if (_pin < 0) return;
    if (_stealth || unread_dm <= 0) {
      if (_led_on) off();
      _pulse_active = false;
      return;
    }

    if (!_pulse_active) {
      if (now - _last_pulse >= PULSE_PERIOD_MS) {
        _pulse_active = true;
        _pulse_start = now;
        _pulse_hue = (_pulse_hue + 40) % 360;
        _last_pulse = now;
      }
      return;
    }

    uint32_t el = now - _pulse_start;
    if (el >= PULSE_DUR_MS) {
      off();
      _pulse_active = false;
      return;
    }

    float half = PULSE_DUR_MS / 2.0f;
    float v = (el <= half) ? (el / half) : ((PULSE_DUR_MS - el) / half);
    uint8_t r, g, b;
    hsvToRgb(_pulse_hue, 1.0f, v, r, g, b);
    neopixelWrite(_pin, r, g, b);
    _led_on = true;
  }

  void off() {
    if (_pin >= 0) neopixelWrite(_pin, 0, 0, 0);
    _led_on = false;
  }

private:
  static void hsvToRgb(int h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    h = h % 360;
    if (h < 0) h += 360;
    int i = (h / 60) % 6;
    float f = h / 60.0f - (h / 60);
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    float rf, gf, bf;
    switch (i) {
      case 0: rf = v; gf = t; bf = p; break;
      case 1: rf = q; gf = v; bf = p; break;
      case 2: rf = p; gf = v; bf = t; break;
      case 3: rf = p; gf = q; bf = v; break;
      case 4: rf = t; gf = p; bf = v; break;
      default: rf = v; gf = p; bf = q; break;
    }
    r = (uint8_t)(rf * 255);
    g = (uint8_t)(gf * 255);
    b = (uint8_t)(bf * 255);
  }

  int _pin = -1;
  bool _stealth = false;
  bool _pulse_active = false;
  bool _led_on = false;
  uint32_t _pulse_start = 0;
  uint32_t _last_pulse = 0;
  int _pulse_hue = 0;
};
