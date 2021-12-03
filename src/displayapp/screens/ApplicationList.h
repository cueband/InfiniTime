#pragma once

#include "cueband.h"

#include <memory>

#include "displayapp/screens/Screen.h"
#include "displayapp/screens/ScreenList.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "components/battery/BatteryController.h"

namespace Pinetime {
  namespace Applications {
    namespace Screens {
      class ApplicationList : public Screen {
      public:
        explicit ApplicationList(DisplayApp* app,
                                 Pinetime::Controllers::Settings& settingsController,
                                 Pinetime::Controllers::Battery& batteryController,
                                 Controllers::DateTime& dateTimeController);
        ~ApplicationList() override;
        bool OnTouchEvent(TouchEvents event) override;

      private:
        Controllers::Settings& settingsController;
        Pinetime::Controllers::Battery& batteryController;
        Controllers::DateTime& dateTimeController;

        ScreenList<
#if defined(CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS) || defined(CUEBAND_CUSTOMIZATION_NO_OTHER_APPS)
        1
#else
        2
#endif
          > screens;
        std::unique_ptr<Screen> CreateScreen1();
#if !defined(CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS) && !defined(CUEBAND_CUSTOMIZATION_NO_OTHER_APPS)
        std::unique_ptr<Screen> CreateScreen2();
        // std::unique_ptr<Screen> CreateScreen3();
#endif
      };
    }
  }
}
