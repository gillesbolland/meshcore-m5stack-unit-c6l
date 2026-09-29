#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <string.h>

/** Variant-local UI prefs (SPIFFS /c6l_ui.bin) — not MeshCore NodePrefs. */
struct UiPrefs {
  uint8_t magic = 0xC6;
  uint8_t version = 1;
  uint8_t radio_setup_done = 0;
  uint8_t reserved0 = 0;         // was hash_setup_done; keep SPIFFS layout
  uint8_t serial_mode = 1;       // 0=BLE, 1=USB, 2=WiFi (USB default)
  int8_t  last_preset = -1;
  // Stash for client-repeater restore
  uint8_t has_radio_stash = 0;
  float   stash_freq = 0;
  float   stash_bw = 0;
  uint8_t stash_sf = 0;
  uint8_t stash_cr = 0;
  int8_t  stash_preset = -1;
  // Keep USB companion/CDC active for debug when Interface is BLE or WiFi
  uint8_t usb_debug = 0;

  static constexpr const char* PATH = "/c6l_ui.bin";

  bool load() {
    if (!SPIFFS.exists(PATH)) return false;
    File f = SPIFFS.open(PATH, "r");
    if (!f) return false;
    UiPrefs tmp;
    size_t n = f.read((uint8_t*)&tmp, sizeof(tmp));
    f.close();
    if (n < 4 || tmp.magic != 0xC6) return false;
    *this = tmp;
    if (serial_mode > 2) serial_mode = 0;
    return true;
  }

  bool save() const {
    File f = SPIFFS.open(PATH, "w");
    if (!f) return false;
    size_t n = f.write((const uint8_t*)this, sizeof(*this));
    f.close();
    return n == sizeof(*this);
  }
};
