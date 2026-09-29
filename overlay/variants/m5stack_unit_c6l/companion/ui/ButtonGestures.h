#pragma once

#include <Arduino.h>

enum class BtnEvent : uint8_t {
  None = 0,
  Click,
  Long,     // fired once when hold crosses LONG_MS while still pressed
  Double,
  Triple
};

/**
 * Single-button gesture detector.
 * Click / Double / Triple resolve after short taps (CLICK_TIMEOUT_MS).
 * Long fires once while still held at LONG_MS (go-back hold starts in UITask).
 * Releasing after a long hold does not emit Click.
 */
class ButtonGestures {
public:
  static constexpr uint32_t LONG_MS = 500;
  static constexpr uint32_t CLICK_TIMEOUT_MS = 240;

  void reset() {
    _last_key = false;
    _down_at = 0;
    _pending_clicks = 0;
    _last_click = 0;
    _long_fired = false;
  }

  bool isPressed() const { return _last_key; }

  BtnEvent update(bool pressed, uint32_t now_ms) {
    BtnEvent ev = BtnEvent::None;

    if (pressed && !_last_key) {
      _down_at = now_ms;
      _long_fired = false;
    } else if (!pressed && _last_key) {
      uint32_t held = now_ms - _down_at;
      if (held >= LONG_MS || _long_fired) {
        // End of long hold — swallow (no click)
        _pending_clicks = 0;
      } else {
        _pending_clicks++;
        _last_click = now_ms;
      }
      _long_fired = false;
    } else if (pressed && _last_key && !_long_fired) {
      if (now_ms - _down_at >= LONG_MS) {
        _long_fired = true;
        _pending_clicks = 0;
        ev = BtnEvent::Long;
      }
    }
    _last_key = pressed;

    if (_pending_clicks && ev == BtnEvent::None && !pressed) {
      if (now_ms - _last_click >= CLICK_TIMEOUT_MS) {
        if (_pending_clicks == 1) ev = BtnEvent::Click;
        else if (_pending_clicks == 2) ev = BtnEvent::Double;
        else ev = BtnEvent::Triple;
        _pending_clicks = 0;
      }
    }
    return ev;
  }

private:
  bool _last_key = false;
  bool _long_fired = false;
  uint32_t _down_at = 0;
  uint8_t _pending_clicks = 0;
  uint32_t _last_click = 0;
};
