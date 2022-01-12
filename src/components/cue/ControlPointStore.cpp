#include <cstring>
#include <cstdio>

#include "ControlPointStore.h"

#define CUE_DEBUG_CONTROL_POINT_CACHE

namespace Pinetime::Controllers {

ControlPointStore::ControlPointStore() : ControlPointStore(VERSION_NONE, nullptr, nullptr, 0) {    
}

ControlPointStore::ControlPointStore(uint32_t version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints) {
    SetData(version, controlPoints, scratch, maxControlPoints);
}

// Set backing arrays
void ControlPointStore::SetData(uint32_t version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints) {
    this->version = version;
    this->controlPoints = controlPoints;
    this->scratch = scratch;
    this->maxControlPoints = maxControlPoints;
    Invalidate();
}

void ControlPointStore::Reset() {
    ClearScratch();
    CommitScratch(VERSION_NONE);
}

void ControlPointStore::ClearScratch() {
    ControlPoint clearedValue;
    for (size_t i = 0; i < maxControlPoints; i++) {
        this->scratch[i] = clearedValue.Value();
    }
}

void ControlPointStore::SetScratch(int index, ControlPoint controlPoint) {
    this->scratch[index] = controlPoint.Value();
}

void ControlPointStore::CommitScratch(uint32_t version) {
    memcpy(controlPoints, scratch, maxControlPoints * sizeof(control_point_packed_t));
    Updated(version);
}

// Control points have been (externally) modified
void ControlPointStore::Updated(uint32_t version) {
    this->version = version;
    Invalidate();
}

// Invalidate the cache (e.g. if the control points are externally modified)
void ControlPointStore::Invalidate() {
    this->cachedCue = ControlPoint::INDEX_NONE;
    this->cachedDay = ControlPoint::DAY_NONE;
    this->cachedTime = ControlPoint::TIME_NONE;
    this->cachedUntilTime = 0;
}

// Determine the control point currently active for the given day/time
ControlPoint ControlPointStore::CueValue(unsigned int day, unsigned int time, int *cueIndex, unsigned int *currentCueCachedRemaining)
{
#if 0
printf("[ALWAYS-INVALIDATE]");
this->cachedDay = ControlPoint::DAY_NONE;
#endif
    // Recompute if cached result was invalidated or expired (out of range)
    if (day != this->cachedDay || this->cachedDay >= ControlPoint::numDays || time < this->cachedTime || this->cachedTime >= ControlPoint::timePerDay || time >= this->cachedUntilTime || this->cachedUntilTime > ControlPoint::timePerDay)
    {
#ifdef CUE_DEBUG_CONTROL_POINT_CACHE
printf("[CACHE-MISS: ");
if (this->cachedDay == ControlPoint::DAY_NONE) printf("was-invalidated;");
else {
if (day != this->cachedDay) printf("wrong-day;");
if (this->cachedDay >= ControlPoint::numDays) printf("day-invalid;");
if (time < this->cachedTime) printf("time-before;");
if (this->cachedTime >= ControlPoint::timePerDay) printf("time-invalid;");
if (time >= this->cachedUntilTime) printf("until-after;");
if (this->cachedUntilTime > ControlPoint::timePerDay) printf("until-invalid;");
}
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
#ifdef CUE_DEBUG_CONTROL_POINT_CACHE
printf("[CACHE: Within control point #%d, elapsed %d, remaining %d, next #%d; cached times %d-%d on day #%d.]", index, elapsed, remaining, next, this->cachedTime, this->cachedUntilTime, this->cachedDay);
#endif
        }
        else
        {
            // Cache that no cue was found for this day
            this->cachedDay = day;
            this->cachedTime = 0;
            this->cachedUntilTime = ControlPoint::timePerDay;
            this->cachedCue = ControlPoint::INDEX_NONE;
#ifdef CUE_DEBUG_CONTROL_POINT_CACHE
printf("[CACHE: No control points all day (%d-%d) on day #%d.]", this->cachedTime, this->cachedUntilTime, this->cachedDay);
#endif
        }
    }

    // Return additional info
    if (cueIndex != nullptr) {
        *cueIndex = this->cachedCue;
    }
    if (currentCueCachedRemaining != nullptr) {
        *currentCueCachedRemaining = this->cachedUntilTime - time;
    }

    // Return the value of the currently-active cue
    if (this->cachedCue < 0 || (size_t)this->cachedCue >= this->maxControlPoints) return ControlPoint();
    return ControlPoint(this->controlPoints[this->cachedCue]);
}

// Determine the control point currently active for the given day/time
ControlPoint ControlPointStore::CueValue(unsigned int timestamp, int *cueIndex, unsigned int *currentCueCachedRemaining)
{
    unsigned int timeOfDay = timestamp % 86400;
    unsigned int day = ((timestamp / 86400) + 4) % 7;    // Epoch is day 4=Thu
    Pinetime::Controllers::ControlPoint controlPoint = CueValue(day, timeOfDay, cueIndex, currentCueCachedRemaining);
    return controlPoint;
}


}
