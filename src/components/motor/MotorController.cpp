#include "cueband.h"

#include "components/motor/MotorController.h"
#include <hal/nrf_gpio.h>
#include "systemtask/SystemTask.h"
#include "drivers/PinMap.h"

using namespace Pinetime::Controllers;

void MotorController::Init() {
  nrf_gpio_cfg_output(PinMap::Motor);
  nrf_gpio_pin_set(PinMap::Motor);

  shortVib = xTimerCreate("shortVib", 1, pdFALSE, 
#ifdef CUEBAND_MOTOR_PATTERNS
    this      // Context required to advance pattern
#else
    nullptr
#endif
    , StopMotor);
  longVib = xTimerCreate("longVib", pdMS_TO_TICKS(1000), pdTRUE, this, Ring);
}

void MotorController::Ring(TimerHandle_t xTimer) {
  auto* motorController = static_cast<MotorController*>(pvTimerGetTimerID(xTimer));
  motorController->RunForDuration(50);
}

void MotorController::RunForDuration(uint8_t motorDuration) {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  if (xTimerChangePeriod(shortVib, pdMS_TO_TICKS(motorDuration), 0) == pdPASS && xTimerStart(shortVib, 0) == pdPASS) {
    nrf_gpio_pin_clear(PinMap::Motor);
#ifdef CUEBAND_TRACK_MOTOR_TIMES
    TrackActive(motorDuration);
#endif
  }
}

void MotorController::StartRinging() {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  RunForDuration(50);
  xTimerStart(longVib, 0);
}

void MotorController::StopRinging() {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  xTimerStop(longVib, 0);
  nrf_gpio_pin_set(PinMap::Motor);
}

void MotorController::StopMotor(TimerHandle_t xTimer) {
  nrf_gpio_pin_set(PinMap::Motor);
#ifdef CUEBAND_MOTOR_PATTERNS
  auto* motorController = static_cast<MotorController*>(pvTimerGetTimerID(xTimer));
  if (motorController != nullptr) {
    motorController->AdvancePattern();
  }
#endif
}

#ifdef CUEBAND_MOTOR_PATTERNS

const int patternNone[]        = { 0 };
const int patternShort[]       = { 100, 0 };
const int patternMedium[]      = { 175, 0 };
const int patternLong[]        = { 250, 0 };
const int patternDoubleShort[] = { 100, 100,  100, 0 };
const int patternDoubleLong[]  = { 250, 250,  250, 0 };
const int patternTripleShort[] = { 100, 100,  100, 100,  100, 0 };   // "PDCue v3.0 MSD"
const int patternTripleLong[]  = { 250, 250,  250, 250,  250, 0 };   // "CWA-PDQ"

const int *patterns[] = {
  patternNone,          // 0
  patternShort,         // 1
  patternMedium,        // 2
  patternLong,          // 3
  patternDoubleShort,   // 4
  patternDoubleLong,    // 5
  patternTripleShort,   // 6
  patternTripleLong,    // 7
};

void MotorController::RunIndex(uint32_t index) {
#ifdef CUEBAND_MOTOR_PATTERNS
  currentPattern = nullptr;
#endif
  if (index < sizeof(patterns) / sizeof(patterns[0])) {
    BeginPattern(patterns[index]);
  } else {
    unsigned int motorPulseWidth = index;
    if (motorPulseWidth > 255) motorPulseWidth = 255;
    RunForDuration(motorPulseWidth);
  }
}

void MotorController::BeginPattern(const int *pattern) {
  currentPattern = pattern;
  currentIndex = 0;
  AdvancePattern();
}

void MotorController::AdvancePattern() {
  if (currentPattern == nullptr) return;

  int duration = currentPattern[currentIndex];

  if (duration <= 0) {
    nrf_gpio_pin_set(PinMap::Motor);    // Off at end of pattern
    currentPattern = nullptr;
    currentIndex = 0;
    return;
  }
  
  if (xTimerChangePeriod(shortVib, pdMS_TO_TICKS(duration), 0) == pdPASS && xTimerStart(shortVib, 0) == pdPASS) {
    if ((currentIndex & 1) == 0) {
      nrf_gpio_pin_clear(PinMap::Motor);  // On phase of pattern
#ifdef CUEBAND_TRACK_MOTOR_TIMES
    TrackActive(duration);
#endif
    } else {
      nrf_gpio_pin_set(PinMap::Motor);    // Off phase of pattern
    }
  } else {
    nrf_gpio_pin_set(PinMap::Motor);    // Off because of timer error
  }
  currentIndex++;
}

#ifdef CUEBAND_TRACK_MOTOR_TIMES
void MotorController::TrackActive(unsigned int timeMs) {
  uptime1024_t now = dateTimeController.Uptime1024();
  uptime1024_t end = now + ((timeMs + CUEBAND_TRACK_MOTOR_TIMES) * 1024 / 1000);
  if (end > this->lastMovement) {
    this->lastMovement = end;
  }
  // Check sensible range (in case of corruption -- overflow should be impossible with 64-bit)
  //if (now + (60 * 1024) < this->lastMovement) { this->lastMovement = now; }
}
#endif

#endif
