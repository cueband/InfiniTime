// cueband.h - Central place for cue.band configuration
// Dan Jackson, 2021

#pragma once

#define CUEBAND_FIX_WARNINGS            // Ignore warnings in original InfiniTime code (without modifying that code)

#define CUEBAND_VERSION "1"             // User-visible string
#define CUEBAND_VERSION_NUMBER 0x01     // 1-byte public firmware release serial number
#define CUEBAND_APPLICATION_TYPE 0x0002

#define CUEBAND_DEVICE_NAME "InfiniTime-######"  // "InfiniTime"
#define CUEBAND_SERIAL_ADDRESS

// Simple UART service
// See: src/components/ble/UartService.cpp
// See: src/components/ble/UartService.h
// See: src/components/ble/NimbleController.h
#define CUEBAND_SERVICE_UART_ENABLED

// Streaming accelerometer samples over UART
// See: src/components/ble/UartService.cpp
// See: src/components/ble/UartService.h
// See: src/systemtask/SystemTask.cpp
#define CUEBAND_STREAM_ENABLED

// Stream the resampled data
//#define CUEBAND_STREAM_RESAMPLED


// (Placeholder) App screen
// See: src/displayapp/Apps.h
// See: src/displayapp/screens/ApplicationList.cpp
// See: src/displayapp/DisplayApp.cpp
// See: displayapp/screens/CueBandApp.h
// See: displayapp/screens/CueBandApp.cpp
#define CUEBAND_APP_ENABLED

// Activity monitoring
#define CUEBAND_ACTIVITY_ENABLED

// Cue prompts
#define CUEBAND_CUE_ENABLED

#define CUEBAND_AXES 3          // Must be 3


// Various customizations for the UI and existing PineTime services
#define CUEBAND_CUSTOMIZATION

#ifdef CUEBAND_CUSTOMIZATION

    // See: src/displayapp/screens/WatchFaceDigital.cpp
    #define CUEBAND_CUSTOMIZATION_NO_HR
    #define CUEBAND_CUSTOMIZATION_NO_STEPS

    // See: src/displayapp/screens/ApplicationList.cpp
    #define CUEBAND_CUSTOMIZATION_ONLY_ESSENTIAL_APPS

    // See: src/components/ble/NimbleController.cpp
    #define CUEBAND_SERVICE_MUSIC_DISABLED
    #define CUEBAND_SERVICE_NAV_DISABLED
    #define CUEBAND_SERVICE_HR_DISABLED

#endif


#if defined(CUEBAND_STREAM_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)

    // Extend accelerometer driver (Bma421.cpp) and Motion Controller (MotionController.cpp) to buffer multiple samples
    #define CUEBAND_BUFFER_ENABLED

    #define CUEBAND_STREAM_DECIMATE 1       // Only stream 1 in N (1=50 Hz, 2=25 Hz)

    #define CUEBAND_FAST_LOOP_IF_REQUIRED

    #define CUEBAND_BUFFER_SAMPLE_RATE 50     // 50 Hz
    #define CUEBAND_BUFFER_SAMPLE_RANGE 8     // +/- 8g
    #define CUEBAND_BUFFER_12BIT_SCALE (2048/CUEBAND_BUFFER_SAMPLE_RANGE)    // at +/- 8g, 256 (12-bit) raw units = 1 'g'
    #define CUEBAND_BUFFER_16BIT_SCALE (CUEBAND_BUFFER_12BIT_SCALE << 4)     // at +/- 8g, 4096

    // Original firmware expects the MotionController to return values scaled at +/- 2g -- see Bma421::Init()
    #define CUEBAND_ORIGINAL_RANGE 2                                // +/- 2g
    #define CUEBAND_ORIGINAL_SCALE (2048/CUEBAND_ORIGINAL_RANGE)    // at +/- 2g, 1024 (12-bit) raw units = 1 'g'

    // Use the FIFO
    #define CUEBAND_FIFO_ENABLED

    #ifdef CUEBAND_FIFO_ENABLED
        // FIFO settings
        #define CUEBAND_FIFO_POLL_RATE 10        // 10 Hz -- Read FIFO approximately this rate (plus jitter) -- TODO: Consider using watermark interrupt instead

        #define CUEBAND_SAMPLE_MAX 32  // (120 * CUEBAND_BUFFER_SAMPLE_RATE / CUEBAND_FIFO_POLL_RATE / 100) // 60 samples (allow 20% margin on rate)

        #define CUEBAND_FIFO_BUFFER_LENGTH (CUEBAND_SAMPLE_MAX * 6)   // FIFO: 360 bytes (up to 1024)

        #define CUEBAND_BUFFER_EFFECTIVE_RATE CUEBAND_BUFFER_SAMPLE_RATE
    #else
        // If not using FIFO, fall back to (rubbish) polled sampling
        #define CUEBAND_POLLED_ENABLED
        #define CUEBAND_POLLED_INPUT_RATE 8     // 8 Hz
        #define CUEBAND_POLLED_OUTPUT_RATE 32   // 30/32 Hz (-> 32 Hz as x4 integer scaling for fakely synthesized nearest-neighbour sampling from polled input rate)

        #define CUEBAND_BUFFER_EFFECTIVE_RATE CUEBAND_POLLED_OUTPUT_RATE

        #define CUEBAND_SAMPLE_MAX 1
    #endif

#endif


// Don't bother reading activity and temperature as they're not (currently) used
#ifdef CUEBAND_BUFFER_ENABLED
    #define CUEBAND_DONT_READ_UNUSED_ACCELEROMETER_VALUES
#endif

// Don't allow the SPI-NOR flash or SPI bus to sleep (TODO: Coordinate this with ActivityController to write to files)
#ifdef CUEBAND_ACTIVITY_ENABLED
    #define CUEBAND_DONT_SLEEP_SPI
    #define CUEBAND_DONT_SLEEP_NOR_FLASH
#endif


// If none of these are defined, the build should be identical to stock InfiniTime firmware
#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED) || defined(CUEBAND_CUE_ENABLED)

    // Modifications to version
    // See: src/components/ble/DeviceInformationService.h
    #define CUEBAND_VERSION_MANUFACTURER_EXTENSION "/cb" CUEBAND_VERSION
    #define CUEBAND_VERSION_MODEL_EXTENSION "/cb" CUEBAND_VERSION
    #define CUEBAND_VERSION_SOFTWARE_EXTENSION "/cb" CUEBAND_VERSION

    // Modifications to Firmware App screen
    // See: src/displayapp/screens/FirmwareValidation.cpp
    #define CUEBAND_INFO_FIRMWARE "\ncue.band/" CUEBAND_VERSION

    // Modifications to System Info App screen
    // See: src/displayapp/screens/SystemInfo.cpp
    #define CUEBAND_INFO_SYSTEM "\ncue.band/" CUEBAND_VERSION

#endif

#if defined(CUEBAND_ACTIVITY_ENABLED)
    #define CUEBAND_FS_FILESIZE_ENABLED
    #define CUEBAND_FS_FILETELL_ENABLED
#endif

#define CUEBAND_POSSIBLE_FIX_FS         // The value in FS.h for `size` looks incorrect?

#define CUEBAND_ACTIVITY_EPOCH_INTERVAL 60 // 60
#define ACTIVITY_BLOCK_SIZE 256
#define ACTIVITY_MAXIMUM_BLOCKS 512  // 512 = 128 kB, ~10 days;  

//#define CUEBAND_DEBUG_DUMMY_MISSING_BLOCKS


// Config compatibility checks
#if defined(CUEBAND_FIFO_ENABLED) && !defined(CUEBAND_BUFFER_ENABLED)
    #error "CUEBAND_FIFO_ENABLED requires CUEBAND_BUFFER_ENABLED"
#endif
#if defined(CUEBAND_STREAM_ENABLED) && !defined(CUEBAND_POLLED_ENABLED) && !defined(CUEBAND_FIFO_ENABLED)
    #error "CUEBAND_STREAM_ENABLED requires CUEBAND_FIFO_ENABLED or CUEBAND_POLLED_ENABLE"
#endif
#if defined(CUEBAND_ACTIVITY_ENABLED) && !defined(CUEBAND_POLLED_ENABLED) && !defined(CUEBAND_FIFO_ENABLED)
    #error "CUEBAND_ACTIVITY_ENABLED requires CUEBAND_FIFO_ENABLED or CUEBAND_POLLED_ENABLE"
#endif
#if defined(CUEBAND_CUE_ENABLED) && !defined(CUEBAND_ACTIVITY_ENABLED)
    #error "CUEBAND_CUE_ENABLED requires CUEBAND_ACTIVITY_ENABLED (for wear time and to store log)"
#endif
#if defined(CUEBAND_STREAM_ENABLED) && !defined(CUEBAND_SERVICE_UART_ENABLED)
    #error "CUEBAND_STREAM_ENABLED requires CUEBAND_SERVICE_UART_ENABLED"
#endif
#if defined(CUEBAND_POLLED_ENABLED) && defined(CUEBAND_FIFO_ENABLED)
    #error "At most one of CUEBAND_POLLED_ENABLED / CUEBAND_FIFO_ENABLED may be defined"
#endif

// Warnings for non-standard build
#if defined(CUEBAND_CONFIGURATION_WARNINGS) // Only warn during compilation of main.cpp
#if !defined(CUEBAND_FIFO_ENABLED)
    #warning "CUEBAND_FIFO_ENABLED not defined - won't use sensor FIFO"
#endif
#if (ACTIVITY_BLOCK_SIZE != 256)
    #warning "ACTIVITY_BLOCK_SIZE non-standard value (usually 256)"
#endif
#if !defined(CUEBAND_SERVICE_UART_ENABLED)
    #warning "CUEBAND_SERVICE_UART_ENABLED not defined - no UART service."
#endif
#if !defined(CUEBAND_STREAM_ENABLED)
    #warning "CUEBAND_STREAM_ENABLED not defined - no streaming supported."
#endif
#if !defined(CUEBAND_APP_ENABLED)
    #warning "CUEBAND_APP_ENABLED not defined - no app."
#endif
#if !defined(CUEBAND_ACTIVITY_ENABLED)
    #warning "CUEBAND_ACTIVITY_ENABLED not defined - no activity monitoring."
#endif
#if !defined(CUEBAND_CUE_ENABLED)
    #warning "CUEBAND_CUE_ENABLED not defined - no cueing."
#endif
#if defined(CUEBAND_ACTIVITY_ENABLED) && (CUEBAND_ACTIVITY_EPOCH_INTERVAL != 60)
    #warning "CUEBAND_ACTIVITY_EPOCH_INTERVAL != 60 - non-standard epoch size."
#endif
#if (ACTIVITY_BLOCK_SIZE != 256)
    #warning "ACTIVITY_BLOCK_SIZE non-standard value (usually 256)"
#endif
#if (ACTIVITY_MAXIMUM_BLOCKS != 512)
    #warning "ACTIVITY_MAXIMUM_BLOCKS non-standard value (usually 512)"     // approx. 10 days when 28 minutes per block (256-byte blocks, 8-bytes per sample, 60 second epoch)
#endif
#if defined(CUEBAND_DEBUG_DUMMY_MISSING_BLOCKS)
    #warning "CUEBAND_DEBUG_DUMMY_MISSING_BLOCKS should not normally be defined"
#endif
#endif

// SystemTask.cpp:
//   uint32_t systick_counter = nrf_rtc_counter_get(portNRF_RTC_REG);