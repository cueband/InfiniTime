#pragma once

#include "cueband.h"

#include <cstdint>
#include <drivers/Bma421.h>
#include <components/ble/MotionService.h>

namespace Pinetime {
  namespace Controllers {
    class MotionController {
    public:
      enum class DeviceTypes {
        Unknown,
        BMA421,
        BMA425,
      };

#ifdef CUEBAND_BUFFER_ENABLED
      void GetBufferData(int16_t **accelValues, unsigned int *lastCount, unsigned int *totalSamples);
      void SetBufferData(int16_t *accelValues, unsigned int lastCount, unsigned int totalSamples);
#endif

      void Update(int16_t x, int16_t y, int16_t z, uint32_t nbSteps);

      int16_t X() const {
        return x;
      }
      int16_t Y() const {
        return y;
      }
      int16_t Z() const {
        return z;
      }
      uint32_t NbSteps() const {
        return nbSteps;
      }

      void ResetTrip() {
        currentTripSteps = 0;
      }
      uint32_t GetTripSteps() const {
        return currentTripSteps;
      }

      bool Should_ShakeWake(uint16_t thresh);
      bool Should_RaiseWake(bool isSleeping);
      int32_t currentShakeSpeed();
      void IsSensorOk(bool isOk);
      bool IsSensorOk() const {
        return isSensorOk;
      }

      DeviceTypes DeviceType() const {
        return deviceType;
      }

      void Init(Pinetime::Drivers::Bma421::DeviceTypes types);
      void SetService(Pinetime::Controllers::MotionService* service);

    private:
      uint32_t nbSteps;
      uint32_t currentTripSteps = 0;
      int16_t x;
      int16_t y;
      int16_t z;
      int16_t lastYForWakeUp = 0;
      bool isSensorOk = false;
      DeviceTypes deviceType = DeviceTypes::Unknown;
      Pinetime::Controllers::MotionService* service = nullptr;

      int16_t lastXForShake = 0;
      int16_t lastYForShake = 0;
      int16_t lastZForShake = 0;
      int32_t accumulatedspeed = 0;
      uint32_t lastShakeTime = 0;

#ifdef CUEBAND_BUFFER_ENABLED
      int16_t *accelValues;
      unsigned int totalSamples;
      unsigned int lastCount;
#endif
    };
  }
}