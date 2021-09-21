#include "ApplicationList.h"
#include <lvgl/lvgl.h>
#include <array>
#include "Symbols.h"
#include "Tile.h"
#include "displayapp/Apps.h"
#include "../DisplayApp.h"

using namespace Pinetime::Applications::Screens;

ApplicationList::ApplicationList(Pinetime::Applications::DisplayApp* app,
                                 Pinetime::Controllers::Settings& settingsController,
                                 Pinetime::Controllers::Battery& batteryController,
                                 Controllers::DateTime& dateTimeController)
  : Screen(app),
    settingsController {settingsController},
    batteryController {batteryController},
    dateTimeController {dateTimeController},
    screens {app,
             settingsController.GetAppMenu(),
             {
               [this]() -> std::unique_ptr<Screen> {
                 return CreateScreen1();
               },
#ifndef CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS
               [this]() -> std::unique_ptr<Screen> {
                 return CreateScreen2();
               },
               //[this]() -> std::unique_ptr<Screen> { return CreateScreen3(); }
#endif
             },
             Screens::ScreenListModes::UpDown} {
}

ApplicationList::~ApplicationList() {
  lv_obj_clean(lv_scr_act());
}

bool ApplicationList::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  return screens.OnTouchEvent(event);
}

std::unique_ptr<Screen> ApplicationList::CreateScreen1() {
  std::array<Screens::Tile::Applications, 6> applications {{
#ifdef CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS
    {Symbols::stopWatch, Apps::StopWatch},
    {Symbols::hourGlass, Apps::Timer},
    {Symbols::clock, Apps::Alarm},
#ifdef CUEBAND_APP_ENABLED
    {CUEBAND_APP_SYMBOL, Apps::CueBand},
#else
    {"", Apps::None},
#endif
    {"", Apps::None},
    {"", Apps::None},
#else
    {Symbols::stopWatch, Apps::StopWatch},
    {Symbols::music, Apps::Music},
    {Symbols::map, Apps::Navigation},
    {Symbols::shoe, Apps::Steps},
    {Symbols::heartBeat, Apps::HeartRate},
    {Symbols::hourGlass, Apps::Timer},
#endif
  }};

  return std::make_unique<Screens::Tile>(0, 
#ifdef CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS
    1,  // Scrollbar suitable for only one screen of apps
#else
    2, 
#endif
    app, settingsController, batteryController, dateTimeController, applications);
}

#ifndef CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS
std::unique_ptr<Screen> ApplicationList::CreateScreen2() {
  std::array<Screens::Tile::Applications, 6> applications {{
#ifdef CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS
    {"", Apps::None},
    {"", Apps::None},
    {"", Apps::None},
    {"", Apps::None},
    {"", Apps::None},
    {"", Apps::None},
#else
    {Symbols::paintbrush, Apps::Paint},
    {Symbols::paddle, Apps::Paddle},
#ifdef CUEBAND_APP_ENABLED
    {CUEBAND_APP_SYMBOL, Apps::CueBand},    //{"", Apps::None},
#else
    {"2", Apps::Twos},
#endif
    {Symbols::chartLine, Apps::Motion},
    {Symbols::drum, Apps::Metronome},
    {Symbols::clock, Apps::Alarm},
#endif
  }};

  return std::make_unique<Screens::Tile>(1, 2, app, settingsController, batteryController, dateTimeController, applications);
}
#endif

/*std::unique_ptr<Screen> ApplicationList::CreateScreen3() {
  std::array<Screens::Tile::Applications, 6> applications {
          {{"A", Apps::Meter},
           {"B", Apps::Navigation},
           {"C", Apps::Clock},
           {"D", Apps::Music},
           {"E", Apps::SysInfo},
           {"F", Apps::Brightness}
          }
  };

  return std::make_unique<Screens::Tile>(2, 3, app, settingsController, batteryController, dateTimeController, applications);
}*/
