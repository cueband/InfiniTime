#pragma once

#include "cueband.h"

namespace Pinetime {
  namespace Applications {
    enum class Apps {
      None,
      Launcher,
      Clock,
      SysInfo,
      FirmwareUpdate,
      FirmwareValidation,
      NotificationsPreview,
      Notifications,
      Timer,
      Alarm,
      FlashLight,
      BatteryInfo,
      Music,
      Paint,
      Paddle,
      Twos,
      HeartRate,
      Navigation,
      StopWatch,
      Metronome,
      Motion,
      Steps,
      Weather,
      PassKey,
      QuickSettings,
      Settings,
      SettingWatchFace,
      SettingTimeFormat,
      SettingDisplay,
      SettingWakeUp,
      SettingSteps,
      SettingSetDate,
      SettingSetTime,
      SettingChimes,
      SettingShakeThreshold,
      SettingBluetooth,
      Error
#ifdef CUEBAND_APP_ENABLED
      , CueBand  // ~37
#ifdef CUEBAND_APP_RELOAD_SCREENS
      , CueBandSnooze
      , CueBandManual
      , CueBandPreferences
      , CueBandInterval
      , CueBandStyle
#endif      
#endif
#ifdef CUEBAND_INFO_APP_ENABLED
      , InfoFromButton  // ~43
      , InfoFromLauncher
      , InfoFromSettings
#endif
#ifdef CUEBAND_OPTIONS_APP_ENABLED
      , SettingCueBandOptions
#endif
    };
  }
}
