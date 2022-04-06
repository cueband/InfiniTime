// Local testing build configuration without varying the global config

// Custom device name to distinguish the local build
#define CUEBAND_LOCAL_DEVICE_NAME "InfiniTime-######"  // "InfiniTime"

// Local configuration to specify the key required for a trusted connection
//#define CUEBAND_LOCAL_KEY ""

// Do not require trusted connections for the local build
#ifdef CUEBAND_USE_TRUSTED_CONNECTION
    #undef CUEBAND_USE_TRUSTED_CONNECTION
#endif

// Show basic apps (stopwatch, timer, alarm)
#ifdef CUEBAND_CUSTOMIZATION_NO_OTHER_APPS
    #undef CUEBAND_CUSTOMIZATION_NO_OTHER_APPS
#endif

// Test invalid time (not just resetting it to build time)
#ifdef CUEBAND_DEBUG_INIT_TIME
    #undef CUEBAND_DEBUG_INIT_TIME
#endif

// Test the compilation of (close to) upstream firmware
//#undef CUEBAND_ACTIVITY_ENABLED    // Disable activity monitoring service etc.
//#undef CUEBAND_CUE_ENABLED         // Disable cue prompts service etc.
