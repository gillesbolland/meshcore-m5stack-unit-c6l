#pragma once

#include <Arduino.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/SSD1306SPIDisplay.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include "../../../examples/companion_radio/NodePrefs.h"
#include "../../../examples/companion_radio/AbstractUITask.h"
#include "ui/ButtonGestures.h"
#include "ui/MessageRing.h"
#include "ui/DmNotifyLed.h"
#include "ui/ScreenNav.h"
#include "ui/ScreenSlide.h"
#include "ui/UiPrefs.h"

class M5StackUnitC6LBoard;

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

/**
 * Thin AbstractUITask adapter for Unit C6L companion.
 * UI logic lives in companion/ui/* for MeshCore upgrade isolation.
 *
 * Gestures: Click = next; Double = select/enter (right slide); Long-hold = interruptible
 * left-slide go-back; Triple = stealth. Local advert is Settings → Local advert.
 */
class UITask : public AbstractUITask {
public:
  UITask(M5StackUnitC6LBoard* board, MultiSerialInterface* serial)
    : AbstractUITask((mesh::MainBoard*)board, serial), _display(NULL), _c6l_board(board) {}

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  void loop() override;

  void notify(UIEventType t) override;
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;

private:
  void wake(uint32_t now);
  void sleepScreen();
  void navBeep();
  void playMsgNotify();
  void playStartup();
  void toggleStealth(uint32_t now);
  void doLocalAdvert(uint32_t now);
  void applyRadioPreset(int idx, bool from_setup, uint32_t now);
  void completeFirstBootMode(uint32_t now);
  void doQuietLocalAdvert();
  void toggleClientRepeat(uint32_t now);
  void factoryReset();
  void playFactoryTune();
  void applySerialMode();
  void renderNow();
  void captureCurrentFrame();
  void captureParentFrame();
  void playForwardSlideIfNeeded(C6lScreen before);
  void prepareHoldBackPair();
  void startHoldBack(uint32_t now);
  void cancelHoldBack();
  void tickHoldBack(bool pressed, uint32_t now);
  void runPendingSelect(uint32_t now);
  bool isChannelName(const char* name);
  SSD1306SPIDisplay* oled();

  DisplayDriver* _display;
  M5StackUnitC6LBoard* _c6l_board;
  NodePrefs* _node_prefs;
  UiPrefs _ui_prefs;

  ButtonGestures _gestures;
  MessageRing _msgs;
  DmNotifyLed _led;
  ScreenNav _nav;
  ScreenSlide _slide;

#ifdef PIN_BUZZER
  genericBuzzer _buzzer;
#endif

  bool _asleep = false;
  uint32_t _last_activity = 0;
  bool _need_refresh = true;
  bool _booted = false;
  bool _pending_channel = false;
  uint32_t _reboot_at = 0;

  // Long-hold go-back slide (interruptible)
  bool _hold_back_active = false;
  uint32_t _hold_back_start = 0;
  uint32_t _hold_back_duration = 0;

  // Deferred double-click select (after highlight blink)
  bool _select_pending = false;

  static constexpr uint32_t SLEEP_MS = 30000;
  static constexpr uint32_t HOLD_BACK_FIRST_MS = 1000;
  static constexpr uint32_t HOLD_BACK_MIN_MS = 250;
  static constexpr uint32_t FORWARD_SLIDE_MS = 280;
};
