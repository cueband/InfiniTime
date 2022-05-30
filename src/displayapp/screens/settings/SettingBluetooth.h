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

      class SettingBluetooth : public Screen {
      public:
        SettingBluetooth(DisplayApp* app, Pinetime::Controllers::Settings& settingsController);
        ~SettingBluetooth() override;

        void OnBluetoothEnabled(lv_obj_t* object, lv_event_t event);
        void OnBluetoothDisabled(lv_obj_t* object, lv_event_t event);

      private:
        Controllers::Settings& settingsController;
        lv_obj_t* cbEnabled;
        lv_obj_t* cbDisabled;
#ifdef CUEBAND_BLUETOOTH_DISABLE_WARNING
        lv_obj_t* lblWarning;
#endif
        bool priorMode;
      };
    }
  }
}
