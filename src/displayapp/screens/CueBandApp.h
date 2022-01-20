#pragma once

#include "cueband.h"

#include <cstdint>
#include <lvgl/lvgl.h>
#include "systemtask/SystemTask.h"
#include "Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/battery/BatteryController.h"
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

namespace Pinetime {

  namespace Controllers {
    class Settings;
  }

  namespace Applications {
    namespace Screens {

      class CueBandApp : public Screen {
        public:
          CueBandApp(
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
          void Close();

          void OnButtonEvent(lv_obj_t* object, lv_event_t event);
          //bool OnTouchEvent(TouchEvents event) override;

        private:
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

          lv_obj_t* batteryIcon;
          lv_obj_t* label_time;

          lv_style_t btn_style;

          lv_obj_t* btnLeft;
          lv_obj_t* btnLeft_lbl;
          lv_obj_t* btnRight;
          lv_obj_t* btnRight_lbl;

          lv_obj_t* backgroundLabel;

          // Updates
          bool changes = true;
          uint32_t lastTime = 0;
          unsigned int timeout = 0;
      };
    }
  }
}
