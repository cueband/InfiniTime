#pragma once

//#include "cueband.h"

#include <cstdlib>
#include <cstdint>

namespace Pinetime::Controllers {

    // Packed type (32-bit)
    typedef uint32_t control_point_packed_t;

    struct ControlPoint {

        static const unsigned int TIME_NONE = ((unsigned int)-1);   // 0xffffffff
        static const unsigned int DAY_NONE = ((unsigned int)-1);    // 0xffffffff
        static const unsigned int INTERVAL_NONE = 0;
        static const int INDEX_NONE = -1;
        static const unsigned int VOLUME_NONE = 0;        

        // Tightly packed control point representation
        //   EDDDDDDD IIIIIIII IIVVVTTT TTTTTTTT
        //   E=<1b>  enabled (1=cue enabled, 0=cue disabled)
        //   D=<7b>  weekdays bitmap the cue applies (b0-b6=Sun-Sat, 0b0000000=disabled)
        //   I=<10b> interval (0-1023 seconds, up to 17m03s)
        //   V=<3b>  volume / pattern setting (0-7, 0=silent)
        //   T=<11b> time of day the cue is valid (minute, 0-1439) (PREVIOUSLY: units of 5 minutes/300 seconds, 0-287; 0x1ff=disabled)
        control_point_packed_t controlPoint;

        static const unsigned int numDays = 7;                  // days in a week (Sun-Sat)
        static const unsigned int timePerDay = 24 * 60 * 60;    // 86400 seconds in a day
        static const unsigned int timeUnitSize = 60;            // 1 minute time-of-day storage unit
        static const unsigned int intervalUnitSize = 1;         // 1 second interval storage unit

        ControlPoint();
        ControlPoint(control_point_packed_t controlPoint);
        ControlPoint(bool enabled, unsigned int weekdays, unsigned int interval, unsigned int volume, unsigned int timeOfDay);

        // All fields to inactive values
        void Clear();

        // From packed
        void Set(control_point_packed_t controlPoint);

        // From components
        void Set(bool enabled, unsigned int weekdays, unsigned int interval, unsigned int volume, unsigned int timeOfDay);

        // Packed value
        control_point_packed_t Value();

        // Cue is valid/set (not whether this is a prompt/rest period)
        bool IsEnabled();

        // Whether this beings a non-prompting period
        bool IsNonPrompting();

        // Weekday bitmap (b0=Sunday, ..., b6=Saturday)
        unsigned int GetWeekdays();

        // Prompt interval in seconds
        unsigned int GetInterval();

        // Prompt volume/pattern (0-7)
        unsigned int GetVolume();

        // Control point time of day in seconds (0-86399)
        unsigned int GetTimeOfDay();

        // Calculate the smallest interval the control point is before-or-at the given day/time. (TIME_NONE if none)
        unsigned int CueTimeBefore(unsigned int day, unsigned int targetTime);

        // Calculate the smallest interval the control point is strictly after the given day/time. (TIME_NONE if none)
        unsigned int CueTimeAfter(unsigned int day, unsigned int targetTime);

        // Find the nearest control points to the given day and time-of-day, both 'before-or-at' and 'after'. false if no valid points.
        // controlPoints - pointer to control_point_packed_t elements
        // numControlPoints - number of control points
        // day - day of the week (0-7)
        // time - time of the day
        // outIndex - current control point index (<0 if none)
        // outElapsed - duration control point has already been active
        // outNextIndex - next control point index (<0 if none)
        // outRemaining - remaining time until the next control point is active
        static bool CueNearest(control_point_packed_t *controlPoints, size_t numControlPoints, unsigned int day, unsigned int time, int *outIndex, unsigned int *outElapsed, int *outNextIndex, unsigned int *outRemaining, bool ignoreAdjacentEquivalent);

        static bool Equivalent(ControlPoint a, ControlPoint b) {
            // Not equivalent if either is invalid (not set)
            if (!a.IsEnabled() || !b.IsEnabled()) return false;

            // Equivalent if both are non-prompting
            if (a.IsNonPrompting() && b.IsNonPrompting()) return true;

            // (Optional) Equivalent if both are prompting (neither are non-prompting) and are exactly the same interval and volume/intensity
            //if (!a.IsNonPrompting() && !b.IsNonPrompting() && a.GetInterval() == b.GetInterval() && a.GetVolume() == b.GetVolume()) return true;
            
            // Not equivalent otherwise
            return false;
        }

    };

    // Confirm packing (used directly as binary format)
    static_assert(sizeof(ControlPoint) == sizeof(control_point_packed_t), "ControlPoint size is not correct");
    static_assert(alignof(ControlPoint) == alignof(control_point_packed_t), "ControlPoint alignment is not correct");

}

