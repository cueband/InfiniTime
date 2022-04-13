#include "cueband.h"

#include "components/motor/MotorController.h"
#include <hal/nrf_gpio.h>
#include "systemtask/SystemTask.h"
#include "app_timer.h"
#include "drivers/PinMap.h"

APP_TIMER_DEF(shortVibTimer);
APP_TIMER_DEF(longVibTimer);

using namespace Pinetime::Controllers;

void MotorController::Init() {
  nrf_gpio_cfg_output(PinMap::Motor);
  nrf_gpio_pin_set(PinMap::Motor);
  app_timer_init();

  app_timer_create(&shortVibTimer, APP_TIMER_MODE_SINGLE_SHOT, StopMotor);
  app_timer_create(&longVibTimer, APP_TIMER_MODE_REPEATED, Ring);
}

void MotorController::Ring(void* p_context) {
  auto* motorController = static_cast<MotorController*>(p_context);
  motorController->RunForDuration(50);
}

void MotorController::RunForDuration(uint8_t motorDuration) {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  nrf_gpio_pin_clear(PinMap::Motor);
#ifdef CUEBAND_TRACK_MOTOR_TIMES
  TrackActive(motorDuration);
#endif
  app_timer_start(shortVibTimer, APP_TIMER_TICKS(motorDuration), nullptr);
}

void MotorController::StartRinging() {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  Ring(this);
  app_timer_start(longVibTimer, APP_TIMER_TICKS(1000), this);
}

void MotorController::StopRinging() {
#ifdef CUEBAND_MOTOR_PATTERNS
  BeginPattern(nullptr);
#endif
  app_timer_stop(longVibTimer);
  nrf_gpio_pin_set(PinMap::Motor);
}

void MotorController::StopMotor(void* p_context) {
  nrf_gpio_pin_set(PinMap::Motor);
#ifdef CUEBAND_MOTOR_PATTERNS
  auto* motorController = static_cast<MotorController*>(p_context);
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
    nrf_gpio_pin_set(PinMap::Motor);    // Off
    currentPattern = nullptr;
    currentIndex = 0;
    return;
  }
  
  if ((currentIndex & 1) == 0) {
    nrf_gpio_pin_clear(PinMap::Motor);  // On
#ifdef CUEBAND_TRACK_MOTOR_TIMES
    TrackActive(duration);
#endif
  } else {
    nrf_gpio_pin_set(PinMap::Motor);    // Off
  }
  currentIndex++;
  app_timer_start(shortVibTimer, APP_TIMER_TICKS(duration), this);
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
