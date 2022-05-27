// Local testing build configuration without varying the global config

// While debugging, use the build time to initialize the clock whenever the device time is invalid
//#define CUEBAND_DEBUG_INIT_TIME

// Custom device name to distinguish the local build
#define CUEBAND_LOCAL_DEVICE_NAME "InfiniTime-######"  // "InfiniTime"

// Local configuration to specify the key required for a trusted connection
//#define CUEBAND_LOCAL_KEY ""

// Do not require trusted connections for the local build
#ifdef CUEBAND_USE_TRUSTED_CONNECTION
    #undef CUEBAND_USE_TRUSTED_CONNECTION
#endif

// Test: Make it trickier to accidentally wipe the firmware by holding the button while worn (risky)
#ifndef CUEBAND_PREVENT_ACCIDENTAL_RECOVERY_MODE
    #define CUEBAND_PREVENT_ACCIDENTAL_RECOVERY_MODE
#endif

// Test: Compilation of (close to) upstream firmware
//#undef CUEBAND_ACTIVITY_ENABLED    // Disable activity monitoring service etc.
//#undef CUEBAND_CUE_ENABLED         // Disable cue prompts service etc.

// Test: custom info org on local builds
#define CUEBAND_GITHUB_ORG "InfiniTimeOrg"

// Test: toggle specific other apps
#ifdef CUEBAND_METRONOME_ENABLED
  #undef CUEBAND_METRONOME_ENABLED
#endif
