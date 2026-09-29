#pragma once

#include <stdio.h>
#include <stdint.h>

/** Format age as compact 12m / 2h / 4d (null-terminated, max ~6 chars). */
inline void formatRelativeAge(uint32_t age_secs, char* out, size_t out_len) {
  if (!out || out_len < 3) return;
  if (age_secs < 60) {
    snprintf(out, out_len, "%lus", (unsigned long)age_secs);
  } else if (age_secs < 3600) {
    snprintf(out, out_len, "%lum", (unsigned long)(age_secs / 60));
  } else if (age_secs < 86400) {
    snprintf(out, out_len, "%luh", (unsigned long)(age_secs / 3600));
  } else {
    snprintf(out, out_len, "%lud", (unsigned long)(age_secs / 86400));
  }
}
