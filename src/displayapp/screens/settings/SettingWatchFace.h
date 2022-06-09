#pragma once

#include "cueband.h"

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>

#include "components/settings/Settings.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime {

  namespace Applications {
    namespace Screens {

      class SettingWatchFace : public Screen {
      public:
        SettingWatchFace(DisplayApp* app, Pinetime::Controllers::Settings& settingsController);
        ~SettingWatchFace() override;

        void UpdateSelected(lv_obj_t* object, lv_event_t event);

      private:
#ifdef CUEBAND_WATCHFACE_LIMIT_OPTIONS
        static constexpr std::array<const char*, 2> options = {" Digital face", " Analog face"};
#else
        static constexpr std::array<const char*, 4> options = {" Digital face", " Analog face", " PineTimeStyle", " Terminal"};
#endif
        Controllers::Settings& settingsController;

        lv_obj_t* cbOption[options.size()];
      };
    }
  }
}
