#pragma once

//#include "cueband.h"

#include <cstdlib>

#include "ControlPoint.h"

namespace Pinetime::Controllers {

  class ControlPointStore {
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
    
      ControlPointStore(ControlPoint *controlPoints, size_t maxControlPoints);

      // Invalidate the cache (e.g. if the control points are externally modified)
      void Invalidate();

      // Determine the control point currently active for the given day/time (nullptr if none)
      ControlPoint *CueValue(unsigned int day, unsigned int time);

  };

}
