#include "displayapp/screens/settings/Settings.h"
#include <lvgl/lvgl.h>
#include <array>
#include "displayapp/screens/List.h"
#include "displayapp/Apps.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/screens/Symbols.h"

using namespace Pinetime::Applications::Screens;

Settings::Settings(Pinetime::Applications::DisplayApp* app, Pinetime::Controllers::Settings& settingsController)
  : Screen(app),
    settingsController {settingsController},
    screens {app,
             settingsController.GetSettingsMenu(),
             {[this]() -> std::unique_ptr<Screen> {
                return CreateScreen1();
              },
              [this]() -> std::unique_ptr<Screen> {
                return CreateScreen2();
              },
              [this]() -> std::unique_ptr<Screen> {
                return CreateScreen3();
              },
              [this]() -> std::unique_ptr<Screen> {
               return CreateScreen4();
              },
             },
             Screens::ScreenListModes::UpDown} {
}

Settings::~Settings() {
  lv_obj_clean(lv_scr_act());
}

bool Settings::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  return screens.OnTouchEvent(event);
}

std::unique_ptr<Screen> Settings::CreateScreen1() {
  std::array<Screens::List::Applications, 4> applications {{
    {Symbols::sun, "Display", Apps::SettingDisplay},
    {Symbols::eye, "Wake Up", Apps::SettingWakeUp},
    {Symbols::clock, "Time format", Apps::SettingTimeFormat},
    {Symbols::home, "Watch face", Apps::SettingWatchFace},
  }};

  return std::make_unique<Screens::List>(0, 4, app, settingsController, applications);
}

std::unique_ptr<Screen> Settings::CreateScreen2() {
  std::array<Screens::List::Applications, 4> applications {{
#if !defined(CUEBAND_CUSTOMIZATION_NO_STEPS)
    {Symbols::shoe, "Steps", Apps::SettingSteps},
#else
    {Symbols::none, "None", Apps::None},
#endif
    {Symbols::clock, "Set date", Apps::SettingSetDate},
    {Symbols::clock, "Set time", Apps::SettingSetTime},
    {Symbols::batteryHalf, "Battery", Apps::BatteryInfo}}};

  return std::make_unique<Screens::List>(1, 4, app, settingsController, applications);
}

std::unique_ptr<Screen> Settings::CreateScreen3() {

  std::array<Screens::List::Applications, 4> applications {{
    {Symbols::clock, "Chimes", Apps::SettingChimes},
    {Symbols::tachometer, "Shake Calib.", Apps::SettingShakeThreshold},
    {Symbols::check, "Firmware", Apps::FirmwareValidation},
    {Symbols::bluetooth, "Bluetooth", Apps::SettingBluetooth}
  }};

  return std::make_unique<Screens::List>(2, 4, app, settingsController, applications);
}

std::unique_ptr<Screen> Settings::CreateScreen4() {

  std::array<Screens::List::Applications, 4> applications {{
    {Symbols::list, "About", Apps::SysInfo},
#if defined(CUEBAND_INFO_APP_ENABLED)
    {CUEBAND_INFO_APP_SYMBOL, "Info", Apps::Info},
#else
    {Symbols::none, "None", Apps::None},
#endif
#ifdef CUEBAND_APP_ENABLED
    {CUEBAND_APP_SYMBOL, "Cues", Apps::CueBand},
#else
    {Symbols::none, "None", Apps::None},
#endif
    {Symbols::none, "None", Apps::None}
  }};

  return std::make_unique<Screens::List>(3, 4, app, settingsController, applications);
}
