#pragma once

#include <Arduino.h>
#include <string.h>
#include <helpers/AdvertDataHelpers.h>

#ifndef C6L_MSG_RING_SIZE
#define C6L_MSG_RING_SIZE 32
#endif

struct MsgRingEntry {
  char name[16];
  char ch[12];       // "DM" or channel name
  char text[40];
  uint8_t pub_prefix[7];
  bool has_prefix;
  bool is_dm;
  bool unread;
};

class MessageRing {
public:
  void clear() {
    _count = 0;
    memset(_entries, 0, sizeof(_entries));
  }

  void setPendingKind(bool is_dm) { _pending_is_dm = is_dm; }

  void onNewMsg(const char* from_name, const char* text) {
    bool is_dm = _pending_is_dm;
    // Find existing conversation by name + kind
    int idx = -1;
    for (int i = 0; i < _count; i++) {
      if (_entries[i].is_dm == is_dm &&
          strncmp(_entries[i].name, from_name ? from_name : "", sizeof(_entries[i].name)) == 0) {
        idx = i;
        break;
      }
    }
    if (idx < 0) {
      if (_count < C6L_MSG_RING_SIZE) {
        idx = _count++;
      } else {
        // Drop oldest (shift)
        memmove(&_entries[0], &_entries[1], sizeof(MsgRingEntry) * (C6L_MSG_RING_SIZE - 1));
        idx = C6L_MSG_RING_SIZE - 1;
      }
      memset(&_entries[idx], 0, sizeof(MsgRingEntry));
    }

    MsgRingEntry& e = _entries[idx];
    e.is_dm = is_dm;
    e.unread = true;
    strncpy(e.name, from_name ? from_name : "?", sizeof(e.name) - 1);
    if (is_dm) {
      strncpy(e.ch, "DM", sizeof(e.ch) - 1);
    } else {
      strncpy(e.ch, from_name ? from_name : "#", sizeof(e.ch) - 1);
    }
    strncpy(e.text, text ? text : "", sizeof(e.text) - 1);

    // Move to front (most recent)
    if (idx > 0) {
      MsgRingEntry tmp = e;
      memmove(&_entries[1], &_entries[0], sizeof(MsgRingEntry) * idx);
      _entries[0] = tmp;
    }
  }

  void markAllRead() {
    for (int i = 0; i < _count; i++) _entries[i].unread = false;
  }

  void markRead(int idx) {
    if (idx >= 0 && idx < _count) _entries[idx].unread = false;
  }

  void setPubPrefix(int idx, const uint8_t* prefix, int len) {
    if (idx < 0 || idx >= _count || !prefix) return;
    int n = len < 7 ? len : 7;
    memcpy(_entries[idx].pub_prefix, prefix, n);
    _entries[idx].has_prefix = true;
  }

  int count() const { return _count; }
  const MsgRingEntry* get(int idx) const {
    if (idx < 0 || idx >= _count) return nullptr;
    return &_entries[idx];
  }

  int unreadDmCount() const {
    int n = 0;
    for (int i = 0; i < _count; i++) {
      if (_entries[i].is_dm && _entries[i].unread) n++;
    }
    return n;
  }

private:
  MsgRingEntry _entries[C6L_MSG_RING_SIZE];
  int _count = 0;
  bool _pending_is_dm = true;
};
