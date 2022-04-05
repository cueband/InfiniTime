#include "cueband.h"

#pragma once

#include <cstdint>

namespace Pinetime {
  namespace Controllers {

    class MotorController {
    public:
      MotorController() = default;

      void Init();
      void RunForDuration(uint8_t motorDuration);
      void StartRinging();
      void StopRinging();

#ifdef CUEBAND_MOTOR_PATTERNS
      void RunIndex(uint32_t index);
      void BeginPattern(const int *pattern);
#endif

    private:
      static void Ring(void* p_context);
      static void StopMotor(void* p_context);

#ifdef CUEBAND_MOTOR_PATTERNS
      void AdvancePattern();
      const int *currentPattern = nullptr;
      int currentIndex = 0;
#endif

    };
  }
}
