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
      SettingAirplaneMode,
      Error
#ifdef CUEBAND_APP_ENABLED
      , CueBand
#endif
#ifdef CUEBAND_INFO_APP_ENABLED
      , Info
#endif
    };
  }
}
