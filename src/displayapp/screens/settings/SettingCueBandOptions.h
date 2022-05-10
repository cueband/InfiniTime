#include "cueband.h"

#ifdef CUEBAND_OPTIONS_APP_ENABLED

#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include "components/cue/CueController.h"
#include "displayapp/screens/Screen.h"

#define SETTINGS_CUEBAND_NUM_OPTIONS 2

namespace Pinetime {

  namespace Applications {
    namespace Screens {

      class SettingCueBandOptions : public Screen {
      public:
        SettingCueBandOptions(DisplayApp* app, Controllers::CueController& cueController);
        ~SettingCueBandOptions() override;

        void UpdateSelected(lv_obj_t* object, lv_event_t event);
        void OnButtonEvent(lv_obj_t* obj, lv_event_t event);
        
      private:

        void Refresh();
        bool OnTouchEvent(TouchEvents event) override;

        Controllers::CueController& cueController;
        lv_obj_t* cbOption[SETTINGS_CUEBAND_NUM_OPTIONS];
        
        lv_obj_t* lblNone;

        lv_obj_t* btnReset;
        lv_obj_t* txtReset;

        // When Refresh is called, it uses lv_checkbox_set_checked, which can cause extra events to be fired, which might trigger UpdateSelected which will call Refresh, causing a loop.
        // This variable is used as a mutex to prevent that.
        bool ignoringEvents;

        bool showResetButton = false;
      };
    }
  }
}

#endif
