#pragma once

#include "cueband.h"

#include <array>
#include <memory>
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/ScreenList.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/List.h"

namespace Pinetime {

  namespace Applications {
    namespace Screens {

      class Settings : public Screen {
      public:
        Settings(DisplayApp* app, Pinetime::Controllers::Settings& settingsController);
        ~Settings() override;

        bool OnTouchEvent(Pinetime::Applications::TouchEvents event) override;

      private:
        auto CreateScreenList() const;
        std::unique_ptr<Screen> CreateScreen(unsigned int screenNum) const;

        Controllers::Settings& settingsController;

        static constexpr int entriesPerScreen = 4;

        // Increment this when more space is needed
        static constexpr int nScreens = 4;

        static constexpr std::array<List::Applications, entriesPerScreen * nScreens> entries {{
          {Symbols::sun, "Display", Apps::SettingDisplay},
          {Symbols::eye, "Wake Up", Apps::SettingWakeUp},
          {Symbols::clock, "Time format", Apps::SettingTimeFormat},
          {Symbols::home, "Watch face", Apps::SettingWatchFace},

#if defined(CUEBAND_OPTIONS_APP_ENABLED)
          {CUEBAND_APP_SYMBOL, "Cues Options", Apps::SettingCueBandOptions},
#endif
#if !defined(CUEBAND_CUSTOMIZATION_NO_STEPS)
          {Symbols::shoe, "Steps", Apps::SettingSteps},
#endif
          {Symbols::clock, "Set date", Apps::SettingSetDate},
          {Symbols::clock, "Set time", Apps::SettingSetTime},
          {Symbols::batteryHalf, "Battery", Apps::BatteryInfo},

          {Symbols::clock, "Chimes", Apps::SettingChimes},
          {Symbols::tachometer, "Shake Calib.", Apps::SettingShakeThreshold},
          {Symbols::check, "Firmware", Apps::FirmwareValidation},
          {Symbols::bluetooth, "Bluetooth", Apps::SettingBluetooth},

          {Symbols::list, "About", Apps::SysInfo},
#if defined(CUEBAND_INFO_APP_ENABLED)
          {CUEBAND_INFO_APP_SYMBOL, "Info", Apps::InfoFromSettings},
#endif
          {Symbols::none, "None", Apps::None},
#if !defined(CUEBAND_OPTIONS_APP_ENABLED)
          {Symbols::none, "None", Apps::None},
#endif
#if defined(CUEBAND_CUSTOMIZATION_NO_STEPS)
          {Symbols::none, "None", Apps::None},
#endif
#if !defined(CUEBAND_INFO_APP_ENABLED)
          {Symbols::none, "None", Apps::None},
#endif
        }};
        ScreenList<nScreens> screens;
      };
    }
  }
}
