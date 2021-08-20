#include "cueband.h"

#ifdef CUEBAND_CUE_ENABLED

#include "CueController.h"

using namespace Pinetime::Controllers;

CueController::CueController(Controllers::Settings& settingsController, 
                             Controllers::ActivityController& activityController,
                             Controllers::MotorController& motorController
                            )
                            :
                            settingsController {settingsController},
                            activityController {activityController},
                            motorController {motorController}
                            {
    SetInterval(INTERVAL_OFF, MAXIMUM_RUNTIME_INFINITE, DEFAULT_MOTOR_PULSE_WIDTH);
}


// Called at 1Hz
void CueController::TimeChanged(uint32_t time) {
    currentTime = time;

    // interval=0 is disabled, and run-time is within maximum duration
    if (interval != 0 and tick < maximumRuntime) {
        // every N seconds
        if (tick % interval == 0) {
            if (motorPulseWidth > 0) {
                motorController.RunForDuration(motorPulseWidth);   // milliseconds

                // Notify activityController of prompt
                activityController.PromptGiven(false);
                promptCount++;
            } else {
                // TODO: Is a zero-width really "muted"? (Should probably only count snoozed ones)
                activityController.PromptGiven(true);
                mutedCount++;
            }
        }
    }

    // Second count
    tick++;
}

void CueController::Init() {
    // TODO: Update if required
    activityController.PromptConfigurationChanged(promptConfigurationId);
}

void CueController::SetInterval(unsigned int interval, unsigned int maximumRuntime, unsigned int motorPulseWidth) {
    if (motorPulseWidth > 1000) motorPulseWidth = 1000;
    if (this->interval != 0) {
        // Start with duration since previous prompt
        tick %= this->interval;
        // ...immediate prompt if greater than interval
        if (tick > interval) {
            tick = 0;
        }
    } else {
        // Immediate prompt if no previous interval
        tick = 0;
    }
    this->interval = interval;
    this->maximumRuntime = maximumRuntime;
    this->motorPulseWidth = motorPulseWidth;

    activityController.Event(ACTIVITY_EVENT_CUE_TEMPORARY);
}


#endif
