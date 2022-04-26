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

static const uint32_t snoozeDurations[] = { 10 * 60, 30 * 60, 60 * 60, 0 };
static const uint32_t impromptuDurations[] = { 10 * 60, 30 * 60, 60 * 60, 0 };
static const uint32_t promptIntervals[] = { 30, 60, 90, 120, 180, 240, 0 };
static const uint32_t promptStyles[] = { 1, 2, 3, 4, 5, 6, 7, 0 };
static const char *const promptDescription[] = {
  // Proper descriptions for each style
  "Off",     // 0
  "Short",   // 1
  "Medium",  // 2
  "Long",    // 3
  "2xShort", // 4
  "2xLong",  // 5
  "3xShort", // 6
  "3xLong",  // 7
};


static uint32_t cycleDuration(const uint32_t *durations, uint32_t current, uint32_t margin = 30) {
  for (const uint32_t *duration = durations; *duration != 0; duration++) {
    if (current + margin < *duration) return *duration;
  }
  return durations[0];
}

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

CueBandApp::CueBandApp(Pinetime::Applications::DisplayApp* app,
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

#ifdef CUEBAND_CUE_ENABLED
  systemTask.ReportAppActivated();
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
  batteryIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text(batteryIcon, BatteryIcon::GetBatteryIcon(batteryController.PercentRemaining()));
  lv_obj_align(batteryIcon, nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

  // Cueing information icon
  lInfoIcon = lv_label_create(lv_scr_act(), nullptr);
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

  btnRight = lv_btn_create(lv_scr_act(), nullptr);
  btnRight->user_data = this;
  lv_obj_set_event_cb(btnRight, ButtonEventHandler);
  lv_obj_add_style(btnRight, LV_BTN_PART_MAIN, &btn_style);
  lv_obj_set_size(btnRight, buttonWidth, buttonHeight);
  lv_obj_align(btnRight, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, - buttonXOffset, 0);
  btnRight_lbl = lv_label_create(btnRight, nullptr);
  lv_obj_set_style_local_text_font(btnRight_lbl, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_48);
  lv_label_set_text_static(btnRight_lbl, "");

  btnPreferences = lv_btn_create(lv_scr_act(), nullptr);
  btnPreferences->user_data = this;
  lv_obj_set_event_cb(btnPreferences, ButtonEventHandler);
  //lv_obj_add_style(btnPreferences, LV_BTN_PART_MAIN, &btn_style);
  lv_obj_set_size(btnPreferences, buttonWidth / 2 - 10, buttonHeight / 2);
  lv_obj_align(btnPreferences, nullptr, LV_ALIGN_IN_TOP_RIGHT, - buttonXOffset, 2 * buttonXOffset);
  btnPreferences_lbl = lv_label_create(btnPreferences, nullptr);
  lv_obj_set_style_local_text_font(btnPreferences_lbl, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_sys_48);    // different font
  lv_label_set_text_static(btnPreferences_lbl, Symbols::settings);
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
    if (now != lastTime && timeout++ > 20) Close();
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
    lv_label_set_text(batteryIcon, BatteryIcon::GetBatteryIcon(batteryController.PercentRemaining()));

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
        //lv_label_set_text_static(units, "#444444 Snooze    Manual#");
        lv_label_set_text_static(units, "Snooze    Manual");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, 0, UNITS_Y_OFFSET);
        break;
      }
      case CUEBAND_SCREEN_SNOOZE:
      {
        symbol = Symbols::cuebandSilence;
        if (cueController.IsSnoozed()) {
          p += sprintf(text, "Snoozing for:");
          sprintf(durationText, "%d", (int)((override_remaining + 30) / 60));
        } else {
          //                 "XXXXXXXXXXXXXXXXX"
          p += sprintf(text, "Not snoozing");
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
          p += sprintf(text, "Manual cue:");
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
        symbol = Symbols::settings;
        unsigned int lastInterval = 0, promptStyle = 0;
        cueController.GetLastImpromptu(&lastInterval, &promptStyle);
        // Interval Style
        // _15 sec.
        p += sprintf(text, "Cue Preferences\n\n#444444 Interval Style#\n%3d s.   %s", lastInterval, promptDescription[promptStyle % 16]);
        lv_label_set_text_static(units, "");
        lv_obj_align(units, lv_scr_act(), LV_ALIGN_CENTER, 0, UNITS_Y_OFFSET);
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
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandImpromptu);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, cueController.IsTemporary() ? LV_COLOR_GREEN : LV_COLOR_GRAY);
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
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandPlus);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, rightEnabled ? LV_COLOR_RED : LV_COLOR_GRAY);
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
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandPlus);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, rightEnabled ? LV_COLOR_GREEN : LV_COLOR_GRAY);
        showPreferences = true;
        break;
      }
      case CUEBAND_SCREEN_PREFERENCES:
      {
        lv_label_set_text_static(btnLeft_lbl, Symbols::cuebandCycle);   // cuebandInterval
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_label_set_text_static(btnRight_lbl, Symbols::cuebandCycle);  // cuebandIntensity
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        break;
      }
      default:
      {
        lv_label_set_text_static(btnLeft_lbl, Symbols::none);
        lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        lv_label_set_text_static(btnRight_lbl, Symbols::none);
        lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
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
  this->screen = screen;
  app->SetFullRefresh(forward ? DisplayApp::FullRefreshDirections::Right : DisplayApp::FullRefreshDirections::Left);  // RightAnim / LeftAnim
  //Update();
}

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
  }
  return false;
}

void CueBandApp::Close() {
  this->running = false;
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
        // Cycle interval preference
        unsigned int interval = 0;
        cueController.GetLastImpromptu(&interval, nullptr);
        interval = cycleDuration(promptIntervals, interval, 0);
        cueController.SetInterval(interval, -1);
        break;
      }
    }

  } else if (object == btnRight && event == LV_EVENT_CLICKED) {

    // Clicked right button
    switch (screen) {
      case CUEBAND_SCREEN_OVERVIEW:
      {
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
        // Increase manual impromptu cue intensity
        unsigned int promptStyle = 0;
        cueController.GetLastImpromptu(nullptr, &promptStyle);
        //promptStyle = (promptStyle + 1) % 16;
        promptStyle = cycleDuration(promptStyles, promptStyle, 0);
        cueController.SetPromptStyle(promptStyle);
  #ifdef CUEBAND_MOTOR_PATTERNS
        systemTask.GetMotorController().RunIndex(promptStyle);
  #endif
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
