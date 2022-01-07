#pragma once

#include "cueband.h"

#ifdef CUEBAND_CUE_ENABLED

#include "components/settings/Settings.h"
#include "components/fs/FS.h"
#include "components/activity/ActivityController.h"
#include "components/motor/MotorController.h"
#include "components/cue/ControlPointStore.h"

#include <cstdint>

#define PROMPT_MAX_CONTROLS 64

namespace Pinetime {
  namespace Controllers {
    class CueController {
    public:
      CueController(Controllers::Settings& settingsController, Controllers::FS& fs, Controllers::ActivityController& activityController, Controllers::MotorController& motorController);

      void Init();
      void TimeChanged(uint32_t timestamp, uint32_t uptime);

      const static unsigned int INTERVAL_OFF = 0;
      const static unsigned int MAXIMUM_RUNTIME_OFF = 0;
      const static unsigned int MAXIMUM_RUNTIME_INFINITE = 0xffffffff;
      const static unsigned int DEFAULT_MOTOR_PULSE_WIDTH = 50; // msec

      void SetInterval(unsigned int interval, unsigned int maximumRuntime, unsigned int motorPulseWidth = DEFAULT_MOTOR_PULSE_WIDTH);
      int ReadCues(uint32_t *version);
      int WriteCues();

    private:

      Pinetime::Controllers::ControlPointStore store;
      unsigned short version;
      Pinetime::Controllers::control_point_packed_t controlPoints[PROMPT_MAX_CONTROLS];
      Pinetime::Controllers::control_point_packed_t scratch[PROMPT_MAX_CONTROLS];

      Controllers::Settings& settingsController;
      Controllers::FS& fs;
      Controllers::ActivityController& activityController;
      Controllers::MotorController& motorController;

      // uint32_t currentTime = 0;
      // uint32_t currentUptime = 0;

      unsigned int tick = 0;              // second index, prompting "run-time"
      unsigned int promptCount = 0;       // reporting: total number of prompts given
      unsigned int mutedCount = 0;        // reporting: total number of muted prompts given

      unsigned int interval = INTERVAL_OFF;                     // interval (seconds), 0=off
      unsigned int motorPulseWidth = DEFAULT_MOTOR_PULSE_WIDTH; // (msec)
      unsigned int maximumRuntime = MAXIMUM_RUNTIME_OFF;        // (seconds) prompting run-time while enabled, set to UINT_MAX for "infinite" (136 years)

      int readError = -1;                 // (Debug) File read status
      int writeError = -1;                // (Debug) File write status
    };
  }
}

#endif
