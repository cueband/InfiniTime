#pragma once

//#include "cueband.h"

#include <cstdlib>
#include <cstdint>

#include "ControlPoint.h"

namespace Pinetime::Controllers {

  class ControlPointStore {
    private:
      // Library of packed control points
      control_point_packed_t *controlPoints;
      control_point_packed_t *scratch;
      size_t maxControlPoints;
      uint32_t version = VERSION_NONE;

      // Cache the currently active cue to minimize searches
      int cachedCue;					// Cue index that is cached (INDEX_NONE for none)
      unsigned int cachedDay;			// Day of the week the cache is valid for
      unsigned int cachedTime;		// Time (on the cached day) the cache is valid from (inclusive)
      unsigned int cachedUntilTime;	// Time (on the cached day) the cache is valid until (exclusive)

      // Invalidate the cache
      void Invalidate();

    public:

      static const unsigned int VERSION_NONE = (unsigned int)-1;

      // Construct no store  
      ControlPointStore();

      // Construct with specified backing arrays
      ControlPointStore(uint32_t version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints);

      // Set backing arrays
      void SetData(uint32_t version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints);

      // Erase stored and scratch control points
      void Reset();

      // Erase all scratch control points
      void ClearScratch();

      // Get specific stored control point
      ControlPoint GetStored(int index);

      // Set specific scratch control point
      void SetScratch(int index, ControlPoint controlPoint);

      // Commit scratch points as current version
      void CommitScratch(uint32_t version);

      // Control points have been (externally) modified
      void Updated(uint32_t version);

      // Determine the control point currently active for the given day/time-of-day
      ControlPoint CueValue(unsigned int day, unsigned int time, int *cueIndex = nullptr, unsigned int *cueRemaining = nullptr);

      // Determine the control point currently active for the given epoch timestamp
      ControlPoint CueValue(unsigned int timestamp, int *cueIndex = nullptr, unsigned int *cueRemaining = nullptr);

      uint32_t GetVersion() { return version; }

  };

}
