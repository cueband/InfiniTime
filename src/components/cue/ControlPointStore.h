#pragma once

//#include "cueband.h"

#include <cstdlib>

#include "ControlPoint.h"

namespace Pinetime::Controllers {

  class ControlPointStore {
    private:
      // Library of packed control points
      control_point_packed_t *controlPoints;
      control_point_packed_t *scratch;
      size_t maxControlPoints;
      unsigned int version = VERSION_NONE;

      // Cache the currently active cue to minimize searches
      int cachedCue;					// Cue index that is cached (INDEX_NONE for none)
      unsigned int cachedDay;			// Day of the week the cache is valid for
      unsigned int cachedTime;		// Time (on the cached day) the cache is valid from (inclusive)
      unsigned int cachedUntilTime;	// Time (on the cached day) the cache is valid until (exclusive)

    public:
    
      static const unsigned int VERSION_NONE = (unsigned int)-1;

      // Construct no store  
      ControlPointStore();

      // Construct with specified backing arrays
      ControlPointStore(unsigned int version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints);

      // Set backing arrays
      void SetData(unsigned int version, control_point_packed_t *controlPoints, control_point_packed_t *scratch, size_t maxControlPoints);

      // Erase stored and scratch control points
      void Reset();

      // Erase all scratch control points
      void ClearScratch();

      // Set specific scratch control point
      void SetScratch(int index, ControlPoint controlPoint);

      // Commit scratch points as current version
      void CommitScratch(unsigned int version);

      // Invalidate the cache (e.g. if the control points are externally modified)
      void Invalidate();

      // Determine the control point currently active for the given day/time (nullptr if none)
      ControlPoint CueValue(unsigned int day, unsigned int time);

  };

}
