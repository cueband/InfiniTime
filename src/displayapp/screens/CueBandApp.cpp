#include "cueband.h"

#ifdef CUEBAND_APP_ENABLED

#include "CueBandApp.h"
#include <lvgl/lvgl.h>
#include "displayapp/DisplayApp.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/BatteryIcon.h"

using namespace Pinetime::Applications::Screens;

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

  // Padding etc.
  static constexpr uint8_t innerDistance = 10;
  static constexpr uint8_t barHeight = 20 + innerDistance;
  static constexpr uint8_t buttonHeight = (LV_VER_RES_MAX - barHeight - innerDistance) / 2;
  static constexpr uint8_t buttonWidth = (LV_HOR_RES_MAX - innerDistance) / 2; // wide buttons
  //static constexpr uint8_t buttonWidth = buttonHeight; // square buttons
  static constexpr uint8_t buttonXOffset = (LV_HOR_RES_MAX - buttonWidth * 2 - innerDistance) / 2;

  // Time
  label_time = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_fmt(label_time, "");
  lv_label_set_align(label_time, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(label_time, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, 0);

  // Battery
  batteryIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text(batteryIcon, "");
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

  lv_obj_t* backgroundLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_long_mode(backgroundLabel, LV_LABEL_LONG_CROP);
  lv_obj_set_size(backgroundLabel, 240, 240);
  lv_obj_set_pos(backgroundLabel, 0, 0);
  lv_label_set_text_static(backgroundLabel, "");

  taskUpdate = lv_task_create(lv_update_task, 1000, LV_TASK_PRIO_LOW, this);
  Update();
}

CueBandApp::~CueBandApp() {
  lv_style_reset(&btn_style);
  lv_task_del(taskUpdate);
  lv_obj_clean(lv_scr_act());
}

void CueBandApp::Update() {
  lv_label_set_text_fmt(label_time, "%02i:%02i", dateTimeController.Hours(), dateTimeController.Minutes());
  lv_label_set_text(batteryIcon, BatteryIcon::GetBatteryIcon(batteryController.PercentRemaining()));

  static char text[80];
  char *p = text;
  *p = '\0';

  const char *symbol = "";
  p += sprintf(text, "%s", cueController.Description(true, &symbol));

  // Left button: Snooze prompts / return to scheduled
  if (cueController.IsTemporary()) {
    lv_label_set_text_static(btnLeft_lbl, Symbols::cuebandScheduled);
  } else {
    lv_label_set_text_static(btnLeft_lbl, Symbols::cuebandSilence);
  }
  //lv_obj_set_style_local_bg_color(btnLeft, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GREEN);

  // Right button: Impromptu prompts / return to scheduled
  if (cueController.IsSnoozed()) {
    lv_label_set_text_static(btnRight_lbl, Symbols::cuebandScheduled);
  } else {
    lv_label_set_text_static(btnRight_lbl, Symbols::cuebandImpromptu);
  }
  //lv_obj_set_style_local_bg_color(btnRight, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GREEN);

  lv_label_set_text_static(lInfoIcon, symbol);

  //lv_label_set_text_fmt(lInfo, "%s", text);
  lv_label_set_text_static(lInfo, text);
}


void CueBandApp::OnButtonEvent(lv_obj_t* object, lv_event_t event) {
  if (object == btnLeft && event == LV_EVENT_CLICKED) {
    // Left button: Snooze prompts / return to scheduled
    if (cueController.IsTemporary()) {    // Temporary cueing: return to schedule
      cueController.SetInterval(0, 0);    // Return to schedule
    } else {                              // Next snooze step cycle
// TODO: Next snooze step cycle
cueController.SetInterval(10, 60);
    }

  } else if (object == btnRight && event == LV_EVENT_CLICKED) {
    // Right button: Impromptu prompts / return to scheduled
    if (cueController.IsSnoozed()) {      // Snoozed cueing: return to schedule
      cueController.SetInterval(0, 0);    // Return to schedule
    } else {                              // Next impromptu step cycle
// TODO: Next impromptu step cycle
cueController.SetInterval(0, 60);
    }

  }
  Update();
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
