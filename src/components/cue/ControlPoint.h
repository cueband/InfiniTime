#pragma once

//#include "cueband.h"

#include <cstdlib>
#include <cstdint>

// Tightly packed control point representation
//   EDDDDDDD IIIIIIII IIVVVTTT TTTTTTTT
//   E=<1b>  enabled (1=cue enabled, 0=cue disabled)
//   D=<7b>  weekdays bitmap the cue applies (b0-b6=Sun-Sat, 0b0000000=disabled)
//   I=<10b> interval (0-1023 seconds, up to 17m03s)
//   V=<3b>  volume / pattern setting (0-7, 0=silent)
//   T=<11b> time of day the cue is valid (minute, 0-1439) (PREVIOUSLY: units of 5 minutes/300 seconds, 0-287; 0x1ff=disabled)
typedef uint32_t cue_control_point_t;

#define CONTROL_POINT_NONE ((unsigned int)-1)   // 0xffffffff

namespace Pinetime {
  namespace Controllers {
    class ControlPoint {
      public:
        static const unsigned int numDays = 7;
        static const unsigned int timeUnitSize = 60;         // 1 minute time units
        static const unsigned int intervalUnitSize = 1;      // 1 second time units
        static const unsigned int timePerDay = 24 * 60 * 60; // 86400 seconds

        // Cue enabled
        static bool IsEnabled(cue_control_point_t controlPoint)
        {
            return (controlPoint & (1 << 31)) != 0;
        }

        // Weekday bitmap (b0=Sunday, ..., b6=Saturday)
        static unsigned int GetWeekdays(cue_control_point_t controlPoint)
        {
            unsigned int weekdayBitmap = (controlPoint >> 24) & ((1 << 7) - 1);
            return weekdayBitmap;
        }

        // Prompt interval in seconds
        static unsigned int GetInterval(cue_control_point_t controlPoint)
        {
            const int intervalUnitSize = 1;    // 1 second time units
            unsigned int unitInterval = (controlPoint >> 14) & ((1 << 10) - 1);
            return unitInterval * intervalUnitSize;
        }

        // Prompt volume/pattern (0-7)
        static unsigned int GetVolume(cue_control_point_t controlPoint)
        {
            unsigned int volume = (controlPoint >> 11) & ((1 << 3) - 1);
            return volume;
        }

        // Control point time of day in seconds (0-86399)
        static unsigned int GetTimeOfDay(cue_control_point_t controlPoint)
        {
            const unsigned int maxUnit = ((timePerDay - 1) / timeUnitSize);
            unsigned int unitOfDay = (controlPoint >> 0) & ((1 << 11) - 1);
            if (unitOfDay > maxUnit) return CONTROL_POINT_NONE;
            return unitOfDay * timeUnitSize;
        }

        // Calculate the smallest interval a given control point is before-or-at the given day/time. (CONTROL_POINT_NONE if none)
        static unsigned int CueTimeBefore(unsigned int weekdays, unsigned int cueTime, unsigned int day, unsigned int targetTime)
        {
            // Check for disabled cues
            if (weekdays == 0 || cueTime >= timePerDay)
            {
                return CONTROL_POINT_NONE;
            }

            // Same day and before or at (<=)
            if (weekdays & (1 << day) && cueTime <= targetTime)
            {
                return targetTime - cueTime;
            }

            // Find closest day before this
            for (unsigned int i = 1; i <= numDays; i++)
            {
                int testDay = (weekdays + numDays - i) % numDays;
                if (weekdays & (1 << testDay))
                {
                    // Remainder of cue day, plus number of whole days, plus time of current day
                    return (timePerDay - cueTime) + ((i - 1) * timePerDay) + targetTime;
                }
            }
            // No days found
            return CONTROL_POINT_NONE;
        }

        // Calculate the smallest interval a given control point is strictly after the given day/time. (CONTROL_POINT_NONE if none)
        static unsigned int CueTimeAfter(unsigned int weekdays, unsigned int cueTime, unsigned int day, unsigned int targetTime)
        {
            // Check for disabled cues
            if (weekdays == 0 || cueTime >= timePerDay)
            {
                return CONTROL_POINT_NONE;
            }

            // Same day and strictly after (>)
            if (weekdays & (1 << day) && cueTime > targetTime)
            {
                return cueTime - targetTime;
            }

            // Find closest day after this
            for (unsigned int i = 1; i <= numDays; i++)
            {
                int testDay = (day + i) % numDays;
                if (weekdays & (1 << testDay))
                {
                    // Remainder of current day, plus number of whole days, plus minutes of cue day
                    return (timePerDay - targetTime) + ((i - 1) * timePerDay) + cueTime;
                }
            }
            // No days found
            return CONTROL_POINT_NONE;
        }


        // Find the nearest control points to the given day and time-of-day, both 'before-or-at' and 'after'. false if no valid points.
        // outIndex - current control point index (<0 if none)
        // outElapsed - duration control point has already been active
        // outNextIndex - next control point index (<0 if none)
        // outRemaining - remaining time until the next control point is active
        static bool CueNearest(cue_control_point_t *controlPoints, size_t numControlPoints, unsigned int day, unsigned int time, int *outIndex, unsigned int *outElapsed, int *outNextIndex, unsigned int *outRemaining)
        {
            int closestIndexBefore = -1;
            unsigned int closestDistanceBefore = CONTROL_POINT_NONE;
            int closestIndexAfter = -1;
            unsigned int closestDistanceAfter = CONTROL_POINT_NONE;
            for (int i = 0; (size_t)i < numControlPoints; i++)
            {
                unsigned int weekdays = ControlPoint::GetWeekdays(controlPoints[i]);
                unsigned int cueTime = ControlPoint::GetTimeOfDay(controlPoints[i]);
                unsigned int distance;
                
                distance = CueTimeBefore(weekdays, cueTime, day, time);
                if (distance != CONTROL_POINT_NONE && (distance < closestDistanceBefore || closestIndexBefore < 0))
                {
                    closestIndexBefore = i;
                    closestDistanceBefore = distance;
                }

                distance = CueTimeAfter(weekdays, cueTime, day, time);
                if (distance != CONTROL_POINT_NONE && (distance < closestDistanceAfter || closestIndexAfter < 0))
                {
                    closestIndexAfter = i;
                    closestDistanceAfter = distance;
                }
            }
            if (outIndex) *outIndex = closestIndexBefore;
            if (outElapsed) *outElapsed = closestDistanceBefore;
            if (outNextIndex) *outNextIndex = closestIndexAfter;
            if (outRemaining) *outRemaining = closestDistanceAfter;
            return closestIndexBefore > 0 && closestIndexAfter > 0;
        }



      private:
        ControlPoint() {}   // no instances
    };
  }
}
