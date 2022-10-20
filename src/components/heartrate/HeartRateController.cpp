#include "components/heartrate/HeartRateController.h"
#include <heartratetask/HeartRateTask.h>
#include <systemtask/SystemTask.h>

using namespace Pinetime::Controllers;

void HeartRateController::Update(HeartRateController::States newState, uint8_t heartRate) {
  this->state = newState;
  if (this->heartRate != heartRate) {
    this->heartRate = heartRate;
    service->OnNewHeartRateValue(heartRate);
  }
}

void HeartRateController::Start() {
  if (task != nullptr) {
    state = States::NotEnoughData;
    task->PushMessage(Pinetime::Applications::HeartRateTask::Messages::StartMeasurement);
  }
}

void HeartRateController::Stop() {
  if (task != nullptr) {
    state = States::Stopped;
    task->PushMessage(Pinetime::Applications::HeartRateTask::Messages::StopMeasurement);
  }
}

void HeartRateController::SetHeartRateTask(Pinetime::Applications::HeartRateTask* task) {
  this->task = task;
}

void HeartRateController::SetService(Pinetime::Controllers::HeartRateService* service) {
  this->service = service;
}

#ifdef CUEBAND_HR_EPOCH
void HeartRateController::SetHrEpoch(bool hrEpoch) {
  if (task == nullptr) return;
  bool currentlyRunning = IsHrEpoch();
  if (!currentlyRunning && hrEpoch) {
    Start();
  }
  if (currentlyRunning && !hrEpoch) {
    Stop();
  }
  return task->SetHrEpoch(hrEpoch);
}

bool HeartRateController::IsHrEpoch() {
  if (task == nullptr) return false;
  return task->IsHrEpoch();
}

int HeartRateController::HrStats(int *meanBpm, int *minBpm, int *maxBpm, bool clear) {
  if (task == nullptr) return false;
  return task->HrStats(meanBpm, minBpm, maxBpm, clear);
}
#endif

#ifdef CUEBAND_BUFFER_RAW_HR
bool HeartRateController::IsRawMeasurement() {
  if (task == nullptr) return false;
  return task->IsRawMeasurement();
}

void HeartRateController::StartRaw() {
  if (task != nullptr) task->SetRawMeasurement(true);
  Start();
}

void HeartRateController::StopRaw() {
  Stop();
  if (task != nullptr) task->SetRawMeasurement(false);
}

bool HeartRateController::BufferAdd(uint32_t measurement) {
  if (task == nullptr) {
    return false;
  }
  this->task->BufferAdd(measurement);
  return true;
}

// If NULL pointer: count of buffer entries available since previous cursor position
// otherwise: read from buffer from previous cursor position, return count, update cursor position
size_t HeartRateController::BufferRead(uint32_t *data, size_t *cursor, size_t maxCount) {
  if (task == nullptr) {
    return 0;
  }
  return this->task->BufferRead(data, cursor, maxCount);
}
#endif
