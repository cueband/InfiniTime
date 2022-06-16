#include "cueband.h"

#ifdef CUEBAND_APP_ENABLED

#include "CueBandApp.h"
#include <lvgl/lvgl.h>
#include "displayapp/DisplayApp.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/BatteryIcon.h"

using namespace Pinetime::Applications::Screens;

#define UNITS_X_OFFSET 80
#define UNITS_Y_OFFSET -5
#define DURATION_Y_OFFSET -25

#define SCREEN_TIMEOUT 25

static const uint32_t snoozeDurations[] = { 10 * 60, 30 * 60, 60 * 60, 0 };
static const uint32_t impromptuDurations[] = { 10 * 60, 30 * 60, 60 * 60, 0 };
static const uint32_t promptIntervals[] = { 30, 60, 90, 120, 180, 240, 0 };
static const uint32_t promptStyles[] = { 1, 2, 3, 4, 5, 6, 7, 0 };
static const char *const promptDescription[] = {
  // Proper descriptions for each style
  "Silent",  // 0
  "Short",   // 1
  "Medium",  // 2
  "Long",    // 3
  "2xShort", // 4
  "2xLong",  // 5
  "3xShort", // 6
  "3xLong",  // 7
};


/*
static uint32_t cycleDuration(const uint32_t *durations, uint32_t current, uint32_t margin = 30) {
  for (const uint32_t *duration = durations; *duration != 0; duration++) {
    if (current + margin < *duration) return *duration;
  }
  return durations[0];
}
*/

static uint32_t nextDuration(const uint32_t *durations, uint32_t current, uint32_t margin = 30) {
  uint32_t last = durations[0];
  for (const uint32_t *duration = durations; *duration != 0; duration++) {
    if (current + margin < *duration) return *duration;
    last = *duration;
  }
  return last;
}

static uint32_t prevDuration(const uint32_t *durations, uint32_t current) {
  uint32_t prev = 0;
  for (const uint32_t *duration = durations; *duration != 0; duration++) {
    if (current <= *duration) return prev;
    prev = *duration;
  }
  return prev;
}

static bool atMaxDuration(const uint32_t *durations, uint32_t current, uint32_t margin = 30) {
  uint32_t next = nextDuration(durations, current, margin);
  return current + margin >= next;
}

static bool atMinDuration(const uint32_t *durations, uint32_t current) {
  uint32_t prev = prevDuration(durations, current);
  return prev == 0;
}

static void ButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
  auto* screen = static_cast<CueBandApp*>(obj->user_data);
  screen->OnButtonEvent(obj, event);
}

static void lv_update_task(struct _lv_task_t* task) {
  auto user_data = static_cast<CueBandApp*>(task->user_data);
  user_data->Update();
}

CueBandApp::CueBandApp(
             CueBandScreen screen,
             Pinetime::Applications::DisplayApp* app,
             System::SystemTask& systemTask,
             Pinetime::Controllers::Battery& batteryController,
             Controllers::DateTime& dateTimeController,
             Controllers::Settings& settingsController
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
             )
  : Screen(app),
    systemTask {systemTask}, 
    batteryController {batteryController},
    dateTimeController {dateTimeController},
    settingsController {settingsController}
#ifdef CUEBAND_CUE_ENABLED
    , cueController {cueController}
#endif
   {
  this->screen = screen;

#ifdef CUEBAND_CUE_ENABLED
  isManualAllowed = cueController.IsManualAllowed();
#endif

  // Padding etc.
  static constexpr uint8_t innerDistance = 10;
  static constexpr uint8_t barHeight = 20 + innerDistance;
  static constexpr uint8_t buttonHeight = (LV_VER_RES_MAX - barHeight - innerDistance) / 2;
  static constexpr uint8_t buttonWidth = (LV_HOR_RES_MAX - innerDistance) / 2; // wide buttons
  //static constexpr uint8_t buttonWidth = buttonHeight; // square buttons
  static constexpr uint8_t buttonXOffset = (LV_HOR_RES_MAX - buttonWidth * 2 - innerDistance) / 2;

  // Background label
  backgroundLabel = lv_label_create(lv_scr_act(), nullptr);
  backgroundLabel->user_data = this;
  lv_obj_set_click(backgroundLabel, true);
  lv_label_set_long_mode(backgroundLabel, LV_LABEL_LONG_CROP);
  lv_obj_set_size(backgroundLabel, 240, 240);
  lv_obj_set_pos(backgroundLabel, 0, 0);
  lv_label_set_text_static(backgroundLabel, "");
  lv_obj_set_event_cb(backgroundLabel, ButtonEventHandler);

  // Time
  label_time = lv_label_create(lv_scr_act(), nullptr);
#if defined(CUEBAND_CUSTOMIZATION_NO_INVALID_TIME) && defined(CUEBAND_DETECT_UNSET_TIME)
  if (dateTimeController.IsUnset()) {
    lv_label_set_text_fmt(label_time, "");
  } else
#endif
  lv_label_set_text(label_time, dateTimeController.FormattedTime().c_str());
  lv_label_set_align(label_time, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(label_time, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);

  // Battery
  batteryIcon.Create(lv_scr_act());
  lv_obj_align(batteryIcon.GetObject(), nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

  // Cueing information icon
  lInfoIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(lInfoIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
  lv_obj_set_style_local_text_font(lInfoIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_20);
  lv_label_set_text(lInfoIcon, "");
  lv_obj_align(lInfoIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, buttonXOffset, barHeight);

  // Cueing information label
  lInfo = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_recolor(lInfo, true);
  lv_label_set_text_fmt(lInfo, "-");
  lv_obj_align(lInfo, nullptr, LV_ALIGN_IN_TOP_LEFT, buttonXOffset + 32, barHeight);
  lv_label_set_long_mode(lInfo, LV_LABEL_LONG_EXPAND);

  // Large duration value
  duration = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(duration, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_76);
  //lv_obj_set_style_local_text_color(duration, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  lv_label_set_text(duration, "");  // "00:00"
  lv_obj_align(duration, lv_scr_act(), LV_ALIGN_CENTER, 0, DURATION_Y_OFFSET);

  // Small duration units
  units = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(units, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  //lv_label_set_recolor(units, true);
  lv_label_set_text_static(units, "");  // "min"
  lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);

  // Input buttons
  lv_style_init(&btn_style);
  lv_style_set_radius(&btn_style, LV_STATE_DEFAULT, buttonHeight / 4);
  lv_style_set_bg_color(&btn_style, LV_STATE_DEFAULT, lv_color_hex(0x111111));

  btnLeft = lv_btn_create(lv_scr_act(), nullptr);
  btnLeft->user_data = this;
  lv_obj_set_event_cb(btnLeft, ButtonEventHandler);
  lv_obj_add_style(btnLeft, LV_BTN_PART_MAIN, &btn_style);
  lv_obj_set_size(btnLeft, buttonWidth, buttonHeight);
  lv_obj_align(btnLeft, nullptr, LV_ALIGN_IN_BOTTOM_LEFT, buttonXOffset, 0);
  btnLeft_lbl = lv_label_create(btnLeft, nullptr);
  lv_obj_set_style_local_text_font(btnLeft_lbl, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_48);
  lv_label_set_text_static(btnLeft_lbl, "");
  lv_obj_set_hidden(btnLeft, !isManualAllowed);
  lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed);

  btnRight = lv_btn_create(lv_scr_act(), nullptr);
  btnRight->user_data = this;
  lv_obj_set_event_cb(btnRight, ButtonEventHandler);
  lv_obj_add_style(btnRight, LV_BTN_PART_MAIN, &btn_style);
  lv_obj_set_size(btnRight, buttonWidth, buttonHeight);
  lv_obj_align(btnRight, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, - buttonXOffset, 0);
  btnRight_lbl = lv_label_create(btnRight, nullptr);
  lv_obj_set_style_local_text_font(btnRight_lbl, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_48);
  lv_label_set_text_static(btnRight_lbl, "");
  lv_obj_set_hidden(btnRight, !isManualAllowed);
  lv_obj_set_hidden(btnRight_lbl, !isManualAllowed);

  btnPreferences = lv_btn_create(lv_scr_act(), nullptr);
  btnPreferences->user_data = this;
  lv_obj_set_event_cb(btnPreferences, ButtonEventHandler);
  //lv_obj_add_style(btnPreferences, LV_BTN_PART_MAIN, &btn_style);
  lv_obj_set_size(btnPreferences, buttonWidth / 2 - 10, buttonHeight / 2);
  lv_obj_align(btnPreferences, nullptr, LV_ALIGN_IN_TOP_RIGHT, - buttonXOffset, 2 * buttonXOffset);
  btnPreferences_lbl = lv_label_create(btnPreferences, nullptr);
  lv_obj_set_style_local_text_font(btnPreferences_lbl, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_48);
  lv_label_set_text_static(btnPreferences_lbl, Symbols::cuebandPreferences);
  lv_obj_set_hidden(btnPreferences, true);
  lv_obj_set_hidden(btnPreferences_lbl, true);

  taskUpdate = lv_task_create(lv_update_task, 200, LV_TASK_PRIO_MID, this);
  Update();

  // If the app was opened from the menu but is disabled, immediately close
  if (!cueController.IsOpenDetails()) {
    Close();
  }
}

CueBandApp::~CueBandApp() {
  lv_style_reset(&btn_style);
  lv_task_del(taskUpdate);
  lv_obj_clean(lv_scr_act());
}

void CueBandApp::Update() {
  uint32_t now = std::chrono::duration_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime().time_since_epoch()).count();
  if (now != lastTime || changes) {
    if (changes) timeout = 0;
    if (now != lastTime && timeout++ >= SCREEN_TIMEOUT) Close(true);
    changes = false;
    lastTime = now;

    uint32_t override_remaining = 0;
    cueController.GetStatus(nullptr, nullptr, nullptr, &override_remaining, nullptr, nullptr, nullptr);

#if defined(CUEBAND_CUSTOMIZATION_NO_INVALID_TIME) && defined(CUEBAND_DETECT_UNSET_TIME)
    if (dateTimeController.IsUnset()) {
      lv_label_set_text_fmt(label_time, "");
    } else
#endif
    lv_label_set_text(label_time, dateTimeController.FormattedTime().c_str());
    batteryIcon.SetBatteryPercentage(batteryController.PercentRemaining());
    lv_obj_set_hidden(batteryIcon.GetObject(), screen == CUEBAND_SCREEN_MANUAL);  // Preferences button partially obscures battery level

    static char text[80];
    static char durationText[16]; // "00"
    char *p = text;
    *p = '\0';

    // Text label and duration
    const char *symbol = Symbols::none;
    sprintf(durationText, "%s", "");
    switch (screen) {
      case CUEBAND_SCREEN_OVERVIEW:
      {
        p += sprintf(text, "%s", cueController.Description(true, &symbol));
        //lv_label_set_text_static(units, "#808080  Mute     Manual#");  // LV_COLOR_GRAY #808080
        lv_label_set_text_static(units, " Mute     Manual");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, 0, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_SNOOZE:
      {
        symbol = Symbols::cuebandSilence;
        if (cueController.IsSnoozed()) {
          p += sprintf(text, "Muted for:");
          sprintf(durationText, "%d", (int)((override_remaining + 30) / 60));
        } else {
          //                 "XXXXXXXXXXXXXXXXX"
          if (cueController.IsWithinScheduledPrompt()) {
            p += sprintf(text, "Not muting");
          } else {
            p += sprintf(text, "Not prompting");
          }
          sprintf(durationText, "--");
        }
        lv_label_set_text_static(units, "min");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_MANUAL:
      {
        symbol = Symbols::cuebandImpromptu;
        if (cueController.IsTemporary()) {
          p += sprintf(text, "Manual cue\nfor:");
          sprintf(durationText, "%d", (int)((override_remaining + 30) / 60));
        } else {
          //                 "XXXXXXXXXXXXXXXXX"
          p += sprintf(text, "No manual cue");
          sprintf(durationText, "--");
        }
        lv_label_set_text_static(units, "min");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_PREFERENCES:
      {
        symbol = Symbols::cuebandPreferences;
        unsigned int lastInterval = 0, promptStyle = 0;
        cueController.GetLastImpromptu(&lastInterval, &promptStyle);
        // Interval Style
        // _15 sec.
        // LV_COLOR_GRAY #808080
        p += sprintf(text, "Cue Preferences\n\n#808080 Interval Style#\n%3d %s   %s", lastInterval < 100 ? lastInterval : (lastInterval / 60), lastInterval < 100 ? "s" : "m", promptDescription[promptStyle % 16]);
        lv_label_set_text_static(units, "");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_INTERVAL:
      {
        symbol = Symbols::cuebandInterval;
        unsigned int lastInterval = 0, promptStyle = 0;
        cueController.GetLastImpromptu(&lastInterval, &promptStyle);
        p += sprintf(text, "Cue Interval:");
        if (lastInterval < 100) {
          sprintf(durationText, "%d", (int)lastInterval);
          lv_label_set_text_static(units, "sec");
        } else {
          sprintf(durationText, "%d", (int)(lastInterval / 60));
          lv_label_set_text_static(units, "min");
        }
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_STYLE:
      {
        symbol = Symbols::cuebandIntensity;
        unsigned int lastInterval = 0, promptStyle = 0;
        cueController.GetLastImpromptu(&lastInterval, &promptStyle);
        p += sprintf(text, "Cue Style:\n\n%s", promptDescription[promptStyle % 16]);
        lv_label_set_text_static(units, "");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, UNITS_X_OFFSET, UNITS_Y_OFFSET);
        break;
      }
      default:
      {
        symbol = Symbols::none;
        p += sprintf(text, "%s", "(error)");
        break;
      }
    }
    lv_label_set_text_static(lInfoIcon, symbol);
    lv_label_set_text_static(duration, durationText);
    lv_obj_align(duration, lv_scr_act(), LV_ALIGN_CENTER, 0, DURATION_Y_OFFSET);
 

    // Buttons
    bool showPreferences = false;
    switch (screen) {
      case CUEBAND_SCREEN_OVERVIEW:
      {
        lv_label_set_text_static(btnLeft_lbl, Symbols::cuebandSilence);
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, cueController.IsSnoozed() ? LV_COLOR_RED : LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed);
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandImpromptu);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, cueController.IsTemporary() ? LV_COLOR_GREEN : LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed);
        break;
      }
      case CUEBAND_SCREEN_SNOOZE:
      {
        bool valid = cueController.IsSnoozed();
        bool leftEnabled = valid;
        bool minimum = !leftEnabled || atMinDuration(snoozeDurations, override_remaining);
        bool rightEnabled = !valid || !atMaxDuration(snoozeDurations, override_remaining);
        lv_label_set_text_static(btnLeft_lbl, minimum ? Symbols::cuebandCancel : Symbols::cuebandMinus);
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed || !leftEnabled);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed || !leftEnabled);
        lv_label_set_text_static(btnRight_lbl, rightEnabled ? Symbols::cuebandPlus : "");
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed || !rightEnabled);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed || !rightEnabled);
        break;
      }
      case CUEBAND_SCREEN_MANUAL:
      {
        bool valid = cueController.IsTemporary();
        bool leftEnabled = valid;
        bool minimum = !leftEnabled || atMinDuration(impromptuDurations, override_remaining);
        bool rightEnabled = !valid || !atMaxDuration(impromptuDurations, override_remaining);
        lv_label_set_text_static(btnLeft_lbl, minimum ? Symbols::cuebandCancel : Symbols::cuebandMinus);
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed || !leftEnabled);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed || !leftEnabled);
        lv_label_set_text_static(btnRight_lbl, rightEnabled ? Symbols::cuebandPlus : "");
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed || !rightEnabled);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed || !rightEnabled);
        showPreferences = true;
        break;
      }
      case CUEBAND_SCREEN_PREFERENCES:
      {
        lv_label_set_text_static(btnLeft_lbl, Symbols::cuebandInterval);   // cuebandInterval
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed);
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandIntensity);  // cuebandIntensity
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed);
        break;
      }
      case CUEBAND_SCREEN_INTERVAL:
      {
        unsigned int interval = 0;
        cueController.GetLastImpromptu(&interval, nullptr);
        bool leftEnabled = !atMinDuration(promptIntervals, interval);
        bool rightEnabled = !atMaxDuration(promptIntervals, interval);
        lv_label_set_text_static(btnLeft_lbl, leftEnabled ? Symbols::cuebandMinus : "");
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed || !leftEnabled);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed || !leftEnabled);
        lv_label_set_text_static(btnRight_lbl, rightEnabled ? Symbols::cuebandPlus : "");
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed || !rightEnabled);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed || !rightEnabled);
        break;
      }
      case CUEBAND_SCREEN_STYLE:
      {
        unsigned int promptStyle = 0;
        cueController.GetLastImpromptu(nullptr, &promptStyle);
        bool leftEnabled = !atMinDuration(promptStyles, promptStyle);
        bool rightEnabled = !atMaxDuration(promptStyles, promptStyle, 0);
        lv_label_set_text_static(btnLeft_lbl, leftEnabled ? Symbols::cuebandPrevious : "");
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, !isManualAllowed || !leftEnabled);
        lv_obj_set_hidden(btnLeft_lbl, !isManualAllowed || !leftEnabled);
        lv_label_set_text_static(btnRight_lbl, rightEnabled ? Symbols::cuebandNext : "");
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, !isManualAllowed || !rightEnabled);
        lv_obj_set_hidden(btnRight_lbl, !isManualAllowed || !rightEnabled);
        break;
      }
      default:
      {
        lv_label_set_text_static(btnLeft_lbl, Symbols::none);
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnLeft, true);
        lv_obj_set_hidden(btnLeft_lbl, true);
        lv_label_set_text_static(btnRight_lbl, Symbols::none);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_obj_set_hidden(btnRight, true);
        lv_obj_set_hidden(btnRight_lbl, true);
        break;
      }
    }
    lv_obj_set_hidden(btnPreferences, !showPreferences);
    lv_obj_set_hidden(btnPreferences_lbl, !showPreferences);

    //lv_label_set_text_fmt(lInfo, "%s", text);
    lv_label_set_text_static(lInfo, text);
  }
}

void CueBandApp::ChangeScreen(CueBandScreen screen, bool forward) {
#ifdef CUEBAND_APP_RELOAD_SCREENS         // Reload app to change screen
  Applications::Apps nextApp = Apps::None;
  switch (screen) {
    case CUEBAND_SCREEN_OVERVIEW:
    {
      nextApp = Apps::CueBand;
      break;
    }
    case CUEBAND_SCREEN_SNOOZE:
    {
      nextApp = Apps::CueBandSnooze;
      break;
    }
    case CUEBAND_SCREEN_MANUAL:
    {
      nextApp = Apps::CueBandManual;
      break;
    }
    case CUEBAND_SCREEN_PREFERENCES:
    {
      nextApp = Apps::CueBandPreferences;
      break;
    }
    case CUEBAND_SCREEN_INTERVAL:
    {
      nextApp = Apps::CueBandInterval;
      break;
    }
    case CUEBAND_SCREEN_STYLE:
    {
      nextApp = Apps::CueBandStyle;
      break;
    }
  }
  if (nextApp != Apps::None) {
    app->StartApp(nextApp, forward ? DisplayApp::FullRefreshDirections::LeftAnim : DisplayApp::FullRefreshDirections::RightAnim);  // Right / Left / RightAnim / LeftAnim
  }
#else         // Change screen but stay in app -- no screen transition
  this->screen = screen;
#ifndef CUEBAND_APP_RELOAD_SCREENS
  #warning "If CUEBAND_APP_RELOAD_SCREENS is not defined, the current app id status will not work correctly."
  //systemTask.ReportAppActivated(this->screen);
#endif
  //app->SetFullRefresh(forward ? DisplayApp::FullRefreshDirections::LeftAnim : DisplayApp::FullRefreshDirections::RightAnim);  // Right / Left / RightAnim / LeftAnim
  //Update();
#endif
}

// Physical button pressed to go back
bool CueBandApp::OnButtonPushed() {
  switch (screen) {
    case CUEBAND_SCREEN_OVERVIEW:
    {
      return false;
    }
    case CUEBAND_SCREEN_SNOOZE:
    {
      ChangeScreen(CUEBAND_SCREEN_OVERVIEW, false);
      return true;
    }
    case CUEBAND_SCREEN_MANUAL:
    {
      ChangeScreen(CUEBAND_SCREEN_OVERVIEW, false);
      return true;
    }
    case CUEBAND_SCREEN_PREFERENCES:
    {
      ChangeScreen(CUEBAND_SCREEN_MANUAL, false);
      return true;
    }
    case CUEBAND_SCREEN_INTERVAL:
    {
      ChangeScreen(CUEBAND_SCREEN_PREFERENCES, false);
      return true;
    }
    case CUEBAND_SCREEN_STYLE:
    {
      ChangeScreen(CUEBAND_SCREEN_PREFERENCES, false);
      return true;
    }
  }
  return false;
}

void CueBandApp::Close(bool timeout) {
  this->running = false;
  if (timeout) {
    app->StartApp(Apps::Clock, DisplayApp::FullRefreshDirections::RightAnim);
  }
}

void CueBandApp::OnButtonEvent(lv_obj_t* object, lv_event_t event) {
  uint32_t override_remaining = 0;
  cueController.GetStatus(nullptr, nullptr, nullptr, &override_remaining, nullptr, nullptr, nullptr);
  changes = true;

  if (object == backgroundLabel && event == LV_EVENT_CLICKED) {
    // Clicked on background
    /*
    changes = false;
    // HACK: Hackily indirectly trigger the close because directly closing triggers the global click to relaunch -- investigate
    //Close()
    lastTime = 0xffffffff; timeout = 999;
    */
  } else if (object == btnLeft && event == LV_EVENT_CLICKED) {

    // Clicked left button
    switch (screen) {
      case CUEBAND_SCREEN_OVERVIEW:
      {
        // If manual prompting, stop
        if (cueController.IsTemporary()) {
          // Return to schedule
          cueController.SetInterval(0, 0);
        }

        uint16_t current_control_point;
        uint32_t override_remaining;
        uint32_t duration;
        cueController.GetStatus(nullptr, nullptr, &current_control_point, &override_remaining, nullptr, nullptr, &duration);
        // If scheduled prompting is active...
        if (cueController.IsAllowed() && override_remaining <= 0 && current_control_point < 0xffff) {
          // ...snooze for remaining cueing duration
          cueController.SetInterval(0, duration);
        }
        // Open snooze screen.
        ChangeScreen(CUEBAND_SCREEN_SNOOZE, true);
        break;
      }
      case CUEBAND_SCREEN_SNOOZE:
      {
        uint32_t duration = prevDuration(snoozeDurations, override_remaining);
        if (duration > 0) {
          cueController.SetInterval(0, duration);
        } else {
          cueController.SetInterval(0, 0);    // Return to schedule
        }
        break;
      }
      case CUEBAND_SCREEN_MANUAL:
      {
        uint32_t duration = prevDuration(impromptuDurations, override_remaining);
        if (duration > 0) {
          cueController.SetInterval((unsigned int)-1, duration);
        } else {
          cueController.SetInterval(0, 0);    // Return to schedule
        }
        break;
      }
      case CUEBAND_SCREEN_PREFERENCES:
      {
        ChangeScreen(CUEBAND_SCREEN_INTERVAL, true);
        break;
      }
      case CUEBAND_SCREEN_INTERVAL:
      {
        unsigned int interval = 0;
        cueController.GetLastImpromptu(&interval, nullptr);
        //if (!atMinDuration(promptIntervals, interval))
        interval = prevDuration(promptIntervals, interval);
        if (interval > 0) {
          cueController.SetInterval(interval, -1);
        }
        break;
      }
      case CUEBAND_SCREEN_STYLE:
      {
        unsigned int promptStyle = 0;
        cueController.GetLastImpromptu(nullptr, &promptStyle);
        promptStyle = prevDuration(promptStyles, promptStyle);
        if (promptStyle > 0) {
          cueController.SetPromptStyle(promptStyle);
  #ifdef CUEBAND_MOTOR_PATTERNS
          systemTask.GetMotorController().RunIndex(promptStyle);
  #endif
        }
        break;
      }
    }

  } else if (object == btnRight && event == LV_EVENT_CLICKED) {

    // Clicked right button
    switch (screen) {
      case CUEBAND_SCREEN_OVERVIEW:
      {
        // If snoozing, stop
        if (cueController.IsSnoozed()) {
          // Return to schedule
          cueController.SetInterval(0, 0);
        }

        uint16_t current_control_point;
        uint32_t override_remaining;
        uint32_t duration;
        cueController.GetStatus(nullptr, nullptr, &current_control_point, &override_remaining, nullptr, nullptr, &duration);
        // If not manual prompting or scheduled prompting...
        if (!cueController.IsTemporary() && !(cueController.IsAllowed() && (override_remaining > 0 || (current_control_point < 0xffff && duration > 0)))) {
          // ...begin manual prompting for default duration
          cueController.SetInterval((unsigned int)-1, Pinetime::Controllers::CueController::DEFAULT_DURATION);
        }
        // Open manual screen
        ChangeScreen(CUEBAND_SCREEN_MANUAL, true);
        break;
      }
      case CUEBAND_SCREEN_SNOOZE:
      {
        uint32_t duration = nextDuration(snoozeDurations, cueController.IsTemporary() ? 0 : override_remaining);
        cueController.SetInterval(0, duration);
        break;
      }
      case CUEBAND_SCREEN_MANUAL:
      {
        uint32_t duration = nextDuration(impromptuDurations, cueController.IsSnoozed() ? 0 : override_remaining);
        cueController.SetInterval((unsigned int)-1, duration);
        break;
      }
      case CUEBAND_SCREEN_PREFERENCES:
      {
        ChangeScreen(CUEBAND_SCREEN_STYLE, true);
        break;
      }
      case CUEBAND_SCREEN_INTERVAL:
      {
        unsigned int interval = 0;
        cueController.GetLastImpromptu(&interval, nullptr);
        //if (!atMaxDuration(promptIntervals, interval, 0))
        interval = nextDuration(promptIntervals, interval, 0);
        if (interval > 0) {
          cueController.SetInterval(interval, -1);
        }
        break;
      }
      case CUEBAND_SCREEN_STYLE:
      {
        unsigned int promptStyle = 0;
        cueController.GetLastImpromptu(nullptr, &promptStyle);
        promptStyle = nextDuration(promptStyles, promptStyle, 0);
        if (promptStyle > 0) {
          cueController.SetPromptStyle(promptStyle);
  #ifdef CUEBAND_MOTOR_PATTERNS
          systemTask.GetMotorController().RunIndex(promptStyle);
  #endif
        }
        break;
      }
    }

  } else if (object == btnPreferences && event == LV_EVENT_CLICKED) {
    ChangeScreen(CUEBAND_SCREEN_PREFERENCES, true);

  }
  //Update();
}


/*
bool CueBandApp::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  switch (event) {
    case Pinetime::Applications::TouchEvents::LongTap:
      //return true;
      return false;
    default:
      return false;
  }
}
*/

#endif
