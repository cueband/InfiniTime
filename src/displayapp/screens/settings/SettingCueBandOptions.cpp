#include "displayapp/screens/settings/SettingCueBandOptions.h"

#ifdef CUEBAND_OPTIONS_APP_ENABLED

#include <lvgl/lvgl.h>
#include "displayapp/DisplayApp.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/Symbols.h"
#include "components/settings/Settings.h"

using namespace Pinetime::Applications::Screens;
using namespace Pinetime::Controllers;

namespace {
  static void event_handler(lv_obj_t* obj, lv_event_t event) {
    SettingCueBandOptions* screen = static_cast<SettingCueBandOptions*>(obj->user_data);
    screen->UpdateSelected(obj, event);
  }

  static void btnEventHandler(lv_obj_t* obj, lv_event_t event) {
    SettingCueBandOptions* screen = static_cast<SettingCueBandOptions*>(obj->user_data);
    screen->OnButtonEvent(obj, event);
  }
}

static options_t optionForIndex[SETTINGS_CUEBAND_NUM_OPTIONS] = {
  CueController::OPTIONS_CUE_ENABLED, 
  CueController::OPTIONS_CUE_STATUS, 
  //CueController::OPTIONS_CUE_DETAILS, 
  //CueController::OPTIONS_CUE_MANUAL, 
};

static const char *labelForIndex[SETTINGS_CUEBAND_NUM_OPTIONS] = {
  " Cues Enabled",
  " Cue Status",
  //" Details",
  //" Manual",
};

SettingCueBandOptions::SettingCueBandOptions(Pinetime::Applications::DisplayApp* app, Pinetime::Controllers::CueController& cueController)
  : Screen(app), cueController {cueController} {
  ignoringEvents = false;
  lv_obj_t* container1 = lv_cont_create(lv_scr_act(), nullptr);

  lv_obj_set_style_local_bg_opa(container1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_pad_all(container1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
  lv_obj_set_style_local_pad_inner(container1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
  lv_obj_set_style_local_border_width(container1, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);

  lv_obj_set_pos(container1, 10, 60);
  lv_obj_set_width(container1, LV_HOR_RES - 20);
  lv_obj_set_height(container1, LV_VER_RES - 50);
  lv_cont_set_layout(container1, LV_LAYOUT_COLUMN_LEFT);

  lv_obj_t* title = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(title, "Cue Options");
  lv_label_set_align(title, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(title, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 15, 15);

  lv_obj_t* icon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
  lv_obj_set_style_local_text_font(icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &cueband_20);
  lv_label_set_text_static(icon, CUEBAND_APP_SYMBOL);

  lv_label_set_align(icon, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(icon, title, LV_ALIGN_OUT_LEFT_MID, -10, 0);

  // "None" label
  lblNone = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(lblNone, "None available");
  lv_label_set_align(lblNone, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(lblNone, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);

  // Labels
  for (int i = 0; i < SETTINGS_CUEBAND_NUM_OPTIONS; i++) {
    cbOption[i] = lv_checkbox_create(container1, nullptr);
    lv_checkbox_set_text_static(cbOption[i], labelForIndex[i]);
    cbOption[i]->user_data = this;
    lv_obj_set_event_cb(cbOption[i], event_handler);
    lv_obj_set_style_local_bg_color(cbOption[i], LV_CHECKBOX_PART_BULLET, LV_STATE_DISABLED, LV_COLOR_BLACK);
    lv_obj_set_style_local_text_color(cbOption[i], LV_CHECKBOX_PART_BG, LV_STATE_DISABLED, LV_COLOR_GRAY);
  }

  // Reset button
  btnReset = lv_btn_create(lv_scr_act(), nullptr);
  btnReset->user_data = this;
  lv_obj_set_event_cb(btnReset, btnEventHandler);
  txtReset = lv_label_create(btnReset, nullptr);
  lv_label_set_text(txtReset, "Reset!");
  lv_obj_set_height(btnReset, 40);
  lv_obj_align(btnReset, lv_scr_act(), LV_ALIGN_IN_BOTTOM_MID, 0, -10);

  // Initial UI state
  Refresh();
}

SettingCueBandOptions::~SettingCueBandOptions() {
  lv_obj_clean(lv_scr_act());
}

void SettingCueBandOptions::Refresh() {
  // Temporarily ignore any LV_EVENT_VALUE_CHANGED events resulting from lv_checkbox_set_checked()
  ignoringEvents = true;
  
  // Get option values
  options_t base;
  options_t mask;
  //options_t value;
  options_t effectiveOptions = cueController.GetOptionsMaskValue(&base, &mask, nullptr);
  bool settingsEnabled = effectiveOptions & CueController::OPTIONS_CUE_SETTING;    // Allow user to toggle any non-overridden cueing functionality in the settings menu.

  // Checked and disabled state
  int countVisible = 0;
  for (int i = 0; i < SETTINGS_CUEBAND_NUM_OPTIONS; i++) {
    // Setting the state clears the disabled style
    lv_checkbox_set_checked(cbOption[i], effectiveOptions & optionForIndex[i]);
    // ...(re)set the disabled style as required
    bool disabled = !settingsEnabled || (mask & optionForIndex[i]);
    if (disabled) {
      lv_checkbox_set_disabled(cbOption[i]);
    } else {
      countVisible++;
    }
    // Hide disabled items for now as their current style does not make them look like they are unavailable
    lv_obj_set_hidden(cbOption[i], disabled);
  }

  // Show none available option if no settings are available
  lv_obj_set_hidden(lblNone, !(countVisible == 0));

  // Long-press to enable reset button
  lv_obj_set_hidden(btnReset, !showResetButton);

  // Do not ignore any future user-triggered LV_EVENT_VALUE_CHANGED events
  ignoringEvents = false;
}

void SettingCueBandOptions::UpdateSelected(lv_obj_t* object, lv_event_t event) {
  if (event == LV_EVENT_VALUE_CHANGED && !ignoringEvents) {
    // Find the index of the checkbox that triggered the event
    int index;
    for (index = 0; index < SETTINGS_CUEBAND_NUM_OPTIONS; ++index) {
      if (cbOption[index] == object) {
        break;
      }
    }

    // Change options
    options_t toggle = (index < SETTINGS_CUEBAND_NUM_OPTIONS) ? optionForIndex[index] : 0;

    // Any valid changes?
    if (toggle != 0) {
      // Get option values
      options_t base;
      options_t mask;
      //options_t value;
      options_t effectiveOptions = cueController.GetOptionsMaskValue(&base, &mask, nullptr);
      bool settingsEnabled = effectiveOptions & CueController::OPTIONS_CUE_SETTING;    // Allow user to toggle any non-overridden cueing functionality in the settings menu.

      // If the setting can be changed
      if (settingsEnabled && !(mask & toggle)) {
        // Toggle the base setting
        options_t newBase = base ^ toggle;

        // Apply the new base setting
        cueController.SetOptionsBaseValue(newBase);

        // Update UI state
        Refresh();
      }
    }
  }
}

bool SettingCueBandOptions::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  switch (event) {
    case Pinetime::Applications::TouchEvents::LongTap:
      showResetButton = true;
      Refresh();
      return true;
    default:
      return false;
  }
}

void SettingCueBandOptions::OnButtonEvent(lv_obj_t* obj, lv_event_t event) {
  if (event == LV_EVENT_CLICKED && obj == btnReset) {
    cueController.Reset(true);
    showResetButton = false;
    //this->running = false;
    Refresh();
  }
}

#endif
