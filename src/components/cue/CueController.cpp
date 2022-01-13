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
    SetInterval(INTERVAL_OFF, MAXIMUM_RUNTIME_OFF);
}


// Called at 1Hz
void CueController::TimeChanged(uint32_t timestamp, uint32_t uptime) {
    bool snoozed = false;

    descriptionValid = false;

    currentTime = timestamp;
    currentUptime = uptime;

    unsigned int effectivePromptStyle = DEFAULT_PROMPT_STYLE;

    // Get scheduled interval (0=none)
    currentControlPoint = store.CueValue(timestamp, &currentCueIndex, &cueRemaining);
    unsigned int cueInterval = currentControlPoint.GetInterval();
    if (!currentControlPoint.IsEnabled()) cueInterval = 0;

#ifdef CUEBAND_DETECT_UNSET_TIME
    // Ignore prompt schedule if current time is not set
    if (currentTime < CUEBAND_DETECT_UNSET_TIME) {
        cueInterval = 0;
    }
#endif
    if (cueInterval > 0) {
        effectivePromptStyle = currentControlPoint.GetVolume();
    }

    unsigned int effectiveInterval = cueInterval;

    // If scheduled prompting is disabled...
    if (!IsEnabled()) {
        activityController.Event(ACTIVITY_EVENT_CUE_DISABLED);
        effectiveInterval = 0;
    }

    // If temporary prompt/snooze...
    if (currentUptime < overrideEndTime) {
        if (interval != 0) {  // if temporary prompting...
            effectiveInterval = interval;
            effectivePromptStyle = promptStyle;
            activityController.Event(ACTIVITY_EVENT_CUE_MANUAL);
        } else {
            activityController.Event(ACTIVITY_EVENT_CUE_SNOOZE);
            snoozed = true;
        }
    }

    // Overridden or scheduled interval
    if (effectiveInterval > 0) {
        // prompt every N seconds
        uint32_t elapsed = (lastPrompt != UPTIME_NONE) ? (currentUptime - lastPrompt) : UPTIME_NONE;
        bool prompt = (elapsed == UPTIME_NONE || elapsed >= effectiveInterval);
        if (prompt) {
            if (!snoozed) {
                // Use as raw width unless matching a style number
                unsigned int motorPulseWidth = effectivePromptStyle;

                // TODO: Customize this range and/or add patterns
                switch (effectivePromptStyle % 8) { 
                    case 0: motorPulseWidth = 15; break;
                    case 1: motorPulseWidth = 30; break;
                    case 2: motorPulseWidth = 45; break;
                    case 3: motorPulseWidth = 60; break;
                    case 4: motorPulseWidth = 75; break;
                    case 5: motorPulseWidth = 90; break;
                    case 6: motorPulseWidth = 105; break;
                    case 7: motorPulseWidth = 120; break;
                }

                // Safety limit
                if (motorPulseWidth > 10 * 1000) motorPulseWidth = 10 * 1000;

                // Prompt
                motorController.RunForDuration(motorPulseWidth);   // milliseconds

                // Notify activityController of prompt
                activityController.PromptGiven(false);
            } else {
                // Snoozed prompt
                activityController.PromptGiven(true);
            }

            // Record last prompt/snoozed-prompt
            lastPrompt = currentUptime;
        }
    }
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

void CueController::GetStatus(uint32_t *active_schedule_id, uint16_t *max_control_points, uint16_t *current_control_point, uint16_t *override_remaining, uint16_t *intensity, uint16_t *interval, uint16_t *duration) {
    bool scheduled = IsScheduled();

    if (active_schedule_id != nullptr) {
        *active_schedule_id = store.GetVersion();
    }
    if (max_control_points != nullptr) {
        *max_control_points = PROMPT_MAX_CONTROLS;
    }
    if (current_control_point != nullptr) {
        *current_control_point = (uint16_t)currentCueIndex;
    }
    unsigned int remaining = scheduled ? 0 : (overrideEndTime - currentUptime);
    if (override_remaining != nullptr) {
        *override_remaining = (uint16_t)(remaining > 0xffff ? 0xffff : remaining);
    }
    if (intensity != nullptr) {
        if (scheduled) {
            *intensity = (uint16_t)currentControlPoint.GetVolume();
        } else {
            *intensity = promptStyle;
        }
    }
    if (interval != nullptr) {
        if (scheduled) {
            *intensity = (uint16_t)currentControlPoint.GetInterval();
        } else {
            *interval = (uint16_t)(this->interval > 0xffff ? 0xffff : this->interval);
        }
    }
    if (duration != nullptr) {
        *duration = (uint16_t)(cueRemaining > 0xffff ? 0xffff : cueRemaining);
    }
}

options_t CueController::GetOptionsMaskValue(options_t *mask, options_t *value) {
    // overridden_mask:  0=default, 1=remote-set
    // overridden_value: 0=off, 1=on
    options_t effectiveValue = (OPTIONS_DEFAULT & ~options_overridden_mask) | (options_overridden_mask & options_overridden_value);

    if (mask != nullptr) *mask = options_overridden_mask;
    if (value != nullptr) *value = options_overridden_value;

    return effectiveValue;
}

void CueController::SetOptionsMaskValue(options_t mask, options_t value) {
    options_t oldMask = options_overridden_mask;
    options_t oldValue = options_overridden_value & options_overridden_mask;

    // (mask,value)
    // (0,0) - do not change option
    // (0,1) - reset option to default
    // (1,0) - override to false
    // (1,1) - override to true

    // Reset required options to default
    options_t resetDefault = ~mask & value;
    options_overridden_mask &= resetDefault;
    options_overridden_value &= resetDefault;

    // Set required overridden mask
    options_overridden_mask |= mask;
    options_overridden_value = (options_overridden_value & ~mask) | (mask & value);

    // If changed, save
    if (options_overridden_mask != oldMask || options_overridden_value != oldValue) {
        WriteCues();
    }
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
    uint8_t headerBuffer[24];
    ret = fs.FileRead(&file_p, headerBuffer, sizeof(headerBuffer));
    if (ret != sizeof(headerBuffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    // Parse header
    bool headerValid = (headerBuffer[0] == 'C' && headerBuffer[1] == 'U' && headerBuffer[2] == 'E' && headerBuffer[3] == 'S');
    unsigned int fileVersion = headerBuffer[4] | (headerBuffer[5] << 8) | (headerBuffer[6] << 16) | (headerBuffer[7] << 24);
    unsigned int promptType = headerBuffer[8] | (headerBuffer[9] << 8) | (headerBuffer[10] << 16) | (headerBuffer[11] << 24);
    options_overridden_mask = headerBuffer[12] | (headerBuffer[13] << 8);
    options_overridden_value = headerBuffer[14] | (headerBuffer[15] << 8);

    if (version != NULL) {
        *version = headerBuffer[16] | (headerBuffer[17] << 8) | (headerBuffer[18] << 16) | (headerBuffer[19] << 24);
    }
    unsigned int promptCount = headerBuffer[20] | (headerBuffer[21] << 8) | (headerBuffer[22] << 16) | (headerBuffer[23] << 24);
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

    uint8_t headerBuffer[24];
    headerBuffer[0] = 'C'; headerBuffer[1] = 'U'; headerBuffer[2] = 'E'; headerBuffer[3] = 'S'; 
    headerBuffer[4] = (uint8_t)CUE_FILE_VERSION; headerBuffer[5] = (uint8_t)(CUE_FILE_VERSION >> 8); headerBuffer[6] = (uint8_t)(CUE_FILE_VERSION >> 16); headerBuffer[7] = (uint8_t)(CUE_FILE_VERSION >> 24); 
    headerBuffer[8] = (uint8_t)CUE_PROMPT_TYPE; headerBuffer[9] = (uint8_t)(CUE_PROMPT_TYPE >> 8); headerBuffer[10] = (uint8_t)(CUE_PROMPT_TYPE >> 16); headerBuffer[11] = (uint8_t)(CUE_PROMPT_TYPE >> 24); 
    headerBuffer[12] = (uint8_t)options_overridden_mask; headerBuffer[13] = (uint8_t)(options_overridden_mask >> 8);
    headerBuffer[14] = (uint8_t)options_overridden_value; headerBuffer[15] = (uint8_t)(options_overridden_value >> 8);
    headerBuffer[16] = (uint8_t)promptVersion; headerBuffer[17] = (uint8_t)(promptVersion >> 8); headerBuffer[18] = (uint8_t)(promptVersion >> 16); headerBuffer[19] = (uint8_t)(promptVersion >> 24); 
    headerBuffer[20] = (uint8_t)promptCount; headerBuffer[21] = (uint8_t)(promptCount >> 8); headerBuffer[22] = (uint8_t)(promptCount >> 16); headerBuffer[23] = (uint8_t)(promptCount >> 24); 
    
    // Open control points file for writing
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, CUE_DATA_FILENAME, LFS_O_WRONLY|LFS_O_CREAT|LFS_O_TRUNC|LFS_O_APPEND);
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

    // Notify that the cues were changed
    activityController.Event(ACTIVITY_EVENT_CUE_CONFIGURATION);

    return 0;
}

void CueController::SetInterval(unsigned int interval, unsigned int maximumRuntime) {
    // New configuration
    this->overrideEndTime = currentUptime + maximumRuntime;
    this->interval = interval;

    // Record this as the last prompt time so that the full interval must elapse
    lastPrompt = currentUptime;
}

void CueController::Reset() {
    store.Reset();
    WriteCues();
}

ControlPoint CueController::GetStoredControlPoint(int index) {
    return store.GetStored(index);
}

void CueController::ClearScratch() {
    store.ClearScratch();
}

void CueController::SetScratchControlPoint(int index, ControlPoint controlPoint) {
    store.SetScratch(index, controlPoint);
}

void CueController::CommitScratch(uint32_t version) {
    store.CommitScratch(version);
    WriteCues();
    descriptionValid = false;
}


void CueController::DebugText(char *debugText) {
  char *p = debugText;

  // File debug
  p += sprintf(p, "Fil: r%d w%d\n", readError, writeError);

  // Version
  p += sprintf(p, "Ver: %lu\n", (unsigned long)version);

  // Options
  p += sprintf(p, "Opt: ");
  options_t mask;
  options_t effectiveOptions = GetOptionsMaskValue(&mask, nullptr);
  for (int i = 7; i >= 0; i--) {
      char masked = (mask & (1 << i)) ? 1 : 0;
      char value = (effectiveOptions & (1 << i)) ? 1 : 0;
      char label = (masked ? "AESDZICX" : "aesdzicx")[i];
      p += sprintf(p, "%c%d", label, value);
  }
  p += sprintf(p, "\n");

  // Control points (stored and scratch)
  int countStored = 0, countScratch = 0;
  for (int i = 0; i < PROMPT_MAX_CONTROLS; i++) {
      countStored += ControlPoint(controlPoints[i]).IsEnabled();
      countScratch += ControlPoint(scratch[i]).IsEnabled();
  }
  p += sprintf(p, "S/S: %d %d /%d\n", countStored, countScratch, PROMPT_MAX_CONTROLS);

  // Current scheduled cue control point
  p += sprintf(p, "Cue: #%d %s d%02x\n", currentCueIndex, currentControlPoint.IsEnabled() ? "E" : "d", currentControlPoint.GetWeekdays());
  p += sprintf(p, " @%d i%d v%d\n", currentControlPoint.GetTimeOfDay(), currentControlPoint.GetInterval(), currentControlPoint.GetVolume());

  // Status
  uint16_t override_remaining;
  uint16_t intensity;
  uint16_t interval;
  uint16_t duration;
  GetStatus(nullptr, nullptr, nullptr, &override_remaining, &intensity, &interval, &duration);
  p += sprintf(p, "Ovr: t%d i%d v%d\n", override_remaining, interval, intensity);
  p += sprintf(p, "Rem: %d\n", duration);

  return;
}

const char *CueController::Description() {
    if (!descriptionValid) {
        char *p = description;
        *p = '\0';

        // Status
        uint32_t active_schedule_id;
        uint16_t max_control_points;
        uint16_t current_control_point;
        uint16_t override_remaining;
        uint16_t intensity;
        uint16_t interval;
        uint16_t duration;
        GetStatus(&active_schedule_id, &max_control_points, &current_control_point, &override_remaining, &intensity, &interval, &duration);

        if (override_remaining > 0) {
            if (interval > 0) {
                p += sprintf(p, "Manual (%dm)", override_remaining / 60);
            } else {
                p += sprintf(p, "Snooze (%dm)", override_remaining / 60);
            }
        } else if (current_control_point < 0xffff) {
            p += sprintf(p, "Cue (%dm)", duration / 60);
        } else if (duration <= 12 * 60) {
            p += sprintf(p, "Not Cueing (%dm)", duration / 60);
        } else {
            p += sprintf(p, "-");
        }
        descriptionValid = true;
    }
    return description;
}


Pinetime::Controllers::ControlPoint currentControlPoint;


#endif
