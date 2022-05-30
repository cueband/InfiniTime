#include "cueband.h"

#ifdef CUEBAND_CUE_ENABLED

#include "CueController.h"
#include "displayapp/screens/Symbols.h"
#include "components/battery/BatteryController.h"

using namespace Pinetime::Controllers;

#define CUE_DATA_FILENAME "CUES.BIN"
#define CUE_FILE_VERSION 1
#define CUE_FILE_MIN_VERSION 1
#define CUE_PROMPT_TYPE 0

CueController::CueController(Controllers::Settings& settingsController, 
                             Controllers::FS& fs,
                             Controllers::ActivityController& activityController,
                             Controllers::MotorController& motorController,
                             Controllers::Battery& batteryController
                            )
                            :
                            settingsController {settingsController},
                            fs {fs},
                            activityController {activityController},
                            motorController {motorController},
                            batteryController {batteryController}
                            {
    store.SetData(Pinetime::Controllers::ControlPointStore::VERSION_NONE, controlPoints, scratch, sizeof(controlPoints) / sizeof(controlPoints[0]));
    store.Reset();  // Clear all control points, version, and scratch.
    SetInterval(INTERVAL_OFF, MAXIMUM_RUNTIME_OFF);
}

void CueController::Vibrate(unsigned int style) {
#ifdef CUEBAND_MOTOR_PATTERNS
    motorController.RunIndex(style);
#else
    // Use as raw width unless matching a style number
    unsigned int motorPulseWidth = style;

    // TODO: Customize this range and/or add patterns
    switch (style % 8) { 
        case 0: motorPulseWidth = 50; break;
        case 1: motorPulseWidth = 75; break;
        case 2: motorPulseWidth = 100; break;
        case 3: motorPulseWidth = 150; break;
        case 4: motorPulseWidth = 200; break;
        case 5: motorPulseWidth = 300; break;
        case 6: motorPulseWidth = 400; break;
        case 7: motorPulseWidth = 500; break;
    }

    // Safety limit
    if (motorPulseWidth > 10 * 1000) motorPulseWidth = 10 * 1000;

    // Prompt
    motorController.RunForDuration(motorPulseWidth);   // milliseconds
#endif
}

bool CueController::SilencedAsUnworn() {
    bool silenced = false;
#ifdef CUEBAND_DETECT_FACE_DOWN
    if (activityController.IsFaceDown()) silenced = true;
#endif
    if (batteryController.IsPowerPresent()) silenced = true;
#ifdef CUEBAND_DETECT_WEAR_TIME
    if (activityController.IsUnmovingActivity()) silenced = true;
#endif
    return silenced;
}

// Called at 1Hz
void CueController::TimeChanged(uint32_t timestamp, uint32_t uptime) {
    bool snoozed = false;
    bool unworn = false;

    descriptionValid = false;

    currentTime = timestamp;
    currentUptime = uptime;

    unsigned int effectivePromptStyle = DEFAULT_PROMPT_STYLE;

    // Get scheduled interval (0=none)
    currentControlPoint = store.CueValue(timestamp, &currentCueIndex, &cueRemaining);
    unsigned int cueInterval = currentControlPoint.GetInterval();
    if (currentControlPoint.IsNonPrompting()) cueInterval = 0;

#ifdef CUEBAND_NO_SCHEDULED_PROMPTS_WHEN_UNSET_TIME
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

    // If not prompting when considered unworn...
#ifdef CUEBAND_SILENT_WHEN_UNWORN
    if (SilencedAsUnworn()) {
        snoozed = true;
        unworn = true;
    }
#endif

    // If we in a valid scheduled cue...
    if (effectiveInterval > 0) {
        // ...and not snoozing or manually overridden...
        if (currentUptime >= overrideEndTime) {
            // ...and we have passed within a new prompt control point...
            if (currentCueIndex != this->lastCueIndex) {
                this->lastCueIndex = currentCueIndex;
                // Check: if cueInterval/effectivePromptStyle are different, and store as last used values
                if (cueInterval > 0 && cueInterval != this->lastInterval) {
                    this->lastInterval = cueInterval;
                    DeferWriteCues();
                }
                if (effectivePromptStyle > 0 && effectivePromptStyle != this->promptStyle) {
                    this->promptStyle = effectivePromptStyle;
                    DeferWriteCues();
                }
            }
        }
    }

    // Track the current effective sheduled interval
    effectiveScheduledInterval = cueInterval;

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
        uint32_t elapsed = (lastPrompt != UPTIME_NONE) ? (uint32_t)((int32_t)currentUptime - (int32_t)lastPrompt) : UPTIME_NONE;
        bool prompt = (elapsed == UPTIME_NONE || elapsed >= effectiveInterval);
        if (prompt) {
            if (!snoozed) {
                Vibrate(effectivePromptStyle);

                // Notify activityController of prompt
                activityController.PromptGiven(false, false);
            } else {
                // Snoozed prompt
                activityController.PromptGiven(true, unworn);
            }

            // Record last prompt/snoozed-prompt
            lastPrompt = currentUptime;
        }
    }

    // Settings change debounce
    if (this->settingsChanged != 0) {
        if (++this->settingsChanged >= 10) {
            this->settingsChanged = 0;
            WriteCues();
        }
    }

}

void CueController::Init() {
    // Store currently contains the Reset() values (e.g. VERSION_NONE and no enabled cues)
    uint32_t version = store.GetVersion();
    // Flag as initialized (here so that ReadCues succeeds)
    initialized = true;
    // Read from file
    readError = ReadCues(&version);
    // Notify control points externally modified
    store.Updated(version);
    // Notify activity controller of current version
    activityController.PromptConfigurationChanged(store.GetVersion());
    descriptionValid = false;
    lastCueIndex = ControlPoint::INDEX_NONE;
}

void CueController::GetStatus(uint32_t *active_schedule_id, uint16_t *max_control_points, uint16_t *current_control_point, uint32_t *override_remaining, uint32_t *intensity, uint32_t *interval, uint32_t *duration) {
    bool scheduled = IsScheduled();

    if (active_schedule_id != nullptr) {
        *active_schedule_id = store.GetVersion();
    }
    if (max_control_points != nullptr) {
        *max_control_points = PROMPT_MAX_CONTROLS;
    }
    if (current_control_point != nullptr) {
        *current_control_point = (uint16_t)(initialized ? currentCueIndex : (uint16_t)-1);
    }
    unsigned int remaining = (initialized ? (scheduled ? 0 : (overrideEndTime - currentUptime)) : 0);
    if (override_remaining != nullptr) {
        *override_remaining = remaining;
    }
    if (intensity != nullptr) {
        if (!initialized) {
            *intensity = 0;
        } else if (scheduled) {
            *intensity = (uint16_t)currentControlPoint.GetVolume();
        } else {
            *intensity = promptStyle;
        }
    }
    if (interval != nullptr) {
        if (!initialized) {
            *interval = 0;
        } else if (scheduled) {
            *interval = (uint16_t)currentControlPoint.GetInterval();
        } else {
            *interval = this->interval;
        }
    }
    if (duration != nullptr) {
        if (!initialized) {
            *duration = 0;
        } else {
            *duration = cueRemaining;
        }
    }
}

void CueController::GetLastImpromptu(unsigned int *lastInterval, unsigned int *promptStyle) {
    if (lastInterval != nullptr) {
        if (this->lastInterval == 0) *lastInterval = DEFAULT_INTERVAL;
        else { *lastInterval = this->lastInterval; }
    }
    if (promptStyle != nullptr) *promptStyle = this->promptStyle;
}


options_t CueController::GetOptionsMaskValue(options_t *base, options_t *mask, options_t *value) {
    // overridden_mask:  0=default, 1=remote-set
    // overridden_value: 0=off, 1=on
    options_t effectiveValue = (options_base_value & ~options_overridden_mask) | (options_overridden_mask & options_overridden_value);

    if (base != nullptr) *base = options_base_value;
    if (mask != nullptr) *mask = options_overridden_mask;
    if (value != nullptr) *value = options_overridden_value;

    return effectiveValue;
}

bool CueController::SetOptionsMaskValue(options_t mask, options_t value) {
    if (!initialized) return false;

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
        descriptionValid = false;
        DeferWriteCues();
    }

    return true;
}

bool CueController::SetOptionsBaseValue(options_t new_base_value) {
    if (!initialized) return false;
    if (this->options_base_value != new_base_value) {
        this->options_base_value = new_base_value;
        descriptionValid = false;
        DeferWriteCues();
    }
    return true;
}

int CueController::ReadCues(uint32_t *version) {
    int ret;
    if (!initialized) return 9;
    this->settingsChanged = 0;

    // Now used default options (instead of startup options)
    options_base_value = OPTIONS_DEFAULT;

    // Restore previous control points
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, CUE_DATA_FILENAME, LFS_O_RDONLY);
    if (ret == LFS_ERR_CORRUPT) fs.FileDelete(CUE_DATA_FILENAME);    // No other sensible action?
    if (ret != LFS_ERR_OK) {
        return 1;
    }

    // Read header
    uint8_t headerBuffer[32];
    ret = fs.FileRead(&file_p, headerBuffer, sizeof(headerBuffer));
    if (ret != sizeof(headerBuffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    // Parse header
    bool headerValid = (headerBuffer[0] == 'C' && headerBuffer[1] == 'U' && headerBuffer[2] == 'E' && headerBuffer[3] == 'S');
    unsigned int fileVersion = headerBuffer[4] | (headerBuffer[5] << 8) | (headerBuffer[6] << 16) | (headerBuffer[7] << 24);
    if (!headerValid || fileVersion < CUE_FILE_MIN_VERSION || fileVersion > CUE_FILE_VERSION) {
        fs.FileClose(&file_p);
        return 3;
    }

    // Options
    options_base_value = headerBuffer[8] | (headerBuffer[9] << 8);
    options_overridden_mask = headerBuffer[10] | (headerBuffer[11] << 8);
    options_overridden_value = headerBuffer[12] | (headerBuffer[13] << 8);

    // Reserved (duration?)
    //uint16_t reserved = headerBuffer[14] | (headerBuffer[15] << 8);

    // Last impromptu settings
    this->lastInterval = headerBuffer[16] | (headerBuffer[17] << 8);
    if (this->lastInterval == 0 || this->lastInterval >= 0xffff) this->lastInterval = DEFAULT_INTERVAL;
    this->promptStyle = headerBuffer[18] | (headerBuffer[19] << 8);
    if (this->promptStyle >= 0xffff) this->promptStyle = DEFAULT_PROMPT_STYLE;

    // Prompt details
    unsigned int promptType  = headerBuffer[20] | (headerBuffer[21] << 8) | (headerBuffer[22] << 16) | (headerBuffer[23] << 24);
    uint32_t promptVersion   = headerBuffer[24] | (headerBuffer[25] << 8) | (headerBuffer[26] << 16) | (headerBuffer[27] << 24);
    unsigned int promptCount = headerBuffer[28] | (headerBuffer[29] << 8) | (headerBuffer[30] << 16) | (headerBuffer[31] << 24);
    if (promptType != CUE_PROMPT_TYPE) {
        fs.FileClose(&file_p);
        return 4;
    }
    if (version != NULL) *version = promptVersion;

    // Read prompts
    unsigned int maxControls = sizeof(controlPoints) / sizeof(controlPoints[0]);
    unsigned int count = (promptCount > maxControls) ? maxControls : promptCount;
    ret = fs.FileRead(&file_p, (uint8_t *)&controlPoints[0], count * sizeof(controlPoints[0]));

    if (ret != (int)(count * sizeof(controlPoints[0]))) {
        fs.FileClose(&file_p);
        return 5;
    }

    // Position after cue values
    unsigned int pos = promptCount * sizeof(controlPoints[0]) + sizeof(headerBuffer);
    ret = fs.FileSeek(&file_p, pos);
    if (ret != (int)pos) {
        fs.FileClose(&file_p);
        return 6;
    }

    fs.FileClose(&file_p);
    return 0;
}

void CueController::DeferWriteCues() {
    if (this->settingsChanged == 0) {
        this->settingsChanged = 1;
    }
}

int CueController::WriteCues() {
    int ret;
    if (!initialized) return 9;
    this->settingsChanged = 0;

    int promptCount = sizeof(controlPoints) / sizeof(controlPoints[0]);
    uint32_t promptVersion = store.GetVersion();

    uint8_t headerBuffer[32];
    headerBuffer[0] = 'C'; headerBuffer[1] = 'U'; headerBuffer[2] = 'E'; headerBuffer[3] = 'S'; 
    headerBuffer[4] = (uint8_t)CUE_FILE_VERSION; headerBuffer[5] = (uint8_t)(CUE_FILE_VERSION >> 8); headerBuffer[6] = (uint8_t)(CUE_FILE_VERSION >> 16); headerBuffer[7] = (uint8_t)(CUE_FILE_VERSION >> 24); 
    headerBuffer[8] = (uint8_t)options_base_value; headerBuffer[9] = (uint8_t)(options_base_value >> 8);
    headerBuffer[10] = (uint8_t)options_overridden_mask; headerBuffer[11] = (uint8_t)(options_overridden_mask >> 8);
    headerBuffer[12] = (uint8_t)options_overridden_value; headerBuffer[13] = (uint8_t)(options_overridden_value >> 8);
    headerBuffer[14] = 0; headerBuffer[15] = 0; // reserverd
    headerBuffer[16] = (uint8_t)this->lastInterval; headerBuffer[17] = (uint8_t)(this->lastInterval >> 8);
    headerBuffer[18] = (uint8_t)this->promptStyle; headerBuffer[19] = (uint8_t)(this->promptStyle >> 8);
    headerBuffer[20] = (uint8_t)CUE_PROMPT_TYPE; headerBuffer[21] = (uint8_t)(CUE_PROMPT_TYPE >> 8); headerBuffer[22] = (uint8_t)(CUE_PROMPT_TYPE >> 16); headerBuffer[23] = (uint8_t)(CUE_PROMPT_TYPE >> 24); 
    headerBuffer[24] = (uint8_t)promptVersion; headerBuffer[25] = (uint8_t)(promptVersion >> 8); headerBuffer[26] = (uint8_t)(promptVersion >> 16); headerBuffer[27] = (uint8_t)(promptVersion >> 24); 
    headerBuffer[28] = (uint8_t)promptCount; headerBuffer[29] = (uint8_t)(promptCount >> 8); headerBuffer[30] = (uint8_t)(promptCount >> 16); headerBuffer[31] = (uint8_t)(promptCount >> 24); 
    
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

void CueController::SetPromptStyle(unsigned int promptStyle) {
    if (promptStyle < 0xffff) {
        if (promptStyle != this->promptStyle) {
            this->promptStyle = promptStyle;
            DeferWriteCues();
        }

        // Now wait for a whole prompt cycle before prompting again
        lastPrompt = currentUptime;
    }
}

bool CueController::SetInterval(unsigned int interval, unsigned int maximumRuntime) {
    if (!initialized) return false;

    // New configuration
    if (maximumRuntime != (unsigned int)-1) this->overrideEndTime = currentUptime + maximumRuntime;

    // If not snoozing...
    if (interval > 0) {

        // Reset last interval if its ever invalid
        if (this->lastInterval == 0 && this->lastInterval != DEFAULT_INTERVAL) {
            this->lastInterval = DEFAULT_INTERVAL;
            DeferWriteCues();
        }

        // If not keeping default interval, and interval has changed...
        if (interval != (unsigned int)-1 && interval != this->lastInterval) {
            this->lastInterval = interval;
            DeferWriteCues();
        }

        // Impromptu interval
        this->interval = this->lastInterval;
    } else {
        // Snoozing
        this->interval = 0;
    }

    // When setting manual prompt details...
    if (interval != (unsigned int)-1 || maximumRuntime != (unsigned int)-1) {
        // Set as just before the next prompt time so that only a couple of seconds will elapse before prompting
        const int delay = 2;
        lastPrompt = (uint32_t)((int32_t)currentUptime - (int32_t)interval - delay);        // UPTIME_NONE
    }
    
    descriptionValid = false;
    return true;
}

void CueController::Reset(bool everything) {
    // Reset store
    store.Reset();
    // Reset full cue state
    if (everything) {
        options_base_value = OPTIONS_DEFAULT;
        options_overridden_mask = 0;
        options_overridden_value = 0;
        lastInterval = DEFAULT_INTERVAL;
        promptStyle = DEFAULT_PROMPT_STYLE;
    }
    lastCueIndex = ControlPoint::INDEX_NONE;
    // Store
    descriptionValid = false;
    DeferWriteCues();
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
    DeferWriteCues();
    lastCueIndex = ControlPoint::INDEX_NONE;
    descriptionValid = false;
}


void CueController::DebugText(char *debugText) {
  char *p = debugText;

  // File debug
  p += sprintf(p, "Fil: %s%d r%d w%d\n", initialized ? "i" : "I", this->settingsChanged, readError, writeError);

  // Version
  p += sprintf(p, "Ver: %lu\n", (unsigned long)version);

  // Options
  p += sprintf(p, "Opt: ");
  options_t base;
  options_t mask;
  options_t effectiveOptions = GetOptionsMaskValue(&base, &mask, nullptr);
  for (int i = 6; i >= 0; i--) {
      char original = (base & (1 << i)) ? 1 : 0;
      char masked = (mask & (1 << i)) ? 1 : 0;
      char effective = (effectiveOptions & (1 << i)) ? 1 : 0;
      char label = (original ? "AESDZICX" : "aesdzicx")[i]; // case of flag indicates base value "unset"/"SET"
      char value = (masked ? "-+" : "01")[(int)effective];  // base value is '0'/'1', overridden high '+', overridden low '-'
      p += sprintf(p, "%c%c", label, value);
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
  p += sprintf(p, "Cue: ##%d %s d%02x\n", currentCueIndex, currentControlPoint.IsEnabled() ? (currentControlPoint.IsNonPrompting() ? "n" : "p") : "d", currentControlPoint.GetWeekdays());
  p += sprintf(p, " @%d i%d v%d\n", currentControlPoint.GetTimeOfDay(), currentControlPoint.GetInterval(), currentControlPoint.GetVolume());

  // Status
  //uint32_t active_schedule_id;
  //uint16_t max_control_points;
  //uint16_t current_control_point;
  uint32_t override_remaining;
  uint32_t intensity;
  uint32_t interval;
  uint32_t duration;
  GetStatus(nullptr, nullptr, nullptr, &override_remaining, &intensity, &interval, &duration);
  p += sprintf(p, "Ovr: t%d i%d v%d\n", (int)override_remaining, (int)interval, (int)intensity);
  p += sprintf(p, "Rem: %d\n", (int)duration);

  return;
}

// Return a static buffer (must be used immediately and not called again before use)
const char *niceTime(unsigned int seconds) {
    static char buffer[12];
    if (seconds == 0) {
        sprintf(buffer, "now");
    } if (seconds < 10) {
        sprintf(buffer, "shortly");
    } if (seconds < 60) {
        sprintf(buffer, "%is", (int)((seconds + 2) / 5) * 5);
    } else if (seconds < 90 * 60) {
        sprintf(buffer, "%im", (int)((seconds + 30) / 60));
    } else if (seconds < 24 * 60 * 60) {
        sprintf(buffer, "%ih", (int)(seconds / 60 / 60));
    } else if (seconds < 0xffffffff) {
        sprintf(buffer, "%id", (int)(seconds / 60 / 60 / 24));
    } else {
        sprintf(buffer, "-");
    }
    return buffer;
}


// Status Description
const char *CueController::Description(bool detailed, const char **symbol) {
    // #ifdef CUEBAND_SYMBOLS
    // cueband_20.c & cueband_48.c
    // static constexpr const char* cuebandCue       = "\xEF\xA0\xBE";                  // 0xf83e, wave-square
    // static constexpr const char* cuebandIsCueing  = "\xEF\x89\xB4";                  // 0xf274, calendar-check
    // static constexpr const char* cuebandNotCueing = "\xEF\x89\xB2";                  // 0xf272, calendar-minus
    // static constexpr const char* cuebandScheduled = "\xEF\x81\xB3";                  // 0xf073, calendar-alt
    // static constexpr const char* cuebandSilence   = "\xEF\x81\x8C";                  // 0xf04c, pause
    // static constexpr const char* cuebandImpromptu = "\xEF\x81\x8B";                  // 0xf04b, play

    if (!descriptionValid || descriptionDetailed != detailed) {
        icon = Applications::Screens::Symbols::cuebandCue;
        char *p = description;
        *p = '\0';

        // Status
        uint32_t active_schedule_id;
        uint16_t max_control_points;
        uint16_t current_control_point;
        uint32_t override_remaining;
        uint32_t intensity;
        uint32_t interval;
        uint32_t duration;
        GetStatus(&active_schedule_id, &max_control_points, &current_control_point, &override_remaining, &intensity, &interval, &duration);

        if (!IsEnabled()) {
            // (leave empty)
        } else if (!initialized) {
            p += sprintf(p, "Initializing...");
        } else if (override_remaining > 0) {
            if (interval > 0) {
                // Temporary cueing
#ifdef CUEBAND_SILENT_WHEN_UNWORN
                if (SilencedAsUnworn()) {
                    p += sprintf(p, "Unworn Man.(%s)", niceTime(override_remaining));
                } else
#endif 
                {
                    p += sprintf(p, "Manual (%s)", niceTime(override_remaining));
                }
                icon = Applications::Screens::Symbols::cuebandImpromptu;
                if (detailed) {
                    p += sprintf(p, "\n[interval %i%s]", (int)(interval < 100 ? interval : (interval / 60)), interval < 100 ? "s" : "m");
                }
            } else {
                // Temporary snooze
                p += sprintf(p, "Mute (%s)", niceTime(override_remaining));
                icon = Applications::Screens::Symbols::cuebandSilence;
            }
        } else if (!IsEnabled()) {
            // Scheduled cueing Disabled
            //p += sprintf(p, ".");
            //icon = Applications::Screens::Symbols::cuebandDisabled;
        } else if (current_control_point < 0xffff && intensity > 0 && interval > 0)  { // !currentControlPoint.IsNonPrompting()
            // Scheduled cueing in progress
#ifdef CUEBAND_SILENT_WHEN_UNWORN
            if (SilencedAsUnworn()) {
                p += sprintf(p, "Unworn Cue (%s)", niceTime(duration));
            } else
#endif 
            {
                p += sprintf(p, "Cue (%s)", niceTime(duration));
            }
            if (detailed) {
                p += sprintf(p, "\n[interval %i%s]", (int)(interval < 100 ? interval : (interval / 60)), interval < 100 ? "s" : "m");
            }
            icon = Applications::Screens::Symbols::cuebandIsCueing;
        } else if (duration <= (7 * 24 * 60 * 60)) {
            // Scheduled cueing
            p += sprintf(p, "Not Cueing (%s)", niceTime(duration));
            icon = Applications::Screens::Symbols::cuebandNotCueing;
        } else {
            // No scheduled cueing
            p += sprintf(p, "None Scheduled");
            icon = Applications::Screens::Symbols::cuebandScheduled;
        }
        descriptionDetailed = detailed;
        descriptionValid = true;
    }

    if (symbol != nullptr) {
        *symbol = icon;
    }

    return description;
}


Pinetime::Controllers::ControlPoint currentControlPoint;


#endif
