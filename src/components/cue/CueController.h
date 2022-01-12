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
      const static unsigned int DEFAULT_PROMPT_STYLE = 3; // msec

      void SetInterval(unsigned int interval, unsigned int maximumRuntime);
      void SetPromptStyle(unsigned int promptStyle = DEFAULT_PROMPT_STYLE) {
        this->promptStyle = promptStyle;
      }
      int ReadCues(uint32_t *version);
      int WriteCues();

      void GetStatus(uint32_t *active_schedule_id, uint16_t *max_control_points, uint16_t *current_control_point, uint16_t *override_remaining, uint16_t *intensity, uint16_t *interval, uint16_t *duration);
      uint32_t GetOptions();

      bool IsTemporary() { return currentUptime < overrideEndTime && interval > 0; }
      bool IsSnoozed() { return currentUptime < overrideEndTime && interval == 0; }
      bool IsScheduled() { return currentUptime >= overrideEndTime; }

    private:

      Pinetime::Controllers::ControlPointStore store;
      unsigned short version;
      Pinetime::Controllers::control_point_packed_t controlPoints[PROMPT_MAX_CONTROLS];
      Pinetime::Controllers::control_point_packed_t scratch[PROMPT_MAX_CONTROLS];

      Controllers::Settings& settingsController;
      Controllers::FS& fs;
      Controllers::ActivityController& activityController;
      Controllers::MotorController& motorController;

      const static uint32_t UPTIME_NONE = (uint32_t)-1;

      uint32_t currentTime = 0;
      uint32_t currentUptime = 0;
      uint32_t lastPrompt = UPTIME_NONE;    // Uptime at the last prompt

      uint32_t overrideEndTime = 0;         // End of override time
      unsigned int interval = INTERVAL_OFF; // override prompt interval (seconds), 0=snooze sheduled prompts

      unsigned int promptStyle = 0;

      int readError = -1;                 // (Debug) File read status
      int writeError = -1;                // (Debug) File write status
    };
  }
}

#endif
