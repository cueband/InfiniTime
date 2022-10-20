#pragma once
#include "cueband.h"

#include <cstdint>
#include <components/ble/HeartRateService.h>

namespace Pinetime {
  namespace Applications {
    class HeartRateTask;
  }
  namespace System {
    class SystemTask;
  }
  namespace Controllers {
    class HeartRateController {
    public:
      enum class States { Stopped, NotEnoughData, NoTouch, Running };

      HeartRateController() = default;
      void Start();
      void Stop();
      void Update(States newState, uint8_t heartRate);

      void SetHeartRateTask(Applications::HeartRateTask* task);
      States State() const {
        return state;
      }
      uint8_t HeartRate() const {
        return heartRate;
      }

      void SetService(Pinetime::Controllers::HeartRateService* service);

#ifdef CUEBAND_HR_EPOCH
      void SetHrEpoch(bool hrEpoch);
      bool IsHrEpoch();
      int HrStats(int *meanBpm, int *minBpm, int *maxBpm, bool clear);
#endif

#ifdef CUEBAND_BUFFER_RAW_HR
      bool IsRawMeasurement();
      void StartRaw();
      void StopRaw();
      
      // Only for adding dummy test measurements
      bool BufferAdd(uint32_t measurement);

      // If NULL pointer: count of buffer entries available since previous cursor position
      // otherwise: read from buffer from previous cursor position, return count, update cursor position
      size_t BufferRead(uint32_t *data, size_t *cursor, size_t maxCount);
#endif

    private:
      Applications::HeartRateTask* task = nullptr;
      States state = States::Stopped;
      uint8_t heartRate = 0;
      Pinetime::Controllers::HeartRateService* service = nullptr;
    };
  }
}