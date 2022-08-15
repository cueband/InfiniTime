#include "heartratetask/HeartRateTask.h"
#include <drivers/Hrs3300.h>
#include <components/heartrate/HeartRateController.h>
#include <nrf_log.h>

using namespace Pinetime::Applications;

#ifdef CUEBAND_BUFFER_RAW_HR
#include "components/activity/compander.h"
#endif

HeartRateTask::HeartRateTask(Drivers::Hrs3300& heartRateSensor, Controllers::HeartRateController& controller)
  : heartRateSensor {heartRateSensor}, controller {controller}, ppg {} {
}

void HeartRateTask::Start() {
  messageQueue = xQueueCreate(10, 1);
  controller.SetHeartRateTask(this);

  if (pdPASS != xTaskCreate(HeartRateTask::Process, "Heartrate", 500, this, 0, &taskHandle))
    APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
}

void HeartRateTask::Process(void* instance) {
  auto* app = static_cast<HeartRateTask*>(instance);
  app->Work();
}

void HeartRateTask::Work() {
  int lastBpm = 0;
  while (true) {
    Messages msg;
    uint32_t delay;
    if (state == States::Running) {
      if (measurementStarted)
        delay = 40;
      else
        delay = 100;
    } else
      delay = portMAX_DELAY;

    if (xQueueReceive(messageQueue, &msg, delay)) {
      switch (msg) {
        case Messages::GoToSleep:
#ifdef CUEBAND_BUFFER_RAW_HR
          if (IsRawMeasurement()) break;
#endif
          StopMeasurement();
          state = States::Idle;
          break;
        case Messages::WakeUp:
#ifdef CUEBAND_BUFFER_RAW_HR
          if (IsRawMeasurement()) break;
#endif
          state = States::Running;
          if (measurementStarted) {
            lastBpm = 0;
            StartMeasurement();
          }
          break;
        case Messages::StartMeasurement:
          if (measurementStarted)
            break;
          lastBpm = 0;
          StartMeasurement();
          measurementStarted = true;
          break;
        case Messages::StopMeasurement:
          if (!measurementStarted)
            break;
          StopMeasurement();
          measurementStarted = false;
          break;
      }
    }

    if (measurementStarted) {
#ifdef CUEBAND_BUFFER_RAW_HR
      uint32_t hrs = heartRateSensor.ReadHrs();
      uint32_t als = heartRateSensor.ReadAls();
      ppg.Preprocess(static_cast<float>(hrs));
#else
      ppg.Preprocess(static_cast<float>(heartRateSensor.ReadHrs()));
#endif
      auto bpm = ppg.HeartRate();

      if (lastBpm == 0 && bpm == 0)
        controller.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
      if (bpm != 0) {
        lastBpm = bpm;
        controller.Update(Controllers::HeartRateController::States::Running, lastBpm);
#ifdef CUEBAND_BUFFER_RAW_HR
lastMeasurement = lastBpm;
lastMeasurementAge = 0;
#endif
      }

#ifdef CUEBAND_BUFFER_RAW_HR
      if (lastMeasurementAge++ > 5 * 25) {
        lastMeasurementAge = 0;
        lastMeasurement = 0;
      }
#if 1   // Include BPM and companded ALS (only suitable for rough inspection), rather than raw ALS
      uint32_t hrmValue = (lastMeasurement << 24) | ((uint32_t)compander_compress((uint16_t)als) << 16) | hrs;
#else
      uint32_t hrmValue = (als << 16) | hrs;
#endif
      BufferAdd(hrmValue);
#endif
    }
  }
}

void HeartRateTask::PushMessage(HeartRateTask::Messages msg) {
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(messageQueue, &msg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    /* Actual macro used here is port specific. */
    // TODO : should I do something here?
  }
}

void HeartRateTask::StartMeasurement() {
#ifdef CUEBAND_BUFFER_RAW_HR
  numSamples = 0;
#endif
  heartRateSensor.Enable();
  vTaskDelay(100);
  ppg.SetOffset(static_cast<float>(heartRateSensor.ReadHrs()));
}

void HeartRateTask::StopMeasurement() {
  heartRateSensor.Disable();
  vTaskDelay(100);
#ifdef CUEBAND_BUFFER_RAW_HR
  numSamples = 0;
#endif
}

#ifdef CUEBAND_BUFFER_RAW_HR
void HeartRateTask::BufferAdd(uint32_t measurement) {
  hrmBuffer[numSamples++ % hrmCapacity] = measurement;
}

// If NULL pointer: count of buffer entries available since previous cursor position
// otherwise: read from buffer from previous cursor position, return count, update cursor position
size_t HeartRateTask::BufferRead(uint32_t *data, size_t *cursor, size_t maxCount) {
  // TODO: Although this is just for a quick test right now, this needs synchronization to be correct
  size_t first = *cursor;
  size_t last = numSamples;
  // Reset cursor if samples wrapped or restarted
  if (first > last) first = 0;
  // Bump cursor if samples overflowed
  if (last > hrmCapacity && first < last - hrmCapacity) first = last - hrmCapacity;
  // Requested count
  size_t count = last - first;
  if (count > maxCount) count = maxCount;

  // If buffer specified:
  if (data != nullptr) {
    // Copy out data
    for (size_t i = 0; i < count; i++) {
      data[i] = hrmBuffer[(first + i) % hrmCapacity];
    }
    // Update cursor
    *cursor = first + count;
  }

  // Return count
  return count;
}
#endif
