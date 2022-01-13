#pragma once

#include "cueband.h"

#include <cstdint>
#include <lvgl/lvgl.h>
#include "systemtask/SystemTask.h"
#include "Screen.h"
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
            Controllers::Settings &settingsController
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
          );

          ~CueBandApp() override;

          void Refresh() override;
          void Update();

          bool OnTouchEvent(TouchEvents event) override;

        private:
          Pinetime::System::SystemTask& systemTask;
          Controllers::Settings& settingsController;
#ifdef CUEBAND_CUE_ENABLED
          Controllers::CueController& cueController;
#endif

          lv_task_t* taskUpdate;
          lv_obj_t *lInfo;

          int screen = 0;

      };
    }
  }
}
