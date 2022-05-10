#include "cueband.h"

#ifdef CUEBAND_OPTIONS_APP_ENABLED

#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include "components/settings/Settings.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime {

  namespace Applications {
    namespace Screens {

      class SettingCueBandOptions : public Screen {
      public:
        SettingCueBandOptions(DisplayApp* app, Pinetime::Controllers::Settings& settingsController);
        ~SettingCueBandOptions() override;

        void UpdateSelected(lv_obj_t* object, lv_event_t event);

      private:
        Controllers::Settings& settingsController;
        uint8_t optionsTotal;
        lv_obj_t* cbOption[5];
        // When UpdateSelected is called, it uses lv_checkbox_set_checked,
        // which can cause extra events to be fired,
        // which might trigger UpdateSelected again, causing a loop.
        // This variable is used as a mutex to prevent that.
        bool ignoringEvents;
      };
    }
  }
}

#endif
