#include "ControlPoint.h"

namespace Pinetime::Controllers {

    // Empty
    ControlPoint::ControlPoint() {
        Clear();
    }

    // From packed
    ControlPoint::ControlPoint(control_point_packed_t controlPoint) {
        Set(controlPoint);
    }

    // From components
    ControlPoint::ControlPoint(bool enabled, unsigned int weekdays, unsigned int interval, unsigned int volume, unsigned int timeOfDay) {
        Set(enabled, weekdays, interval, volume, timeOfDay);
    }

    // All fields to inactive values
    void ControlPoint::Clear()
    {
        Set(false, ControlPoint::DAY_NONE, ControlPoint::TIME_NONE, ControlPoint::VOLUME_NONE, ControlPoint::TIME_NONE);
    }

    // From packed
    void ControlPoint::Set(control_point_packed_t controlPoint) {
        this->controlPoint = controlPoint;
    }

    // From components
    void ControlPoint::Set(bool enabled, unsigned int weekdays, unsigned int interval, unsigned int volume, unsigned int timeOfDay) {
        this->controlPoint = 
              (enabled ? (1 << 31) : 0)
            | ((weekdays & (1 << numDays)) << 24)
            | ((interval >= (1 << 10) ? ((1 << 10) - 1) : interval) << 14)
            | ((volume & ((1 << 3) - 1)) << 11)
            | (timeOfDay & ((1 << 11) - 1))
            ;
    }

    // Packed value
    control_point_packed_t ControlPoint::Value() {
        return controlPoint;
    }

    // Cue enabled
    bool ControlPoint::IsEnabled()
    {
        return (controlPoint & (1 << 31)) != 0;
    }

    // Weekday bitmap (b0=Sunday, ..., b6=Saturday)
    unsigned int ControlPoint::GetWeekdays()
    {
        unsigned int weekdayBitmap = (controlPoint >> 24) & ((1 << numDays) - 1);
        return weekdayBitmap;
    }

    // Prompt interval in seconds
    unsigned int ControlPoint::GetInterval()
    {
        unsigned int unitInterval = (controlPoint >> 14) & ((1 << 10) - 1);
        return unitInterval * intervalUnitSize;
    }

    // Prompt volume/pattern (0-7)
    unsigned int ControlPoint::GetVolume()
    {
        unsigned int volume = (controlPoint >> 11) & ((1 << 3) - 1);
        return volume;
    }

    // Control point time of day in seconds (0-86399)
    unsigned int ControlPoint::GetTimeOfDay()
    {
        const unsigned int maxUnit = ((timePerDay - 1) / timeUnitSize);
        unsigned int unitOfDay = (controlPoint >> 0) & ((1 << 11) - 1);
        if (unitOfDay > maxUnit) return TIME_NONE;
        return unitOfDay * timeUnitSize;
    }

    // Calculate the smallest interval the control point is before-or-at the given day/time. (TIME_NONE if none)
    unsigned int ControlPoint::CueTimeBefore(unsigned int day, unsigned int targetTime)
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
    unsigned int ControlPoint::CueTimeAfter(unsigned int day, unsigned int targetTime)
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
    // controlPoints - pointer to control_point_packed_t elements
    // numControlPoints - number of control points
    // day - day of the week (bitmap: b0-b7)
    // time - time of the day
    // outIndex - current control point index (<0 if none)
    // outElapsed - duration control point has already been active
    // outNextIndex - next control point index (<0 if none)
    // outRemaining - remaining time until the next control point is active
    bool ControlPoint::CueNearest(control_point_packed_t *controlPoints, size_t numControlPoints, unsigned int day, unsigned int time, int *outIndex, unsigned int *outElapsed, int *outNextIndex, unsigned int *outRemaining)
    {
        int closestIndexBefore = INDEX_NONE;
        unsigned int closestDistanceBefore = TIME_NONE;
        int closestIndexAfter = INDEX_NONE;
        unsigned int closestDistanceAfter = TIME_NONE;
        for (int i = 0; (size_t)i < numControlPoints; i++)
        {
            ControlPoint controlPoint = ControlPoint(controlPoints[i]);
            unsigned int distance;

            if (!controlPoint.IsEnabled())
            {
                continue;
            }
            
            distance = controlPoint.CueTimeBefore(day, time);
            if (distance != TIME_NONE && (distance < closestDistanceBefore || closestIndexBefore != TIME_NONE))
            {
                closestIndexBefore = i;
                closestDistanceBefore = distance;
            }

            distance = controlPoint.CueTimeAfter(day, time);
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

}
