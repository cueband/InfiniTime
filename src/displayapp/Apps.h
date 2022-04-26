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
      , CueBand
#ifdef CUEBAND_APP_RELOAD_SCREENS
      , CueBandSnooze
      , CueBandManual
      , CueBandPreferences
#endif      
#endif
#ifdef CUEBAND_INFO_APP_ENABLED
      , Info
#endif
    };
  }
}
