#pragma once

#include "TinyListDraw.h"
#include "MessageRing.h"
#include "RelativeTime.h"
#include "ButtonGestures.h"
#include "RadioPresets.h"
#include "UiPrefs.h"
#include <helpers/AdvertDataHelpers.h>
#include <helpers/ContactInfo.h>
#include "../../../../examples/companion_radio/MyMesh.h"
#if defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID)
  #include <WiFi.h>
#endif

enum class C6lScreen : uint8_t {
  GestureHelp = 0,  // first-boot button instructions
  PresetSetup,      // first-boot radio preset gate
  Menu,
  Messages,
  MessageDetail,
  Reply,
  Contacts,       // favourites only + "other contacts"
  OtherContacts,  // non-favourites + "erase list"
  ContactDetail,
  Advert,         // menu label: Nodes
  Status,
  Settings,
  IfaceMenu,     // Settings → Interfaces
  LoRaSettings,  // Settings → LoRa
  PresetList,
  PathHash,
  ModeSetup,  // first-boot companion Mode (BLE/USB/WiFi)
};

enum class ModalKind : uint8_t {
  None = 0,
  Status,
  PickMode,
  PickUsbDbg,
  BlePin,
  WifiStatus,
  Factory,
};

struct CannedReply {
  const char* label;
  const char* text;
};

static const CannedReply C6L_CANNED[] = {
  {"Yes", "Yes"},
  {"No", "No"},
  {"Cant talk", "Can't talk right now"},
  {"Find me", "Please find me"},
  {"OMW", "On my way"},
  {"Help", "Help"},
};
static const int C6L_CANNED_N = sizeof(C6L_CANNED) / sizeof(C6L_CANNED[0]);

static const char* C6L_MENU[] = {"Messages", "Contacts", "Nodes", "Status", "Settings"};
static const int C6L_MENU_N = 5;

/** Contact.flags LSB = favourite (MeshCore companion protocol). */
static inline bool c6lContactIsFavorite(const ContactInfo& c) {
  return (c.flags & 0x01) != 0;
}

/** Settings root: LoRa · Interfaces · Path hash · Local advert · Factory reset */
static const int C6L_SETTINGS_N = 5;
/** Interfaces submenu: Mode · BLE · WiFi · USB Debug */
static const int C6L_IFACE_N = 4;
/** LoRa submenu: Preset · Repeat */
static const int C6L_LORA_N = 2;

/** OLED labels for path_hash_mode 0/1/2 (1/2/3 byte hashes). */
static const char* C6L_HASH_LABELS[3] = {
  "1B 64 hops",
  "2B 32 hops",
  "3B 21 hops",
};

static const char* c6lSerialModeLabel(uint8_t mode) {
  if (mode == 1) return "USB";
  if (mode == 2) return "WiFi";
  return "BLE";
}


/**
 * Screen tree + navigation for 64x48 companion UI.
 * Mesh I/O via the_mesh / UITask callbacks.
 */
class ScreenNav {
public:
  C6lScreen screen = C6lScreen::Menu;
  int menu_cursor = 0;
  int list_cursor = 0;
  int detail_idx = 0;
  int msg_cursor = 0;
  int reply_cursor = 0;
  int settings_cursor = 0;
  int iface_cursor = 0;
  int lora_cursor = 0;
  int preset_cursor = 0;
  int path_cursor = 0;
  int mode_cursor = 1;    // first-boot ModeSetup default: USB
  int detail_cursor = 0;  // ContactDetail row (0 = favourite toggle)
  int help_page = 0;      // GestureHelp page 0..1
  uint8_t detail_pub[PUB_KEY_SIZE] = {};
  bool detail_has_key = false;
  C6lScreen contact_detail_from = C6lScreen::Contacts;

  // Unified modal (factory / picks / status / BLE PIN / WiFi status)
  ModalKind modal = ModalKind::None;
  int modal_cursor = 0;

  bool modalActive() const { return modal != ModalKind::None; }

  void closeModal() { modal = ModalKind::None; modal_cursor = 0; }

  void openFactoryConfirm() {
    flash_active = false;
    modal = ModalKind::Factory;
    modal_cursor = 0;
  }

  void openPickMode(UiPrefs& ui) {
    flash_active = false;
    modal = ModalKind::PickMode;
    modal_cursor = (int)ui.serial_mode;
    if (modal_cursor < 0 || modal_cursor > 2) modal_cursor = 0;
  }

  void openPickUsbDbg(UiPrefs& ui) {
    flash_active = false;
    modal = ModalKind::PickUsbDbg;
    modal_cursor = ui.usb_debug ? 0 : 1;  // 0=ON 1=OFF
  }

  void openBlePinModal() {
    flash_active = false;
    modal = ModalKind::BlePin;
    modal_cursor = 0;
  }

  /** WiFi companion status: IP :port when Mode=WiFi and connected. */
  void openWifiStatusModal(UiPrefs& ui) {
    flash_active = false;
    modal = ModalKind::WifiStatus;
    modal_cursor = 0;
#if !(defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID))
    (void)ui;
    strncpy(flash_text, "no WiFi", sizeof(flash_text) - 1);
#elif !defined(WIFI_SSID)
    if (ui.serial_mode != 2)
      strncpy(flash_text, "Mode not WiFi", sizeof(flash_text) - 1);
    else
      strncpy(flash_text, "no SSID", sizeof(flash_text) - 1);
#else
#ifndef TCP_PORT
    const int tcp_port = 5000;
#else
    const int tcp_port = TCP_PORT;
#endif
    if (ui.serial_mode != 2) {
      strncpy(flash_text, "Mode not WiFi", sizeof(flash_text) - 1);
    } else if (WiFi.status() != WL_CONNECTED) {
      strncpy(flash_text, "not connected", sizeof(flash_text) - 1);
    } else {
      IPAddress ip = WiFi.localIP();
      snprintf(flash_text, sizeof(flash_text), "%d.%d.%d.%d :%d",
               ip[0], ip[1], ip[2], ip[3], tcp_port);
    }
#endif
    flash_text[sizeof(flash_text) - 1] = 0;
  }

  /** Update status modal text. */
  void setModalStatus(const char* text) {
    strncpy(flash_text, text ? text : "", sizeof(flash_text) - 1);
    flash_text[sizeof(flash_text) - 1] = 0;
  }

  static int settingsCount() { return C6L_SETTINGS_N; }


  // Double-click select blink (highlight flash before action)
  bool select_blink_active = false;
  uint32_t select_blink_start = 0;
  static constexpr uint32_t SELECT_BLINK_HALF_MS = 55;
  static constexpr int SELECT_BLINK_HALVES = 6;  // 3 full on/off cycles

  void startSelectBlink(uint32_t now) {
    select_blink_active = true;
    select_blink_start = now;
  }

  bool selectBlinkActive() const { return select_blink_active; }

  /** true = draw cursor highlighted; false = off phase of blink. */
  bool selectCursorLit(uint32_t now) const {
    if (!select_blink_active) return true;
    return ((now - select_blink_start) / SELECT_BLINK_HALF_MS) % 2 == 0;
  }

  /** Returns true when blink just finished (caller should run pending select). */
  bool tickSelectBlink(uint32_t now) {
    if (!select_blink_active) return false;
    if (now - select_blink_start >= SELECT_BLINK_HALF_MS * (uint32_t)SELECT_BLINK_HALVES) {
      select_blink_active = false;
      return true;
    }
    return false;
  }

  void cancelSelectBlink() { select_blink_active = false; }

  // Message Detail body marquee
  int scroll_px = 0;
  uint32_t scroll_next_ms = 0;
  bool scroll_paused = true;
  static constexpr uint32_t SCROLL_SPEED_MS = 45;
  static constexpr uint32_t SCROLL_PAUSE_MS = 1000;

  void resetScroll() {
    scroll_px = 0;
    scroll_next_ms = 0;
    scroll_paused = true;
  }

  // Flash / bordered popup
  bool flash_active = false;
  bool popup_to_menu = false;
  char flash_text[32];
  uint32_t flash_until = 0;
  static constexpr uint32_t POPUP_MS = 2500;

  void showFlash(const char* text, uint32_t ms, uint32_t now) {
    strncpy(flash_text, text ? text : "", sizeof(flash_text) - 1);
    flash_text[sizeof(flash_text) - 1] = 0;
    flash_active = true;
    popup_to_menu = false;
    flash_until = now + ms;
  }

  void showPopup(const char* text, uint32_t now, bool return_to_menu) {
    strncpy(flash_text, text ? text : "", sizeof(flash_text) - 1);
    flash_text[sizeof(flash_text) - 1] = 0;
    flash_active = true;
    popup_to_menu = return_to_menu;
    flash_until = now + POPUP_MS;
  }

  bool consumeFlash(uint32_t now) {
    if (flash_active && now >= flash_until) {
      flash_active = false;
      if (popup_to_menu) {
        screen = C6lScreen::Menu;
        popup_to_menu = false;
      }
      return true;
    }
    return false;
  }

  void dismissPopup() {
    if (!flash_active) return;
    flash_active = false;
    if (popup_to_menu) {
      screen = C6lScreen::Menu;
      popup_to_menu = false;
    }
  }

  void initFromPrefs(const UiPrefs& ui) {
    if (ui.radio_setup_done) {
      screen = C6lScreen::Menu;
    } else if (ui.last_preset >= 0 && ui.last_preset < C6L_PRESET_N) {
      // Preset chosen; Mode gate next (hash skipped — default 2B applied with preset)
      screen = C6lScreen::ModeSetup;
      mode_cursor = 1;  // USB default
    } else {
      screen = C6lScreen::GestureHelp;
      help_page = 0;
      preset_cursor = 0;
    }
  }

  /** True if long-hold go-back can leave this screen. */
  bool canGoBack() const {
    if (modalActive()) return true;  // hold cancels modal
    return screen != C6lScreen::Menu
        && screen != C6lScreen::GestureHelp
        && screen != C6lScreen::PresetSetup
        && screen != C6lScreen::ModeSetup;
  }

  /** Parent screen for go-back preview (no state mutation). */
  C6lScreen parentScreen() const {
    if (modalActive()) return screen;  // cancel modal stays on same screen
    switch (screen) {
      case C6lScreen::GestureHelp:
      case C6lScreen::PresetSetup:
      case C6lScreen::ModeSetup:
      case C6lScreen::Menu:
        return screen;
      case C6lScreen::Messages:
      case C6lScreen::Contacts:
      case C6lScreen::Advert:
      case C6lScreen::Status:
      case C6lScreen::Settings:
        return C6lScreen::Menu;
      case C6lScreen::OtherContacts:
        return C6lScreen::Contacts;
      case C6lScreen::MessageDetail:
        return C6lScreen::Messages;
      case C6lScreen::Reply:
        return C6lScreen::MessageDetail;
      case C6lScreen::ContactDetail:
        return contact_detail_from;
      case C6lScreen::IfaceMenu:
      case C6lScreen::PathHash:
      case C6lScreen::LoRaSettings:
        return C6lScreen::Settings;
      case C6lScreen::PresetList:
        return C6lScreen::LoRaSettings;
      default:
        return C6lScreen::Menu;
    }
  }

  /** Navigate up one level. Returns true if left a screen. */
  bool goBack() {
    if (modalActive()) {  // hold-to-back cancels modal first
      closeModal();
      flash_active = false;
      return true;
    }
    dismissPopup();
    switch (screen) {
      case C6lScreen::GestureHelp:
      case C6lScreen::PresetSetup:
      case C6lScreen::ModeSetup:
        return false;
      case C6lScreen::Menu:
        return false;
      case C6lScreen::Messages:
      case C6lScreen::Contacts:
      case C6lScreen::Advert:
      case C6lScreen::Status:
      case C6lScreen::Settings:
        screen = C6lScreen::Menu;
        return true;
      case C6lScreen::OtherContacts:
        list_cursor = 0;
        screen = C6lScreen::Contacts;
        return true;
      case C6lScreen::MessageDetail:
        screen = C6lScreen::Messages;
        return true;
      case C6lScreen::Reply:
        screen = C6lScreen::MessageDetail;
        return true;
      case C6lScreen::ContactDetail:
        screen = contact_detail_from;
        detail_has_key = false;
        return true;
      case C6lScreen::IfaceMenu:
      case C6lScreen::PathHash:
      case C6lScreen::LoRaSettings:
        screen = C6lScreen::Settings;
        return true;
      case C6lScreen::PresetList:
        screen = C6lScreen::LoRaSettings;
        return true;
      default:
        screen = C6lScreen::Menu;
        return true;
    }
  }

  void enterSection(uint32_t now, MessageRing& msgs, MyMesh& mesh) {
    switch (menu_cursor) {
      case 0:
        screen = C6lScreen::Messages;
        list_cursor = 0;
        if (msgs.count() == 0) showPopup("This place is empty", now, true);
        break;
      case 1:
        screen = C6lScreen::Contacts;
        list_cursor = 0;
        if (mesh.getNumContacts() <= 0) showPopup("Nobody here", now, true);
        break;
      case 2:
        screen = C6lScreen::Advert;
        list_cursor = 0;
        {
          AdvertPath recent[ADVERT_PATH_TABLE_SIZE];
          if (collectChatAdverts(mesh, recent) == 0)
            showPopup("No one around", now, true);
        }
        break;
      case 3:
        screen = C6lScreen::Status;
        break;
      case 4:
        screen = C6lScreen::Settings;
        settings_cursor = 0;
        break;
      default:
        break;
    }
  }

  static int collectChatAdverts(MyMesh& mesh, AdvertPath* out) {
    AdvertPath all[ADVERT_PATH_TABLE_SIZE];
    mesh.getRecentlyHeard(all, ADVERT_PATH_TABLE_SIZE);
    int n = 0;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      if (all[i].recv_timestamp == 0) continue;
      if (all[i].type != ADV_TYPE_CHAT) continue;
      if (all[i].name[0] == 0) continue;
      out[n++] = all[i];
    }
    return n;
  }

  /** Real contacts only (skip MAX_ANON_CONTACTS slots). display_idx is 0..getNumContacts()-1. */
  static bool getRealContact(MyMesh& mesh, int display_idx, ContactInfo& dest) {
    if (display_idx < 0) return false;
    ContactsIterator it = mesh.startContactsIterator();
    ContactInfo c;
    int i = 0;
    while (it.hasNext(&mesh, c)) {
      if (i == display_idx) {
        dest = c;
        return true;
      }
      i++;
    }
    return false;
  }

  static int countFavorites(MyMesh& mesh) {
    int n = 0;
    ContactsIterator it = mesh.startContactsIterator();
    ContactInfo c;
    while (it.hasNext(&mesh, c)) {
      if (c6lContactIsFavorite(c)) n++;
    }
    return n;
  }

  static int countOthers(MyMesh& mesh) {
    int n = 0;
    ContactsIterator it = mesh.startContactsIterator();
    ContactInfo c;
    while (it.hasNext(&mesh, c)) {
      if (!c6lContactIsFavorite(c)) n++;
    }
    return n;
  }

  /** Favourites list: 0..fav-1 = contacts, last row = "other contacts". */
  static int favoritesListCount(MyMesh& mesh) {
    return countFavorites(mesh) + 1;
  }

  /** Other list: row 0 = "erase list", then non-favourites. */
  static int othersListCount(MyMesh& mesh) {
    return countOthers(mesh) + 1;
  }

  static bool getFavoriteContact(MyMesh& mesh, int fav_idx, ContactInfo& dest) {
    if (fav_idx < 0) return false;
    ContactsIterator it = mesh.startContactsIterator();
    ContactInfo c;
    int i = 0;
    while (it.hasNext(&mesh, c)) {
      if (!c6lContactIsFavorite(c)) continue;
      if (i == fav_idx) {
        dest = c;
        return true;
      }
      i++;
    }
    return false;
  }

  static bool getOtherContact(MyMesh& mesh, int other_idx, ContactInfo& dest) {
    if (other_idx < 0) return false;
    ContactsIterator it = mesh.startContactsIterator();
    ContactInfo c;
    int i = 0;
    while (it.hasNext(&mesh, c)) {
      if (c6lContactIsFavorite(c)) continue;
      if (i == other_idx) {
        dest = c;
        return true;
      }
      i++;
    }
    return false;
  }

  static bool lookupDetailContact(MyMesh& mesh, const uint8_t* pub, ContactInfo& dest) {
    if (!pub) return false;
    ContactInfo* p = mesh.lookupContactByPubKey(pub, PUB_KEY_SIZE);
    if (!p) return false;
    dest = *p;
    return true;
  }

  static int contactDetailRowCount(const ContactInfo& c) {
    int n = 4;  // fav, name, pubkey, hops
    if (c.gps_lat != 0 || c.gps_lon != 0) n += 1;
    return n;
  }

  void openContactDetail(const ContactInfo& c, C6lScreen from) {
    memcpy(detail_pub, c.id.pub_key, PUB_KEY_SIZE);
    detail_has_key = true;
    contact_detail_from = from;
    detail_cursor = 0;
    screen = C6lScreen::ContactDetail;
  }

  /**
   * Build Nodes list order: unknown (not in contacts) first, optional "----", then known.
   * out_map[i] = index into recent[], or -1 for separator.
   */
  static int buildNodesRows(MyMesh& mesh, const AdvertPath* recent, int chat_n,
                            int* out_map, int max_out) {
    if (!recent || !out_map || max_out <= 0) return 0;
    int n = 0;
    int known_start = -1;
    for (int pass = 0; pass < 2; pass++) {
      bool want_known = (pass == 1);
      if (pass == 1 && n > 0) {
        // Only insert separator if both sides have entries
        int unknowns = n;
        int knowns = 0;
        for (int i = 0; i < chat_n; i++) {
          bool in = mesh.lookupContactByPubKey(recent[i].pub_key, PUB_KEY_SIZE) != nullptr;
          if (in) knowns++;
        }
        if (unknowns > 0 && knowns > 0 && n < max_out) {
          out_map[n++] = -1;
          known_start = n;
        }
      }
      for (int i = 0; i < chat_n && n < max_out; i++) {
        bool in = mesh.lookupContactByPubKey(recent[i].pub_key, PUB_KEY_SIZE) != nullptr;
        if (in != want_known) continue;
        out_map[n++] = i;
      }
    }
    (void)known_start;
    return n;
  }

  void settingsLabel(int idx, char* dest, size_t dest_len, NodePrefs* prefs, UiPrefs& ui) {
    (void)ui;
    switch (idx) {
      case 0:
        strncpy(dest, "LoRa", dest_len);
        break;
      case 1:
        strncpy(dest, "Interfaces", dest_len);
        break;
      case 2: {
        int mode = prefs ? (int)prefs->path_hash_mode : 0;
        if (mode < 0 || mode > 2) mode = 0;
        snprintf(dest, dest_len, "H %s", C6L_HASH_LABELS[mode]);
        break;
      }
      case 3:
        strncpy(dest, "Local advert", dest_len);
        break;
      case 4:
        strncpy(dest, "Factory reset", dest_len);
        break;
      default:
        dest[0] = 0;
        break;
    }
    dest[dest_len - 1] = 0;
  }

  void ifaceLabel(int idx, char* dest, size_t dest_len, UiPrefs& ui) {
    switch (idx) {
      case 0:
        snprintf(dest, dest_len, "Mode %s", c6lSerialModeLabel(ui.serial_mode));
        break;
      case 1:
        strncpy(dest, "BLE", dest_len);
        break;
      case 2:
        strncpy(dest, "WiFi", dest_len);
        break;
      case 3:
        strncpy(dest, "USB Debug", dest_len);
        break;
      default:
        dest[0] = 0;
        break;
    }
    dest[dest_len - 1] = 0;
  }

  void loraLabel(int idx, char* dest, size_t dest_len, NodePrefs* prefs, UiPrefs& ui) {
    switch (idx) {
      case 0:
        if (prefs && prefs->isRepeatEn())
          snprintf(dest, dest_len, "CR %.3f", (double)prefs->freq);
        else if (ui.last_preset >= 0 && ui.last_preset < C6L_PRESET_N)
          snprintf(dest, dest_len, "Preset %s", C6L_PRESETS[ui.last_preset].label);
        else
          strncpy(dest, "Radio preset", dest_len);
        break;
      case 1:
        snprintf(dest, dest_len, "Repeat %s", prefs && prefs->isRepeatEn() ? "ON" : "OFF");
        break;
      default:
        dest[0] = 0;
        break;
    }
    dest[dest_len - 1] = 0;
  }

  /** Click = next; Double = select/enter. Returns true if UI should beep. */
  bool handle(BtnEvent ev, MessageRing& msgs, MyMesh& mesh, bool connected, uint32_t now,
              NodePrefs* prefs, UiPrefs& ui, bool* need_reboot, bool* applied_preset,
              bool* want_advert = nullptr,
              bool* want_factory = nullptr, bool* want_repeat = nullptr,
              bool* applied_mode = nullptr) {
    (void)connected;
    if (need_reboot) *need_reboot = false;
    if (applied_preset) *applied_preset = false;
    if (want_advert) *want_advert = false;
    if (want_factory) *want_factory = false;
    if (want_repeat) *want_repeat = false;
    if (applied_mode) *applied_mode = false;
    if (ev != BtnEvent::Click && ev != BtnEvent::Double) return false;

    bool select = (ev == BtnEvent::Double);

    // Interactive modals first
    if (modal == ModalKind::Factory) {
      if (select) {
        closeModal();
        if (want_factory) *want_factory = true;
        return true;
      }
      closeModal();  // click cancels
      return false;
    }

    if (modal == ModalKind::PickMode) {
      if (!select) {
        modal_cursor = (modal_cursor + 1) % 3;
        return false;
      }
      ui.serial_mode = (uint8_t)modal_cursor;
      ui.save();
      closeModal();
      char line[16];
      snprintf(line, sizeof(line), "%s mode", c6lSerialModeLabel(ui.serial_mode));
      showFlash(line, 900, now);
      if (need_reboot) *need_reboot = true;
      return true;
    }

    if (modal == ModalKind::PickUsbDbg) {
      if (!select) {
        modal_cursor = (modal_cursor + 1) % 2;
        return false;
      }
      ui.usb_debug = (modal_cursor == 0) ? 1 : 0;
      ui.save();
      closeModal();
      showFlash(ui.usb_debug ? "debug ON" : "debug OFF", 900, now);
      if (need_reboot) *need_reboot = true;
      return true;
    }

    if (modal == ModalKind::BlePin) {
      if (select) {
        closeModal();
        return true;
      }
      return false;
    }

    if (modal == ModalKind::WifiStatus) {
      if (select) {
        closeModal();
        return true;
      }
      return false;
    }

    if (flash_active) {
      dismissPopup();
      return false;
    }

    if (screen == C6lScreen::GestureHelp) {
      if (!select) {
        if (help_page < 1) help_page++;
        return false;
      }
      if (help_page < 1) {
        help_page++;
        return false;
      }
      screen = C6lScreen::PresetSetup;
      preset_cursor = 0;
      return true;
    }

    if (screen == C6lScreen::PresetSetup) {
      if (!select) {
        preset_cursor = (preset_cursor + 1) % C6L_PRESET_N;
        return false;
      }
      if (applied_preset) *applied_preset = true;
      return true;
    }

    if (screen == C6lScreen::Menu) {
      if (!select) {
        menu_cursor = (menu_cursor + 1) % C6L_MENU_N;
        return false;
      }
      enterSection(now, msgs, mesh);
      return true;
    }

    if (screen == C6lScreen::Messages) {
      int n = msgs.count();
      if (n <= 0) {
        showPopup("This place is empty", now, true);
        return false;
      }
      if (!select) {
        list_cursor = (list_cursor + 1) % n;
        return false;
      }
      detail_idx = list_cursor;
      msg_cursor = 0;
      resetScroll();
      msgs.markRead(detail_idx);
      screen = C6lScreen::MessageDetail;
      return true;
    }

    if (screen == C6lScreen::MessageDetail) {
      if (!select) {
        msg_cursor = (msg_cursor + 1) % 3;  // REPLY, peer, body
        return false;
      }
      if (msg_cursor == 0) {
        reply_cursor = 0;
        screen = C6lScreen::Reply;
        return true;
      }
      return false;
    }

    if (screen == C6lScreen::Reply) {
      if (!select) {
        reply_cursor = (reply_cursor + 1) % C6L_CANNED_N;
        return false;
      }
      const MsgRingEntry* m = msgs.get(detail_idx);
      if (m && m->is_dm) {
        ContactInfo* c = nullptr;
        if (m->has_prefix) c = mesh.lookupContactByPubKey(m->pub_prefix, 7);
        if (!c) c = mesh.searchContactsByPrefix(m->name);
        if (c) {
          uint32_t ack = 0, timeout = 0;
          uint32_t ts = mesh.getRTCClock()->getCurrentTime();
          mesh.sendMessage(*c, ts, 0, C6L_CANNED[reply_cursor].text, ack, timeout);
          msgs.markRead(detail_idx);
          char flash[16];
          TinyListDraw::clip(flash, sizeof(flash), C6L_CANNED[reply_cursor].text, 12);
          char line[20];
          snprintf(line, sizeof(line), "sent %s", flash);
          showFlash(line, 900, now);
        } else {
          showFlash("no contact", 900, now);
        }
      } else {
        showFlash("DM only", 900, now);
      }
      list_cursor = detail_idx;
      screen = C6lScreen::Messages;
      return true;
    }

    if (screen == C6lScreen::Contacts) {
      int nc = mesh.getNumContacts();
      if (nc <= 0) {
        showPopup("Nobody here", now, true);
        return false;
      }
      int rows = favoritesListCount(mesh);
      if (!select) {
        list_cursor = (list_cursor + 1) % rows;
        return false;
      }
      int fav_n = countFavorites(mesh);
      if (list_cursor == fav_n) {
        // last row → other contacts
        list_cursor = 0;
        screen = C6lScreen::OtherContacts;
        return true;
      }
      ContactInfo c;
      if (getFavoriteContact(mesh, list_cursor, c)) {
        openContactDetail(c, C6lScreen::Contacts);
        return true;
      }
      return false;
    }

    if (screen == C6lScreen::OtherContacts) {
      int rows = othersListCount(mesh);
      if (!select) {
        list_cursor = (list_cursor + 1) % rows;
        return false;
      }
      if (list_cursor == 0) {
        int n = mesh.eraseNonFavoriteContacts();
        char line[16];
        snprintf(line, sizeof(line), "erased %d", n);
        showFlash(line, 1000, now);
        list_cursor = 0;
        return true;
      }
      ContactInfo c;
      if (getOtherContact(mesh, list_cursor - 1, c)) {
        openContactDetail(c, C6lScreen::OtherContacts);
        return true;
      }
      return false;
    }

    if (screen == C6lScreen::ContactDetail) {
      ContactInfo c;
      if (!detail_has_key || !lookupDetailContact(mesh, detail_pub, c)) {
        screen = contact_detail_from;
        return true;
      }
      int rows = contactDetailRowCount(c);
      if (!select) {
        detail_cursor = (detail_cursor + 1) % rows;
        return false;
      }
      if (detail_cursor == 0) {
        bool next = !c6lContactIsFavorite(c);
        if (mesh.setContactFavorite(detail_pub, next)) {
          showFlash(next ? "Fav ON" : "Fav OFF", 800, now);
        }
        return true;
      }
      return false;
    }

    if (screen == C6lScreen::Advert) {
      AdvertPath recent[ADVERT_PATH_TABLE_SIZE];
      int chat_n = collectChatAdverts(mesh, recent);
      if (chat_n <= 0) {
        showPopup("No one around", now, true);
        return false;
      }
      int map[ADVERT_PATH_TABLE_SIZE + 1];
      int rows = buildNodesRows(mesh, recent, chat_n, map, ADVERT_PATH_TABLE_SIZE + 1);
      if (rows <= 0) return false;
      if (!select) {
        list_cursor = (list_cursor + 1) % rows;
        return false;
      }
      int ai = map[list_cursor];
      if (ai < 0) return true;  // separator
      if (ai >= 0 && ai < chat_n) {
        int rc = mesh.addContactFromRecent(recent[ai].pub_key);
        if (rc == 1) showFlash("added", 1000, now);
        else if (rc == 2) showFlash("known", 1000, now);
        else showFlash("fail", 1000, now);
      }
      return true;
    }

    if (screen == C6lScreen::Status) {
      return false;
    }

    if (screen == C6lScreen::Settings) {
      if (!select) {
        settings_cursor = (settings_cursor + 1) % settingsCount();
        return false;
      }
      switch (settings_cursor) {
        case 0:
          screen = C6lScreen::LoRaSettings;
          lora_cursor = 0;
          break;
        case 1:
          screen = C6lScreen::IfaceMenu;
          iface_cursor = 0;
          break;
        case 2:
          screen = C6lScreen::PathHash;
          path_cursor = prefs ? prefs->path_hash_mode : 0;
          if (path_cursor > 2) path_cursor = 0;
          break;
        case 3:
          if (want_advert) *want_advert = true;
          break;
        case 4:
          openFactoryConfirm();
          break;
      }
      return true;
    }

    if (screen == C6lScreen::IfaceMenu) {
      if (!select) {
        iface_cursor = (iface_cursor + 1) % C6L_IFACE_N;
        return false;
      }
      switch (iface_cursor) {
        case 0:
          openPickMode(ui);
          break;
        case 1:
          openBlePinModal();
          break;
        case 2:
          openWifiStatusModal(ui);
          break;
        case 3:
          openPickUsbDbg(ui);
          break;
      }
      return true;
    }

    if (screen == C6lScreen::LoRaSettings) {
      if (!select) {
        lora_cursor = (lora_cursor + 1) % C6L_LORA_N;
        return false;
      }
      if (lora_cursor == 0) {
        if (prefs && prefs->isRepeatEn()) {
          showFlash("locked", 800, now);
          return true;
        }
        screen = C6lScreen::PresetList;
        preset_cursor = (ui.last_preset >= 0) ? ui.last_preset : 0;
        return true;
      }
      if (want_repeat) *want_repeat = true;
      return true;
    }

    if (screen == C6lScreen::PresetList) {
      // Belt-and-suspenders: preset picker is locked while client-repeat is on
      if (prefs && prefs->isRepeatEn()) {
        screen = C6lScreen::LoRaSettings;
        showFlash("locked", 800, now);
        return true;
      }
      if (!select) {
        preset_cursor = (preset_cursor + 1) % C6L_PRESET_N;
        return false;
      }
      if (applied_preset) *applied_preset = true;
      return true;
    }

    if (screen == C6lScreen::PathHash) {
      if (!select) {
        path_cursor = (path_cursor + 1) % 3;
        return false;
      }
      if (prefs) {
        prefs->path_hash_mode = (uint8_t)path_cursor;
        mesh.savePrefs();
        showFlash("hash saved", 800, now);
      }
      screen = C6lScreen::Settings;
      return true;
    }

    if (screen == C6lScreen::ModeSetup) {
      if (!select) {
        mode_cursor = (mode_cursor + 1) % 3;
        return false;
      }
      if (applied_mode) *applied_mode = true;
      return true;
    }

    return false;
  }

  bool tickMarquee(DisplayDriver* d, MessageRing& msgs, uint32_t now) {
    if (!d || screen != C6lScreen::MessageDetail || flash_active || modalActive()) return false;
    const MsgRingEntry* m = msgs.get(detail_idx);
    if (!m || !m->text[0]) return false;

    int tw = (int)d->getTextWidth(m->text);
    int maxScroll = tw - TinyListDraw::marqueeAvailWidth();
    if (maxScroll <= 0) {
      if (scroll_px != 0) { scroll_px = 0; return true; }
      return false;
    }
    if (scroll_next_ms == 0) {
      scroll_next_ms = now + SCROLL_PAUSE_MS;
      scroll_paused = true;
      return false;
    }
    if (now < scroll_next_ms) return false;
    if (scroll_paused) {
      scroll_paused = false;
      scroll_next_ms = now + SCROLL_SPEED_MS;
      return false;
    }
    scroll_px++;
    if (scroll_px >= maxScroll) {
      scroll_px = 0;
      scroll_paused = true;
      scroll_next_ms = now + SCROLL_PAUSE_MS;
    } else {
      scroll_next_ms = now + SCROLL_SPEED_MS;
    }
    return true;
  }

  /** Draw one frame into the display buffer (no endFrame / panel push). */
  void paintFrame(DisplayDriver* d, MessageRing& msgs, MyMesh& mesh, bool connected,
                  NodePrefs* prefs, UiPrefs& ui, bool include_modals = true) {
    if (!d) return;
    const bool lit = selectCursorLit(millis());
    d->startFrame();
    drawScreenBody(d, msgs, mesh, connected, prefs, ui, lit);
    if (include_modals) {
      paintModal(d, mesh, lit);
    }
  }

  void paintModal(DisplayDriver* d, MyMesh& mesh, bool lit) {
    if (!d) return;
    switch (modal) {
      case ModalKind::Factory:
        TinyListDraw::drawBorderedOverlay(d, "Reset all? double=OK hold=back");
        break;
      case ModalKind::PickMode: {
        static const char* items[3] = {"BLE", "USB", "WiFi"};
        TinyListDraw::drawBorderedPickList(d, items, 3, modal_cursor, lit);
        break;
      }
      case ModalKind::PickUsbDbg: {
        static const char* items[2] = {"debug ON", "debug OFF"};
        TinyListDraw::drawBorderedPickList(d, items, 2, modal_cursor, lit);
        break;
      }
      case ModalKind::BlePin: {
        char line[28];
        uint32_t pin = mesh.getBLEPin();
        if (pin != 0)
          snprintf(line, sizeof(line), "PIN:%06lu hold=back", (unsigned long)pin);
        else
          snprintf(line, sizeof(line), "PIN:------ hold=back");
        TinyListDraw::drawBorderedOverlay(d, line);
        break;
      }
      case ModalKind::WifiStatus:
        TinyListDraw::drawBorderedOverlay(d, flash_text[0] ? flash_text : "WiFi");
        break;
      default:
        if (flash_active) TinyListDraw::drawBorderedOverlay(d, flash_text);
        break;
    }
  }

  void render(DisplayDriver* d, MessageRing& msgs, MyMesh& mesh, bool connected, NodePrefs* prefs,
              UiPrefs& ui) {
    if (!d) return;
    paintFrame(d, msgs, mesh, connected, prefs, ui, true);
    d->endFrame();
  }

  /**
   * Render parent screen into the display buffer without mutating nav state
   * (for hold-to-back slide preview). Does not push to panel.
   */
  void paintParentPreview(DisplayDriver* d, MessageRing& msgs, MyMesh& mesh, bool connected,
                          NodePrefs* prefs, UiPrefs& ui) {
    if (!d || !canGoBack()) return;
    C6lScreen saved = screen;
    ModalKind saved_modal = modal;
    modal = ModalKind::None;
    screen = parentScreen();
    paintFrame(d, msgs, mesh, connected, prefs, ui, false);
    screen = saved;
    modal = saved_modal;
  }

  void drawScreenBody(DisplayDriver* d, MessageRing& msgs, MyMesh& mesh, bool connected,
                      NodePrefs* prefs, UiPrefs& ui, bool lit) {
    if (screen == C6lScreen::GestureHelp) {
      char lines[3][16];
      const char* pl[3];
      if (help_page == 0) {
        strncpy(lines[0], "Buttons", sizeof(lines[0]));
        strncpy(lines[1], "click=next", sizeof(lines[1]));
        strncpy(lines[2], "dbl=select", sizeof(lines[2]));
      } else {
        strncpy(lines[0], "hold=back", sizeof(lines[0]));
        strncpy(lines[1], "x3=silent", sizeof(lines[1]));
        strncpy(lines[2], "dbl=OK", sizeof(lines[2]));
      }
      pl[0] = lines[0]; pl[1] = lines[1]; pl[2] = lines[2];
      TinyListDraw::renderLines(d, pl, 3);
      return;
    }

    if (screen == C6lScreen::PresetSetup || screen == C6lScreen::PresetList) {
      const char* items[C6L_PRESET_N];
      for (int i = 0; i < C6L_PRESET_N; i++) items[i] = C6L_PRESETS[i].label;
      TinyListDraw::renderList(d, items, C6L_PRESET_N, preset_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::Menu) {
      TinyListDraw::renderList(d, C6L_MENU, C6L_MENU_N, menu_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::Messages) {
      if (msgs.count() == 0) return;  // empty popup drawn as overlay by render()
      char bufs[C6L_MSG_RING_SIZE][16];
      const char* items[C6L_MSG_RING_SIZE];
      TinyRowStyle styles[C6L_MSG_RING_SIZE];
      int n = 0;
      for (int i = 0; i < msgs.count(); i++) {
        const MsgRingEntry* m = msgs.get(i);
        char name[16];
        const char* src = m->is_dm ? m->name : m->ch;
        TinyListDraw::clip(name, sizeof(name), src, 14);
        strncpy(bufs[n], name, sizeof(bufs[n]));
        items[n] = bufs[n];
        styles[n] = m->unread ? TinyRowStyle::Unread : TinyRowStyle::Plain;
        n++;
      }
      TinyListDraw::renderList(d, items, styles, n, list_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::MessageDetail) {
      const MsgRingEntry* m = msgs.get(detail_idx);
      char rows[2][16];
      TinyRowStyle styles[2] = {TinyRowStyle::ForwardBtn, TinyRowStyle::Plain};
      strncpy(rows[0], "REPLY", sizeof(rows[0]));
      if (m) {
        char peer[12];
        TinyListDraw::clip(peer, sizeof(peer), m->name, 10);
        snprintf(rows[1], sizeof(rows[1]), "%s %s", peer, m->ch);
      } else {
        strncpy(rows[1], "?", sizeof(rows[1]));
      }
      TinyListDraw::drawStyledRow(d, rows[0], 0, (msg_cursor == 0) && lit, styles[0]);
      TinyListDraw::drawStyledRow(d, rows[1], 1, (msg_cursor == 1) && lit, styles[1]);
      const char* body = (m && m->text[0]) ? m->text : "";
      TinyListDraw::drawMarqueeRow(d, body, 2, (msg_cursor == 2) && lit, scroll_px);
      return;
    }

    if (screen == C6lScreen::Reply) {
      char bufs[C6L_CANNED_N][16];
      const char* items[C6L_CANNED_N];
      for (int i = 0; i < C6L_CANNED_N; i++) {
        TinyListDraw::clip(bufs[i], sizeof(bufs[i]), C6L_CANNED[i].label, 14);
        items[i] = bufs[i];
      }
      TinyListDraw::renderList(d, items, C6L_CANNED_N, reply_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::Contacts) {
      if (mesh.getNumContacts() == 0) return;
      const int max_show = 24;
      char bufs[max_show][16];
      const char* items[max_show];
      int fav_n = countFavorites(mesh);
      int n = 0;
      for (int i = 0; i < fav_n && n < max_show - 1; i++) {
        ContactInfo c;
        if (!getFavoriteContact(mesh, i, c)) continue;
        TinyListDraw::clip(bufs[n], sizeof(bufs[n]), c.name[0] ? c.name : "?", 14);
        items[n] = bufs[n];
        n++;
      }
      strncpy(bufs[n], "other contacts", sizeof(bufs[n]));
      items[n] = bufs[n];
      n++;
      TinyListDraw::renderList(d, items, n, list_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::OtherContacts) {
      const int max_show = 24;
      char bufs[max_show][16];
      const char* items[max_show];
      int other_n = countOthers(mesh);
      int n = 0;
      strncpy(bufs[n], "erase list", sizeof(bufs[n]));
      items[n] = bufs[n];
      n++;
      for (int i = 0; i < other_n && n < max_show; i++) {
        ContactInfo c;
        if (!getOtherContact(mesh, i, c)) continue;
        TinyListDraw::clip(bufs[n], sizeof(bufs[n]), c.name[0] ? c.name : "?", 14);
        items[n] = bufs[n];
        n++;
      }
      TinyListDraw::renderList(d, items, n, list_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::ContactDetail) {
      ContactInfo c;
      char lines[6][16];
      const char* pl[6];
      int n = 0;
      if (!detail_has_key || !lookupDetailContact(mesh, detail_pub, c)) {
        strncpy(lines[0], "?", sizeof(lines[0]));
        pl[0] = lines[0];
        TinyListDraw::renderLines(d, pl, 1);
        return;
      }
      snprintf(lines[n], sizeof(lines[n]), "Fav %s", c6lContactIsFavorite(c) ? "ON" : "OFF");
      pl[n] = lines[n]; n++;
      TinyListDraw::clip(lines[n], sizeof(lines[n]), c.name[0] ? c.name : "?", 14);
      pl[n] = lines[n]; n++;
      // pubkey: first 8 bytes as hex
      snprintf(lines[n], sizeof(lines[n]), "%02x%02x%02x%02x%02x%02x%02x%02x",
               c.id.pub_key[0], c.id.pub_key[1], c.id.pub_key[2], c.id.pub_key[3],
               c.id.pub_key[4], c.id.pub_key[5], c.id.pub_key[6], c.id.pub_key[7]);
      pl[n] = lines[n]; n++;
      if (c.out_path_len == OUT_PATH_UNKNOWN)
        strncpy(lines[n], "hops ?", sizeof(lines[n]));
      else if (c.out_path_len == 0)
        strncpy(lines[n], "direct", sizeof(lines[n]));
      else
        snprintf(lines[n], sizeof(lines[n]), "hops %u", (unsigned)c.out_path_len);
      pl[n] = lines[n]; n++;
      if (c.gps_lat != 0 || c.gps_lon != 0) {
        snprintf(lines[n], sizeof(lines[n]), "%.2f %.2f",
                 (double)c.gps_lat / 1000000.0, (double)c.gps_lon / 1000000.0);
        pl[n] = lines[n]; n++;
      }
      // Draw as selectable list so Fav row highlights
      TinyListDraw::renderList(d, pl, n, detail_cursor, TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::Advert) {
      AdvertPath recent[ADVERT_PATH_TABLE_SIZE];
      int chat_n = collectChatAdverts(mesh, recent);
      if (chat_n == 0) return;
      int map[ADVERT_PATH_TABLE_SIZE + 1];
      int rows = buildNodesRows(mesh, recent, chat_n, map, ADVERT_PATH_TABLE_SIZE + 1);
      char bufs[ADVERT_PATH_TABLE_SIZE + 1][16];
      const char* items[ADVERT_PATH_TABLE_SIZE + 1];
      int n = 0;
      uint32_t tnow = mesh.getRTCClock()->getCurrentTime();
      for (int i = 0; i < rows; i++) {
        if (map[i] < 0) {
          strncpy(bufs[n], "----", sizeof(bufs[n]));
        } else {
          int ai = map[i];
          char ages[8];
          uint32_t age = (tnow > recent[ai].recv_timestamp) ? (tnow - recent[ai].recv_timestamp) : 0;
          formatRelativeAge(age, ages, sizeof(ages));
          char name[12];
          TinyListDraw::clip(name, sizeof(name), recent[ai].name, 10);
          char line[16];
          snprintf(line, sizeof(line), "%s %s", name, ages);
          TinyListDraw::clip(bufs[n], sizeof(bufs[n]), line, 14);
        }
        items[n] = bufs[n];
        n++;
      }
      TinyListDraw::renderList(d, items, n, list_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::Status) {
      char lines[5][16];
      const char* pl[5];
      int n = 0;
      char id[8];
      snprintf(id, sizeof(id), "%02x%02x", mesh.self_id.pub_key[0], mesh.self_id.pub_key[1]);
      snprintf(lines[n], sizeof(lines[n]), "node %s", id);
      pl[n] = lines[n]; n++;
      char name[16];
      TinyListDraw::clip(name, sizeof(name), prefs ? prefs->node_name : "?", 14);
      strncpy(lines[n], name, sizeof(lines[n]));
      pl[n] = lines[n]; n++;
      snprintf(lines[n], sizeof(lines[n]), "%d peers", mesh.getNumContacts());
      pl[n] = lines[n]; n++;
      if (prefs)
        snprintf(lines[n], sizeof(lines[n]), "%.1f/%d", prefs->freq, (int)prefs->sf);
      else
        strncpy(lines[n], "LoRa", sizeof(lines[n]));
      pl[n] = lines[n]; n++;
#if defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID)
      if (ui.serial_mode == 2 && WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(lines[n], sizeof(lines[n]), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        pl[n] = lines[n]; n++;
      } else
#endif
      if (!connected) {
        uint32_t pin = mesh.getBLEPin();
        if (pin != 0) {
          snprintf(lines[n], sizeof(lines[n]), "PIN:%06lu", (unsigned long)pin);
          pl[n] = lines[n]; n++;
        }
      } else {
        snprintf(lines[n], sizeof(lines[n]), "%d DM unread", msgs.unreadDmCount());
        pl[n] = lines[n]; n++;
      }
      TinyListDraw::renderLines(d, pl, n);
      return;
    }

    if (screen == C6lScreen::Settings) {
      char bufs[C6L_SETTINGS_N][16];
      const char* items[C6L_SETTINGS_N];
      for (int i = 0; i < C6L_SETTINGS_N; i++) {
        settingsLabel(i, bufs[i], sizeof(bufs[i]), prefs, ui);
        items[i] = bufs[i];
      }
      TinyListDraw::renderList(d, items, C6L_SETTINGS_N, settings_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::IfaceMenu) {
      char bufs[C6L_IFACE_N][16];
      const char* items[C6L_IFACE_N];
      for (int i = 0; i < C6L_IFACE_N; i++) {
        ifaceLabel(i, bufs[i], sizeof(bufs[i]), ui);
        items[i] = bufs[i];
      }
      TinyListDraw::renderList(d, items, C6L_IFACE_N, iface_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::LoRaSettings) {
      char bufs[C6L_LORA_N][16];
      const char* items[C6L_LORA_N];
      for (int i = 0; i < C6L_LORA_N; i++) {
        loraLabel(i, bufs[i], sizeof(bufs[i]), prefs, ui);
        items[i] = bufs[i];
      }
      TinyListDraw::renderList(d, items, C6L_LORA_N, lora_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::PathHash) {
      TinyListDraw::renderList(d, C6L_HASH_LABELS, 3, path_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }

    if (screen == C6lScreen::ModeSetup) {
      static const char* items[3] = {"BLE", "USB", "WiFi"};
      TinyListDraw::renderList(d, items, 3, mode_cursor,
                               TinyListDraw::MAX_ROWS, lit);
      return;
    }
  }
};
