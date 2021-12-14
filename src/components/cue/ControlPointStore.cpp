#include <cstring>

#include "ControlPointStore.h"

namespace Pinetime::Controllers {

ControlPointStore::ControlPointStore() : ControlPointStore(nullptr, nullptr, 0) {    
}

ControlPointStore::ControlPointStore(control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints) {
    SetData(controlPoints, scratch, maxControlPoints);
}

// Set backing arrays
void ControlPointStore::SetData(control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints) {
    this->controlPoints = controlPoints;
    this->scratch = scratch;
    this->maxControlPoints = maxControlPoints;
    Invalidate();
}

void ControlPointStore::Reset() {
    ClearScratch();
    CommitScratch(0xFFFFFFFF);
    Invalidate();
}

void ControlPointStore::ClearScratch() {
    ControlPoint clearedValue;
    for (int i = 0; i < maxControlPoints; i++) {
        this->scratch[i] = clearedValue.Value();
    }
    Invalidate();
}

void ControlPointStore::SetScratch(int index, ControlPoint controlPoint) {
    this->scratch[index] = controlPoint.Value();
}

void ControlPointStore::CommitScratch(unsigned int version) {
    memcpy(controlPoints, scratch, maxControlPoints * sizeof(control_point_packed_t));
// TODO: Store version
}

// Invalidate the cache (e.g. if the control points are externally modified)
void ControlPointStore::Invalidate() {
    this->cachedCue = ControlPoint::INDEX_NONE;
    this->cachedDay = ControlPoint::DAY_NONE;
    this->cachedTime = ControlPoint::TIME_NONE;
    this->cachedUntilTime = 0;
}

// Determine the control point currently active for the given day/time (nullptr if none)
ControlPoint ControlPointStore::CueValue(unsigned int day, unsigned int time)
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
            this->cachedCue = ControlPoint::INDEX_NONE;
        }
    }
    // Return the value of the currently-active cue
    if (this->cachedCue < 0 || (size_t)this->cachedCue >= this->maxControlPoints) return ControlPoint();
    return ControlPoint(this->controlPoints[this->cachedCue]);
}

}
