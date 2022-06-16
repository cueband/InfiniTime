#pragma once

#include "cueband.h"

#include <cstdint>
#include <lvgl/lvgl.h>
#include "systemtask/SystemTask.h"
#include "Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/battery/BatteryController.h"
#include <displayapp/screens/BatteryIcon.h>
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

namespace Pinetime {

  namespace Controllers {
    class Settings;
  }

  namespace Applications {
    namespace Screens {

      enum CueBandScreen {
        CUEBAND_SCREEN_OVERVIEW,    // 0
        CUEBAND_SCREEN_SNOOZE,      // 1
        CUEBAND_SCREEN_MANUAL,      // 2
        CUEBAND_SCREEN_PREFERENCES, // 3
        CUEBAND_SCREEN_INTERVAL,    // 4
        CUEBAND_SCREEN_STYLE,       // 5
      };

      class CueBandApp : public Screen {
        public:
          CueBandApp(
            CueBandScreen screen,
            DisplayApp* app, 
            System::SystemTask& systemTask, 
            Pinetime::Controllers::Battery& batteryController,
            Controllers::DateTime& dateTimeController,
            Controllers::Settings &settingsController
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
          );

          ~CueBandApp() override;

          void Update();
          void Close(bool timeout = false);

          void OnButtonEvent(lv_obj_t* object, lv_event_t event);
          bool OnButtonPushed() override;
          //bool OnTouchEvent(TouchEvents event) override;

        private:
          CueBandScreen screen = CUEBAND_SCREEN_OVERVIEW;
          void ChangeScreen(CueBandScreen screen, bool forward);

          bool isManualAllowed = false;

          Pinetime::System::SystemTask& systemTask;
          Pinetime::Controllers::Battery& batteryController;
          Controllers::DateTime& dateTimeController;
          Controllers::Settings& settingsController;
#ifdef CUEBAND_CUE_ENABLED
          Controllers::CueController& cueController;
#endif

          lv_task_t* taskUpdate;
          lv_obj_t* lInfoIcon;
          lv_obj_t* lInfo;

          BatteryIcon batteryIcon;
          lv_obj_t* label_time;

          lv_obj_t *duration;
          lv_obj_t *units;

          lv_style_t btn_style;

          lv_obj_t* btnLeft;
          lv_obj_t* btnLeft_lbl;
          lv_obj_t* btnRight;
          lv_obj_t* btnRight_lbl;
          lv_obj_t* btnPreferences;
          lv_obj_t* btnPreferences_lbl;

          lv_obj_t* backgroundLabel;

          // Updates
          bool changes = true;
          uint32_t lastTime = 0;
          unsigned int timeout = 0;
      };
    }
  }
}
