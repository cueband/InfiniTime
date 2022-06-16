#pragma once

#include "cueband.h"

#include <array>
#include <memory>

#include "displayapp/screens/Screen.h"
#include "displayapp/screens/ScreenList.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "components/battery/BatteryController.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/Tile.h"

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
        auto CreateScreenList() const;
        std::unique_ptr<Screen> CreateScreen(unsigned int screenNum) const;

        Controllers::Settings& settingsController;
        Pinetime::Controllers::Battery& batteryController;
        Controllers::DateTime& dateTimeController;

        static constexpr int appsPerScreen = 6;

#if defined(CUEBAND_CUSTOMIZATION_NO_OTHER_APPS) || defined(CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS)
        static constexpr int nScreens = 1;
#else   // Upstream InfiniTime
        // Increment this when more space is needed
        static constexpr int nScreens = 2;
#endif

        static constexpr std::array<Tile::Applications, appsPerScreen * nScreens> applications {{
#if defined(CUEBAND_CUSTOMIZATION_NO_OTHER_APPS) || defined(CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS)    // Customized

        #if defined(CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS)
          {Symbols::stopWatch, Apps::StopWatch},
          {Symbols::clock, Apps::Alarm},
          {Symbols::hourGlass, Apps::Timer},
        #else // defined(CUEBAND_CUSTOMIZATION_NO_OTHER_APPS)
          {"", Apps::None},
          {"", Apps::None},
          {"", Apps::None},
        #endif
        #ifdef CUEBAND_INFO_APP_ENABLED
          {CUEBAND_INFO_APP_SYMBOL, Apps::InfoFromLauncher},
        #else
          {"", Apps::None},
        #endif
        #ifdef CUEBAND_METRONOME_ENABLED
          {Symbols::drum, Apps::Metronome},
        #else
          {"", Apps::None},
        #endif
        #ifdef CUEBAND_APP_ENABLED
          {CUEBAND_APP_SYMBOL, Apps::CueBand},
        #else
          {"", Apps::None},
        #endif

#else                                                       // Upstream InfiniTime, with some apps replaced
          {Symbols::stopWatch, Apps::StopWatch},
          {Symbols::clock, Apps::Alarm},
          {Symbols::hourGlass, Apps::Timer},
          {Symbols::shoe, Apps::Steps},
          {Symbols::heartBeat, Apps::HeartRate},
          {Symbols::music, Apps::Music},

          {Symbols::paintbrush, Apps::Paint},
          {Symbols::paddle, Apps::Paddle},
          {"2", Apps::Twos},
          {Symbols::chartLine, Apps::Motion},
          {Symbols::drum, Apps::Metronome},
          {Symbols::map, Apps::Navigation},
#endif
        }};
        ScreenList<nScreens> screens;
      };
    }
  }
}
