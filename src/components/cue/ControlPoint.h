#pragma once

#include "cueband.h"

#include <cstdint>

// Tightly packed control point representation
//   EDDDDDDD IIIIIIII IIVVVTTT TTTTTTTT
//   E=<1b>  enabled (1=cue enabled, 0=cue disabled)
//   D=<7b>  weekdays bitmap the cue applies (b0-b6=Sun-Sat, 0b0000000=disabled)
//   I=<10b> interval (0-1023 seconds, up to 17m03s)
//   V=<3b>  volume / pattern setting (0-7, 0=silent)
//   T=<11b> time of day the cue is valid (minute, 0-1439) (PREVIOUSLY: units of 5 minutes/300 seconds, 0-287; 0x1ff=disabled)
typedef uint32_t cue_control_point_t;


namespace Pinetime {
  namespace Controllers {
    class ControlPoint {
      public:
        static const int timeUnitSize = 60;    // 1 minute time units
        static const int intervalUnitSize = 1; // 1 second time units

        // Cue enabled
        static bool isEnabled(cue_control_point_t controlPoint)
        {
            return (controlPoint & (1 << 31)) != 0;
        }

        // Weekday bitmap (b0=Sunday, ..., b6=Saturday)
        static int getWeekdays(cue_control_point_t controlPoint)
        {
            int weekdayBitmap = (controlPoint >> 24) & ((1 << 7) - 1);
            return weekdayBitmap;
        }

        // Prompt interval in seconds
        static int getInterval(cue_control_point_t controlPoint)
        {
            const int intervalUnitSize = 1;    // 1 second time units
            int unitInterval = (controlPoint >> 14) & ((1 << 10) - 1);
            return unitInterval * intervalUnitSize;
        }

        // Prompt volume/pattern (0-7)
        static int getVolume(cue_control_point_t controlPoint)
        {
            int volume = (controlPoint >> 11) & ((1 << 3) - 1);
            return volume;
        }

        // Control point time of day in seconds (0-86399)
        static int getTimeOfDay(cue_control_point_t controlPoint)
        {
            const int maxUnit = ((24 * 60 * 60 - 1) / timeUnitSize);
            int unitOfDay = (controlPoint >> 0) & ((1 << 11) - 1);
            if (unitOfDay > maxUnit) return -1;
            return unitOfDay * timeUnitSize;
        }

      private:
        ControlPoint() {}   // no instances
    };
  }
}
