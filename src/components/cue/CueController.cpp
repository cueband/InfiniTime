#include "cueband.h"

#ifdef CUEBAND_CUE_ENABLED

#include "CueController.h"

using namespace Pinetime::Controllers;

#define CUE_DATA_FILENAME "CUES.BIN"
#define CUE_FILE_VERSION 0
#define CUE_PROMPT_TYPE 0

CueController::CueController(Controllers::Settings& settingsController, 
                             Controllers::FS& fs,
                             Controllers::ActivityController& activityController,
                             Controllers::MotorController& motorController
                            )
                            :
                            settingsController {settingsController},
                            fs {fs},
                            activityController {activityController},
                            motorController {motorController}
                            {
    store.SetData(Pinetime::Controllers::ControlPointStore::VERSION_NONE, controlPoints, scratch, sizeof(controlPoints) / sizeof(controlPoints[0]));
    store.Reset();  // Clear all control points, version, and scratch.
    SetInterval(INTERVAL_OFF, MAXIMUM_RUNTIME_INFINITE, DEFAULT_MOTOR_PULSE_WIDTH);
}


// Called at 1Hz
void CueController::TimeChanged(uint32_t timestamp, uint32_t uptime) {
    // currentTime = time;
    // currentUptime = uptime;

Pinetime::Controllers::ControlPoint controlPoint = store.CueValue(timestamp);
unsigned int cueValue = controlPoint.GetInterval();
//if (controlPoint.IsEnabled() && cueValue > 0)
    
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
    // Store currently contains the Reset() values (e.g. VERSION_NONE and no enabled cues)
    uint32_t version = store.GetVersion();
    // Read from file
    readError = ReadCues(&version);
    // Notify control points externally modified
    store.Updated(version);
    // Notify activity controller of current version
    activityController.PromptConfigurationChanged(store.GetVersion());
}

int CueController::ReadCues(uint32_t *version) {
    int ret;

    // Restore previous control points
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, CUE_DATA_FILENAME, LFS_O_RDONLY);
    if (ret == LFS_ERR_CORRUPT) fs.FileDelete(CUE_DATA_FILENAME);    // No other sensible action?
    if (ret != LFS_ERR_OK) {
        return 1;
    }

    // Read header
    uint8_t headerBuffer[20];
    ret = fs.FileRead(&file_p, headerBuffer, sizeof(headerBuffer));
    if (ret != sizeof(headerBuffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    // Parse header
    bool headerValid = (headerBuffer[0] == 'C' && headerBuffer[1] == 'U' && headerBuffer[2] == 'E' && headerBuffer[3] == 'S');
    unsigned int fileVersion = headerBuffer[4] + (headerBuffer[5] << 8) + (headerBuffer[6] << 16) + (headerBuffer[7] << 24);
    unsigned int promptType = headerBuffer[8] + (headerBuffer[9] << 8) + (headerBuffer[10] << 16) + (headerBuffer[11] << 24);
    unsigned int promptCount = headerBuffer[12] + (headerBuffer[13] << 8) + (headerBuffer[14] << 16) + (headerBuffer[15] << 24);
    if (version != NULL) {
        *version = headerBuffer[16] + (headerBuffer[17] << 8) + (headerBuffer[18] << 16) + (headerBuffer[19] << 24);
    }
    if (!headerValid || fileVersion > CUE_FILE_VERSION || promptType != CUE_PROMPT_TYPE) {
        fs.FileClose(&file_p);
        return 3;
    }

    // Read prompts
    unsigned int maxControls = sizeof(controlPoints) / sizeof(controlPoints[0]);
    unsigned int count = (promptCount > maxControls) ? maxControls : promptCount;
    ret = fs.FileRead(&file_p, (uint8_t *)&controlPoints[0], count * sizeof(controlPoints[0]));

    if (ret != (int)(count * sizeof(controlPoints[0]))) {
        fs.FileClose(&file_p);
        return 4;
    }

    // Position after cue values
    unsigned int pos = promptCount * sizeof(controlPoints[0]) + sizeof(headerBuffer);
    ret = fs.FileSeek(&file_p, pos);
    if (ret != (int)pos) {
        fs.FileClose(&file_p);
        return 5;
    }

    fs.FileClose(&file_p);
    return 0;
}

int CueController::WriteCues() {
    int ret;

    int promptCount = sizeof(controlPoints) / sizeof(controlPoints[0]);
    uint32_t promptVersion = store.GetVersion();

    uint8_t headerBuffer[20];
    headerBuffer[0] = 'C'; headerBuffer[1] = 'U'; headerBuffer[2] = 'E'; headerBuffer[3] = 'S'; 
    headerBuffer[4] = (uint8_t)CUE_FILE_VERSION; headerBuffer[5] = (uint8_t)(CUE_FILE_VERSION >> 8); headerBuffer[6] = (uint8_t)(CUE_FILE_VERSION >> 16); headerBuffer[7] = (uint8_t)(CUE_FILE_VERSION >> 24); 
    headerBuffer[8] = (uint8_t)CUE_PROMPT_TYPE; headerBuffer[9] = (uint8_t)(CUE_PROMPT_TYPE >> 8); headerBuffer[10] = (uint8_t)(CUE_PROMPT_TYPE >> 16); headerBuffer[11] = (uint8_t)(CUE_PROMPT_TYPE >> 24); 
    headerBuffer[12] = (uint8_t)promptCount; headerBuffer[13] = (uint8_t)(promptCount >> 8); headerBuffer[14] = (uint8_t)(promptCount >> 16); headerBuffer[15] = (uint8_t)(promptCount >> 24); 
    headerBuffer[16] = (uint8_t)promptVersion; headerBuffer[17] = (uint8_t)(promptVersion >> 8); headerBuffer[18] = (uint8_t)(promptVersion >> 16); headerBuffer[19] = (uint8_t)(promptVersion >> 24); 
    
    // Open control points file for writing
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, CUE_DATA_FILENAME, LFS_O_WRONLY|LFS_O_CREAT|LFS_O_APPEND);
    if (ret == LFS_ERR_CORRUPT) fs.FileDelete(CUE_DATA_FILENAME);    // No other sensible action?
    if (ret != LFS_ERR_OK) {
        return 1;
    }

    // Write header
    ret = fs.FileWrite(&file_p, headerBuffer, sizeof(headerBuffer));
    if (ret != sizeof(headerBuffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    // Write cues
    ret = fs.FileWrite(&file_p, (uint8_t *)&controlPoints[0], promptCount * sizeof(controlPoints[0]));
    if (ret != (int)(promptCount * sizeof(controlPoints[0]))) {
        fs.FileClose(&file_p);
        return 3;
    }

    fs.FileClose(&file_p);
    return 0;
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
