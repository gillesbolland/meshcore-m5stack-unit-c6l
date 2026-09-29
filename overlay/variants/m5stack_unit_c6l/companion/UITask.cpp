#include "UITask.h"
#include <target.h>
#include <string.h>
#include "../../../examples/companion_radio/MyMesh.h"
#include "ui/BootLogo.h"
#include "ui/RadioPresets.h"
#include "ui/TinyListDraw.h"

#if defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID)
  #include <WiFi.h>
#endif

#if defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID)
  #include <helpers/esp32/SerialWifiInterface.h>
  extern SerialWifiInterface wifi_interface;
#ifndef TCP_PORT
  #define TCP_PORT 5000
#endif

static bool s_wifi_tcp_begun = false;

static void c6lEnsureWifiTcp() {
  if (!s_wifi_tcp_begun) {
    wifi_interface.begin(TCP_PORT);
    s_wifi_tcp_begun = true;
  }
}
#endif

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  (void)sensors;
  _display = display;
  _node_prefs = node_prefs;
  _asleep = false;
  _need_refresh = true;
  _booted = false;
  _pending_channel = false;
  _reboot_at = 0;
  _last_activity = millis();
  _gestures.reset();
  _msgs.clear();
  _nav = ScreenNav();

  _ui_prefs.load();
  _nav.initFromPrefs(_ui_prefs);

#ifdef PIN_BUZZER
  _buzzer.begin();
  if (_node_prefs) {
    _buzzer.quiet(_node_prefs->buzzer_quiet != 0);
    _led.setStealth(_node_prefs->buzzer_quiet != 0);
  }
#endif

#ifdef P_LORA_TX_NEOPIXEL_LED
  _led.begin(P_LORA_TX_NEOPIXEL_LED);
#else
  _led.begin(2);
#endif

  applySerialMode();

  if (_display) {
    _display->turnOn();
    // Skip logo on first-boot setup gates — it plays after Mode select reboot.
    if (_ui_prefs.radio_setup_done) {
      BootLogo::play(_display, 1000);
      playStartup();
    }
    _booted = true;
    renderNow();
  }
}

void UITask::applySerialMode() {
  const uint8_t mode = _ui_prefs.serial_mode;

#if defined(BLE_PIN_CODE)
  if (mode == 0) enableBluetooth();
  else disableBluetooth();
#endif

#if defined(ENABLE_USB_INTERFACE)
  if (mode == 1 || _ui_prefs.usb_debug) enableUsb();
  else disableUsb();
#endif

#if defined(ENABLE_WIFI_INTERFACE) || defined(WIFI_SSID)
  if (mode == 2) {
    if (_c6l_board) _c6l_board->setInhibitSleep(true);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    c6lEnsureWifiTcp();
  #ifdef WIFI_SSID
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PWD);
    }
  #endif
    enableWifi();
  } else {
    // Do not touch the WiFi driver unless it was started (BLE-only boot must stay off WiFi)
    disableWifi();
  }
#else
  (void)mode;
#endif
}

void UITask::playStartup() {
#ifdef PIN_BUZZER
  if (!_led.stealth()) _buzzer.startup();
#endif
}

void UITask::playMsgNotify() {
#ifdef PIN_BUZZER
  if (!_led.stealth()) _buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
#endif
}

void UITask::navBeep() {
  if (_led.stealth()) return;
#ifdef PIN_BUZZER
  if (_c6l_board) _c6l_board->playTone(2000, 15);
#endif
}

void UITask::msgRead(int msgcount) {
  if (msgcount <= 0) {
    _msgs.markAllRead();
  }
  _need_refresh = true;
}

bool UITask::isChannelName(const char* name) {
  if (!name) return false;
#ifdef MAX_GROUP_CHANNELS
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails ch;
    if (!the_mesh.getChannel(i, ch)) continue;
    if (ch.name[0] && strcmp(ch.name, name) == 0) return true;
  }
#endif
  return false;
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  (void)path_len;
  (void)msgcount;

  bool is_dm = true;
  if (_pending_channel) {
    is_dm = false;
    _pending_channel = false;
  } else if (isChannelName(from_name)) {
    is_dm = false;
  } else if (the_mesh.searchContactsByPrefix(from_name) == nullptr && from_name && from_name[0] == '#') {
    is_dm = false;
  }

  _msgs.setPendingKind(is_dm);
  _msgs.onNewMsg(from_name, text);

  if (is_dm) {
    ContactInfo* c = the_mesh.searchContactsByPrefix(from_name);
    if (c) {
      _msgs.setPubPrefix(0, c->id.pub_key, 7);
    }
    playMsgNotify();
    wake(millis());
  }

  _need_refresh = true;
}

void UITask::notify(UIEventType t) {
  if (t == UIEventType::channelMessage) {
    _pending_channel = true;
  }
  _need_refresh = true;
}

void UITask::wake(uint32_t now) {
  _last_activity = now;
  if (_asleep) {
    _asleep = false;
    if (_display) _display->turnOn();
    _need_refresh = true;
  }
}

void UITask::sleepScreen() {
  _asleep = true;
  if (_display) _display->turnOff();
  _led.off();
}

void UITask::toggleStealth(uint32_t now) {
  bool next = !_led.stealth();
  _led.setStealth(next);
#ifdef PIN_BUZZER
  _buzzer.quiet(next);
#endif
  if (_node_prefs) {
    _node_prefs->buzzer_quiet = next ? 1 : 0;
    the_mesh.savePrefs();
  }
  if (!next) navBeep();
  _nav.showFlash(next ? "silent" : "audible", 1000, now);
  _need_refresh = true;
}

void UITask::doLocalAdvert(uint32_t now) {
  the_mesh.advert();
  _nav.showFlash("local advert", 1200, now);
  _need_refresh = true;
}

SSD1306SPIDisplay* UITask::oled() {
  return static_cast<SSD1306SPIDisplay*>(_display);
}

void UITask::captureCurrentFrame() {
  SSD1306SPIDisplay* d = oled();
  if (!d) return;
  _nav.paintFrame(d, _msgs, the_mesh, _connected, _node_prefs, _ui_prefs, true);
  _slide.captureFrom(d);
}

void UITask::captureParentFrame() {
  SSD1306SPIDisplay* d = oled();
  if (!d) return;
  _nav.paintParentPreview(d, _msgs, the_mesh, _connected, _node_prefs, _ui_prefs);
  _slide.captureTo(d);
}

void UITask::prepareHoldBackPair() {
  captureCurrentFrame();
  captureParentFrame();
}

void UITask::playForwardSlideIfNeeded(C6lScreen before) {
  if (!_display || before == _nav.screen) return;
  SSD1306SPIDisplay* d = oled();
  if (!d) return;
  _nav.paintFrame(d, _msgs, the_mesh, _connected, _node_prefs, _ui_prefs, true);
  _slide.captureTo(d);
  _slide.playForward(d, FORWARD_SLIDE_MS);
}

void UITask::startHoldBack(uint32_t now) {
  if (!_nav.canGoBack()) return;
  // Interactive modal: long-hold cancels immediately (no slide).
  if (_nav.modalActive()) {
    _nav.goBack();
    _need_refresh = true;
    return;
  }
  _nav.dismissPopup();
  prepareHoldBackPair();
  _hold_back_active = true;
  _hold_back_start = now;
  _hold_back_duration = HOLD_BACK_FIRST_MS;
  if (SSD1306SPIDisplay* d = oled()) _slide.blit(d, 0.f, true);
  _need_refresh = true;
}

void UITask::cancelHoldBack() {
  if (!_hold_back_active) return;
  _hold_back_active = false;
  _need_refresh = true;
}

void UITask::tickHoldBack(bool pressed, uint32_t now) {
  if (!_hold_back_active) return;
  if (!pressed) {
    cancelHoldBack();
    renderNow();
    return;
  }
  _last_activity = now;
  uint32_t elapsed = now - _hold_back_start;
  float progress = 0.f;
  if (_hold_back_duration > 0) {
    progress = (float)elapsed / (float)_hold_back_duration;
    if (progress > 1.f) progress = 1.f;
  }
  SSD1306SPIDisplay* d = oled();
  if (d) _slide.blit(d, progress, true);

  if (elapsed >= _hold_back_duration) {
    if (_nav.goBack()) navBeep();
    if (!_nav.canGoBack()) {
      cancelHoldBack();
      renderNow();
    } else {
      // Chain next back while still held — twice as fast
      _hold_back_duration = _hold_back_duration / 2;
      if (_hold_back_duration < HOLD_BACK_MIN_MS) _hold_back_duration = HOLD_BACK_MIN_MS;
      _hold_back_start = now;
      prepareHoldBackPair();
      if (d) _slide.blit(d, 0.f, true);
    }
  }
}

void UITask::runPendingSelect(uint32_t now) {
  _select_pending = false;
  C6lScreen before = _nav.screen;
  // Snapshot outgoing screen (post-blink highlight) before mutate
  captureCurrentFrame();

  int preset_cur = _nav.preset_cursor;
  bool need_reboot = false;
  bool applied_preset = false;
  bool want_advert = false;
  bool want_factory = false;
  bool want_repeat = false;
  bool applied_mode = false;
  if (_nav.handle(BtnEvent::Double, _msgs, the_mesh, _connected, now,
                  _node_prefs, _ui_prefs, &need_reboot, &applied_preset, &want_advert,
                  &want_factory, &want_repeat, &applied_mode)) {
    // beep already played when blink started
  }
  if (want_advert) {
    doLocalAdvert(now);
  } else if (applied_preset) {
    applyRadioPreset(preset_cur, before == C6lScreen::PresetSetup, now);
  } else if (applied_mode && before == C6lScreen::ModeSetup) {
    completeFirstBootMode(now);
  } else if (want_repeat) {
    toggleClientRepeat(now);
  } else if (want_factory) {
    // Farewell on blank bg, then play the tune while it stays visible
    if (_display) {
      _display->startFrame();
      TinyListDraw::drawBorderedOverlay(_display, "Andy Kirby is a thief");
      _display->endFrame();
    }
    playFactoryTune();
#ifdef PIN_BUZZER
    {
      uint32_t cap = millis() + 5000;
      while (_buzzer.isPlaying() && millis() < cap) {
        _buzzer.loop();
        delay(5);
      }
    }
#else
    delay(1400);
#endif
    factoryReset();
  } else if (need_reboot) {
    _reboot_at = now + 1000;
  }

  playForwardSlideIfNeeded(before);
  _need_refresh = true;
}

void UITask::doQuietLocalAdvert() {
  the_mesh.advert();
}

void UITask::applyRadioPreset(int idx, bool from_setup, uint32_t now) {
  if (!_node_prefs || idx < 0 || idx >= C6L_PRESET_N) return;
  const RadioPreset& p = C6L_PRESETS[idx];
  _node_prefs->freq = p.freq;
  _node_prefs->sf = p.sf;
  _node_prefs->bw = p.bw;
  _node_prefs->cr = p.cr;
  // On first-boot, path hash is chosen next — don't apply preset's hash bytes yet
  if (!from_setup && p.path_hash_bytes >= 1 && p.path_hash_bytes <= 3) {
    _node_prefs->path_hash_mode = (uint8_t)(p.path_hash_bytes - 1);
  }
  radio_driver.setParams(_node_prefs->freq, _node_prefs->bw, _node_prefs->sf, _node_prefs->cr);
  the_mesh.savePrefs();

  _ui_prefs.last_preset = (int8_t)idx;
  _ui_prefs.save();

  _nav.showFlash("preset OK", 900, now);
  if (from_setup) {
    // First-boot: default 2B path hash (no hash picker); next is Mode
    if (_node_prefs) {
      _node_prefs->path_hash_mode = 1;
      the_mesh.savePrefs();
    }
    _nav.mode_cursor = 1;  // USB default
    _nav.screen = C6lScreen::ModeSetup;
  } else {
    _nav.screen = C6lScreen::LoRaSettings;
  }
  _need_refresh = true;
}

void UITask::completeFirstBootMode(uint32_t now) {
  uint8_t mode = (uint8_t)_nav.mode_cursor;
  if (mode > 2) mode = 1;
  _ui_prefs.serial_mode = mode;
  _ui_prefs.radio_setup_done = 1;
  _ui_prefs.save();
  applySerialMode();
  doQuietLocalAdvert();
  // Stay on ModeSetup (no Menu) — reboot lands on Menu after prefs load
  char line[16];
  snprintf(line, sizeof(line), "%s mode", c6lSerialModeLabel(mode));
  _nav.showFlash(line, 900, now);
  _reboot_at = now + 1200;
  _need_refresh = true;
}

void UITask::toggleClientRepeat(uint32_t now) {
  if (!_node_prefs) return;

  if (_node_prefs->isRepeatEn()) {
    // Restore prior regular preset / modem from stash
    if (_ui_prefs.has_radio_stash) {
      _node_prefs->freq = _ui_prefs.stash_freq;
      _node_prefs->bw = _ui_prefs.stash_bw;
      _node_prefs->sf = _ui_prefs.stash_sf;
      _node_prefs->cr = _ui_prefs.stash_cr;
      radio_driver.setParams(_node_prefs->freq, _node_prefs->bw, _node_prefs->sf, _node_prefs->cr);
      if (_ui_prefs.stash_preset >= 0) _ui_prefs.last_preset = _ui_prefs.stash_preset;
      _ui_prefs.has_radio_stash = 0;
    }
    _node_prefs->setRepeatEn(false);
    the_mesh.savePrefs();
    _ui_prefs.save();
    _nav.showFlash("Repeat OFF", 900, now);
  } else {
    // Stash current radio, enable MeshCore client-repeat, switch to CR channel
    _ui_prefs.stash_freq = _node_prefs->freq;
    _ui_prefs.stash_bw = _node_prefs->bw;
    _ui_prefs.stash_sf = _node_prefs->sf;
    _ui_prefs.stash_cr = _node_prefs->cr;
    _ui_prefs.stash_preset = _ui_prefs.last_preset;
    _ui_prefs.has_radio_stash = 1;

    float cr_freq = c6lClientRepeatFreqFor(_node_prefs->freq);
    _node_prefs->freq = cr_freq;
    _node_prefs->sf = C6L_CR_SF;
    _node_prefs->bw = C6L_CR_BW;
    _node_prefs->cr = C6L_CR_CR;
    _node_prefs->setRepeatEn(true);
    radio_driver.setParams(_node_prefs->freq, _node_prefs->bw, _node_prefs->sf, _node_prefs->cr);
    the_mesh.savePrefs();
    _ui_prefs.save();

    char line[16];
    snprintf(line, sizeof(line), "CR %.3f", (double)cr_freq);
    _nav.showFlash(line, 900, now);
  }
  _need_refresh = true;
}

void UITask::factoryReset() {
  // Same wipe as app CMD_FACTORY_RESET: FS + NVS. Boot then mints a new pubkey
  // and UiPrefs is gone → first-boot preset/hash gate.
  if (!the_mesh.factoryReset()) {
    _nav.showFlash("reset fail", 1200, millis());
    _need_refresh = true;
    return;
  }
  delay(200);
  ESP.restart();
}

void UITask::playFactoryTune() {
#ifdef PIN_BUZZER
  // Force play even in stealth: unmute briefly so play() accepts the melody.
  const bool was_quiet = _buzzer.isQuiet();
  _buzzer.quiet(false);
  // Trailing rest so the driver releases the pin cleanly before wipe.
  _buzzer.play("r:d=4,o=5,b=380:8g,8a,8c6,8a,e6,8p,e6,8p,d6.,p,8p,8g,8a,8c6,8a,d6,8p,d6,8p,c6,8b,a.,32p");
  _buzzer.quiet(was_quiet);  // restore stealth mute; current RTTTL keeps running
#endif
}

void UITask::renderNow() {
  if (!_display || _asleep) return;
  if (_hold_back_active) {
    // Hold-back draws via tickHoldBack slide blit; avoid clobbering mid-slide.
    return;
  }
  _nav.render(_display, _msgs, the_mesh, _connected, _node_prefs, _ui_prefs);
  _need_refresh = false;
}

void UITask::loop() {
  if (!_display) return;

  uint32_t now = millis();

  if (_reboot_at != 0 && now >= _reboot_at) {
    ESP.restart();
  }

#ifdef PIN_BUZZER
  if (_buzzer.isPlaying()) _buzzer.loop();
#endif

  if (_nav.consumeFlash(now)) {
    _need_refresh = true;
  }

  if (!_asleep && !_hold_back_active && _nav.tickMarquee(_display, _msgs, now)) {
    _need_refresh = true;
  }

  // Finish deferred select after highlight blink
  if (_select_pending) {
    if (_nav.tickSelectBlink(now)) {
      runPendingSelect(now);
      renderNow();
    } else if (_nav.selectBlinkActive()) {
      _need_refresh = true;
    }
  }

  bool pressed = board.isButtonPressed();
  BtnEvent ev = _gestures.update(pressed, now);

  tickHoldBack(pressed, now);

  if (ev != BtnEvent::None) {
    bool was_asleep = _asleep;
    wake(now);

    if (ev == BtnEvent::Long) {
      if (!was_asleep && !_select_pending) startHoldBack(now);
      renderNow();
    } else if (ev == BtnEvent::Triple) {
      cancelHoldBack();
      _nav.cancelSelectBlink();
      _select_pending = false;
      toggleStealth(now);
      renderNow();
    } else if (ev == BtnEvent::Double) {
      // Blink highlighted row, then select
      if (!was_asleep && !_hold_back_active && !_select_pending) {
        _nav.startSelectBlink(now);
        _select_pending = true;
        navBeep();
        _need_refresh = true;
        renderNow();
      } else {
        renderNow();
      }
    } else if (ev == BtnEvent::Click) {
      if (!was_asleep && !_hold_back_active && !_select_pending) {
        bool need_reboot = false;
        bool applied_preset = false;
        // A single click while the factory-reset modal is up cancels it (handled in nav).
        _nav.handle(BtnEvent::Click, _msgs, the_mesh, _connected, now,
                    _node_prefs, _ui_prefs, &need_reboot, &applied_preset);
        navBeep();  // short tick for next-item / modal dismiss
        _need_refresh = true;
        renderNow();
      } else {
        renderNow();
      }
    }
  } else if (!_asleep && !_hold_back_active && !_select_pending &&
             (now - _last_activity) >= SLEEP_MS) {
    sleepScreen();
  }

  _led.tick(now, _msgs.unreadDmCount());

  if (!_asleep && (_need_refresh || _nav.flash_active || _hold_back_active ||
                   _nav.selectBlinkActive())) {
    renderNow();
  }
}
