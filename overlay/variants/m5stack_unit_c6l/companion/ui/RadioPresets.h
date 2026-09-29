#pragma once

#include <Arduino.h>

struct RadioPreset {
  const char* label;   // short OLED label
  float freq;
  uint8_t sf;
  float bw;
  uint8_t cr;
  uint8_t path_hash_bytes;  // 1..3; 0 = leave unchanged
};

/** MeshCore app/flasher suggested presets (api.meshcore.nz, curated for OLED). */
static const RadioPreset C6L_PRESETS[] = {
  {"US/CA Rec",   910.525f, 7,  62.5f, 5, 0},
  {"EU/UK Narr",  869.618f, 8,  62.5f, 8, 0},
  {"EU/UK Old",   869.525f, 11, 250.0f, 5, 0},
  {"NL Narr",     869.618f, 7,  62.5f, 5, 0},
  {"HU 2B",       869.618f, 7,  62.5f, 5, 2},
  {"CZ Narr",     869.432f, 7,  62.5f, 5, 0},
  {"CH",          869.618f, 8,  62.5f, 8, 0},
  {"AU",          915.800f, 10, 250.0f, 5, 0},
  {"AU Narr",     916.575f, 7,  62.5f, 8, 0},
  {"NZ Narr",     917.375f, 7,  62.5f, 5, 2},
  {"EU433 LR",    433.650f, 11, 250.0f, 5, 0},
  {"EU433 Narr",  433.650f, 8,  62.5f, 8, 0},
  {"VN Narr",     920.250f, 8,  62.5f, 5, 0},
};
static const int C6L_PRESET_N = sizeof(C6L_PRESETS) / sizeof(C6L_PRESETS[0]);

/** Fixed modem params for all MeshCore client-repeat channels. */
static constexpr uint8_t C6L_CR_SF = 8;
static constexpr float   C6L_CR_BW = 62.5f;
static constexpr uint8_t C6L_CR_CR = 8;

/** Band-map current MHz → MeshCore client-repeat channel MHz. */
static inline float c6lClientRepeatFreqFor(float freq_mhz) {
  if (freq_mhz >= 400.0f && freq_mhz < 500.0f) return 433.000f;
  if (freq_mhz >= 860.0f && freq_mhz < 880.0f) return 869.495f;
  return 918.000f;
}
