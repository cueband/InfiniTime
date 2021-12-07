#pragma once

//#include "cueband.h"

#include <cstdlib>
#include <cstdint>

namespace Pinetime {
  namespace Controllers {

    static const unsigned int TIME_NONE = ((unsigned int)-1);   // 0xffffffff
    static const unsigned int DAY_NONE = ((unsigned int)-1);    // 0xffffffff
    static const int INDEX_NONE = -1;

    struct ControlPoint {
      public:
        // Tightly packed control point representation
        //   EDDDDDDD IIIIIIII IIVVVTTT TTTTTTTT
        //   E=<1b>  enabled (1=cue enabled, 0=cue disabled)
        //   D=<7b>  weekdays bitmap the cue applies (b0-b6=Sun-Sat, 0b0000000=disabled)
        //   I=<10b> interval (0-1023 seconds, up to 17m03s)
        //   V=<3b>  volume / pattern setting (0-7, 0=silent)
        //   T=<11b> time of day the cue is valid (minute, 0-1439) (PREVIOUSLY: units of 5 minutes/300 seconds, 0-287; 0x1ff=disabled)
        uint32_t controlPoint;

        static const unsigned int numDays = 7;                  // days in a week (Sun-Sat)
        static const unsigned int timePerDay = 24 * 60 * 60;    // 86400 seconds in a day
        static const unsigned int timeUnitSize = 60;            // 1 minute time-of-day storage unit
        static const unsigned int intervalUnitSize = 1;         // 1 second interval storage unit

        // Cue enabled
        bool IsEnabled()
        {
            return (controlPoint & (1 << 31)) != 0;
        }

        // Weekday bitmap (b0=Sunday, ..., b6=Saturday)
        unsigned int GetWeekdays()
        {
            unsigned int weekdayBitmap = (controlPoint >> 24) & ((1 << 7) - 1);
            return weekdayBitmap;
        }

        // Prompt interval in seconds
        unsigned int GetInterval()
        {
            const int intervalUnitSize = 1;    // 1 second time units
            unsigned int unitInterval = (controlPoint >> 14) & ((1 << 10) - 1);
            return unitInterval * intervalUnitSize;
        }

        // Prompt volume/pattern (0-7)
        unsigned int GetVolume()
        {
            unsigned int volume = (controlPoint >> 11) & ((1 << 3) - 1);
            return volume;
        }

        // Control point time of day in seconds (0-86399)
        unsigned int GetTimeOfDay()
        {
            const unsigned int maxUnit = ((timePerDay - 1) / timeUnitSize);
            unsigned int unitOfDay = (controlPoint >> 0) & ((1 << 11) - 1);
            if (unitOfDay > maxUnit) return TIME_NONE;
            return unitOfDay * timeUnitSize;
        }

        // Calculate the smallest interval the control point is before-or-at the given day/time. (TIME_NONE if none)
        unsigned int CueTimeBefore(unsigned int day, unsigned int targetTime)
        {
            unsigned int weekdays = GetWeekdays();
            unsigned int cueTime = GetTimeOfDay();

            // Check for disabled cues
            if (weekdays == 0 || cueTime >= timePerDay)
            {
                return TIME_NONE;
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
            return TIME_NONE;
        }

        // Calculate the smallest interval the control point is strictly after the given day/time. (TIME_NONE if none)
        unsigned int CueTimeAfter(unsigned int day, unsigned int targetTime)
        {
            unsigned int weekdays = GetWeekdays();
            unsigned int cueTime = GetTimeOfDay();

            // Check for disabled cues
            if (weekdays == 0 || cueTime >= timePerDay)
            {
                return TIME_NONE;
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
            return TIME_NONE;
        }

        // Find the nearest control points to the given day and time-of-day, both 'before-or-at' and 'after'. false if no valid points.
        // controlPoints - pointer to ControlPoint elements
        // numControlPoints - number of control points
        // day - day of the week (0-7)
        // time - time of the day
        // outIndex - current control point index (<0 if none)
        // outElapsed - duration control point has already been active
        // outNextIndex - next control point index (<0 if none)
        // outRemaining - remaining time until the next control point is active
        static bool CueNearest(ControlPoint *controlPoints, size_t numControlPoints, unsigned int day, unsigned int time, int *outIndex, unsigned int *outElapsed, int *outNextIndex, unsigned int *outRemaining)
        {
            int closestIndexBefore = INDEX_NONE;
            unsigned int closestDistanceBefore = TIME_NONE;
            int closestIndexAfter = INDEX_NONE;
            unsigned int closestDistanceAfter = TIME_NONE;
            for (int i = 0; (size_t)i < numControlPoints; i++)
            {
                ControlPoint *controlPoint = &controlPoints[i];
                unsigned int distance;

                if (!controlPoint->IsEnabled())
                {
                    continue;
                }
                
                distance = controlPoint->CueTimeBefore(day, time);
                if (distance != TIME_NONE && (distance < closestDistanceBefore || closestIndexBefore != TIME_NONE))
                {
                    closestIndexBefore = i;
                    closestDistanceBefore = distance;
                }

                distance = controlPoint->CueTimeAfter(day, time);
                if (distance != TIME_NONE && (distance < closestDistanceAfter || closestIndexAfter != TIME_NONE))
                {
                    closestIndexAfter = i;
                    closestDistanceAfter = distance;
                }
            }
            if (outIndex) *outIndex = closestIndexBefore;
            if (outElapsed) *outElapsed = closestDistanceBefore;
            if (outNextIndex) *outNextIndex = closestIndexAfter;
            if (outRemaining) *outRemaining = closestDistanceAfter;
            return closestIndexBefore >= 0 && closestIndexAfter >= 0;
        }

      private:
        ControlPoint() {}   // no instances
    };

    // Confirm packing (used directly as binary format)
    static_assert(sizeof(ControlPoint) == sizeof(uint32_t), "ControlPoint size is not correct");

    class ControlPointCache {
      private:
        // Library of control points
        ControlPoint *controlPoints;
        size_t maxControlPoints;

        // Cache the currently active cue to minimize searches
        int cachedCue;					// Cue index that is cached (INDEX_NONE for none)
        unsigned int cachedDay;			// Day of the week the cache is valid for
        unsigned int cachedTime;		// Time (on the cached day) the cache is valid from (inclusive)
        unsigned int cachedUntilTime;	// Time (on the cached day) the cache is valid until (exclusive)

      public:
      
        ControlPointCache(ControlPoint *controlPoints, size_t maxControlPoints) {
            this->controlPoints = controlPoints;
            this->maxControlPoints = maxControlPoints;
            Invalidate();
        }

        void Invalidate() {
            this->cachedCue = INDEX_NONE;
            this->cachedDay = DAY_NONE;
            this->cachedTime = TIME_NONE;
            this->cachedUntilTime = 0;
        }

        // Determine the control point currently active for the given day/time (nullptr if none)
        ControlPoint *CueValue(unsigned int day, unsigned int time)
        {
            // Recompute if cached result was invalidated or expired (out of range)
            if (day != this->cachedDay || this->cachedDay >= ControlPoint::numDays || time < this->cachedTime || this->cachedTime >= ControlPoint::timePerDay || time >= this->cachedUntilTime || this->cachedUntilTime > ControlPoint::timePerDay)
            {
#if 0
printf("[CACHE-MISS: ");
if (day != this->cachedDay) printf("wrong-day;");
if (this->cachedDay >= ControlPoint::numDays) printf("day-invalid;");
if (time < this->cachedTime) printf("time-before;");
if (this->cachedTime >= ControlPoint::timePerDay) printf("time-invalid;");
if (time >= this->cachedUntilTime) printf("until-after;");
if (this->cachedUntilTime > ControlPoint::timePerDay) printf("until-invalid;");
printf("]");
#endif
                int index, next;
                unsigned int elapsed, remaining;
                if (ControlPoint::CueNearest(this->controlPoints, this->maxControlPoints, day, time, &index, &elapsed, &next, &remaining))
                {
                    // Cache the interval clipped to within the current day
                    this->cachedDay = day;
                    this->cachedTime = (elapsed <= time) ? (time - elapsed) : 0;
                    this->cachedUntilTime = (remaining > ControlPoint::timePerDay - time) ? ControlPoint::timePerDay : (time + remaining);
                    this->cachedCue = index;
                }
                else
                {
                    // Cache that no cue was found for this day
                    this->cachedDay = day;
                    this->cachedTime = 0;
                    this->cachedUntilTime = ControlPoint::timePerDay;
                    this->cachedCue = INDEX_NONE;
                }
            }
            // Return the value of the currently-active cue
            if (this->cachedCue < 0 || (size_t)this->cachedCue >= this->maxControlPoints) return nullptr;
            return &this->controlPoints[this->cachedCue];
        }

    };

  }
}
