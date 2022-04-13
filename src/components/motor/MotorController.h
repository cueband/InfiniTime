#include "cueband.h"

#pragma once

#ifdef CUEBAND_TRACK_MOTOR_TIMES
#include "components/datetime/DateTimeController.h"
//namespace Pinetime::Controllers { class DateTime; }
#endif

#include <cstdint>

namespace Pinetime {
  namespace Controllers {

    class MotorController {
    public:
#ifdef CUEBAND_TRACK_MOTOR_TIMES
      MotorController(Pinetime::Controllers::DateTime& dateTimeController) : dateTimeController { dateTimeController } {};
#else
      MotorController() = default;
#endif

      void Init();
      void RunForDuration(uint8_t motorDuration);
      void StartRinging();
      void StopRinging();

#ifdef CUEBAND_MOTOR_PATTERNS
      void RunIndex(uint32_t index);
      void BeginPattern(const int *pattern);
#endif
#ifdef CUEBAND_TRACK_MOTOR_TIMES
      uptime1024_t GetLastMovement() { return lastMovement; }
#endif

    private:
      static void Ring(void* p_context);
      static void StopMotor(void* p_context);

#ifdef CUEBAND_TRACK_MOTOR_TIMES
      void TrackActive(unsigned int timeMs);
      Pinetime::Controllers::DateTime& dateTimeController;
      uptime1024_t lastMovement = 0;
#endif
#ifdef CUEBAND_MOTOR_PATTERNS
      void AdvancePattern();
      const int *currentPattern = nullptr;
      int currentIndex = 0;
#endif

    };
  }
}
