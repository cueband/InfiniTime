#pragma once

#include "cueband.h"

#include <cstdint>
#include <lvgl/lvgl.h>
#include "systemtask/SystemTask.h"
#include "Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/motion/MotionController.h"
#ifdef CUEBAND_ACTIVITY_ENABLED
#include "components/activity/ActivityController.h"
#endif
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

namespace Pinetime {

  namespace Controllers {
    class Settings;
  }

  namespace Applications {
    namespace Screens {

      class InfoApp : public Screen {
        public:
          InfoApp(
            DisplayApp* app, 
            System::SystemTask& systemTask, 
            Controllers::DateTime& dateTimeController,
            Controllers::MotionController& motionController, 
            Controllers::Settings &settingsController
#ifdef CUEBAND_ACTIVITY_ENABLED
            , Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
          );

          ~InfoApp() override;

          void Refresh() override;
          void Update();

          bool OnTouchEvent(TouchEvents event) override;

          int ScreenCount();

        private:
          Pinetime::System::SystemTask& systemTask;
          Pinetime::Controllers::DateTime& dateTimeController;
          Controllers::MotionController& motionController;
          Controllers::Settings& settingsController;
#ifdef CUEBAND_ACTIVITY_ENABLED
          Controllers::ActivityController& activityController;
#endif
#ifdef CUEBAND_CUE_ENABLED
          Controllers::CueController& cueController;
#endif
#ifdef CUEBAND_INFO_APP_ID
          #define QR_IMAGE_PALETTE (2*4)
          #define QR_IMAGE_DIMENSION (21+5+5+1) // 32
          #define QR_IMAGE_SIZE (QR_IMAGE_DIMENSION * QR_IMAGE_DIMENSION / 8 + QR_IMAGE_PALETTE)  // 128

          char longAddress[18] = {0};   // "a0:b1:c2:d3:e4:f5\0"
          char shortAddress[13] = {0};  // "A0B1C2D3E4F5\0"

          uint8_t data_qr[QR_IMAGE_SIZE] __attribute__((aligned(8)));
          lv_img_dsc_t image_qr = {0};
          lv_obj_t* qr_obj;
#endif

          lv_task_t* taskUpdate;
          lv_obj_t *lInfo;

          int screen = 0;

      };
    }
  }
}
