#pragma once

#include "cueband.h"
#include "resampler.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#define ACTIVITY_CONFIG_DEFAULT 0xffff

#include "components/settings/Settings.h"
#include "components/fs/FS.h"
#ifdef CUEBAND_TRACK_MOTOR_TIMES
      #include "components/datetime/DateTimeController.h"
      #include "components/motor/MotorController.h"
#endif

#if defined(CUEBAND_BUFFER_ENABLED)
#include "components/motion/MotionController.h"
#endif

#ifdef CUEBAND_HR_EPOCH
#include "components/heartrate/HeartRateController.h"
#endif

#include <array>
#include <cstdint>

#define ACTIVITY_BLOCK_INVALID 0xffffffff

#define ACTIVITY_EVENT_POWER_CONNECTED     0x0001 // Connected to power for at least part of the epoch
#define ACTIVITY_EVENT_POWER_CHANGED       0x0002 // Power connection status changed during the epoch
#define ACTIVITY_EVENT_BLUETOOTH_CONNECTED 0x0004 // Connected to Bluetooth for at least part the epoch
#define ACTIVITY_EVENT_BLUETOOTH_CHANGED   0x0008 // Bluetooth connection status changed during the epoch
#define ACTIVITY_EVENT_BLUETOOTH_COMMS     0x0010 // Communication protocol activity
#define ACTIVITY_EVENT_WATCH_AWAKE         0x0020 // Watch was awoken at least once during the epoch
#define ACTIVITY_EVENT_WATCH_INTERACTION   0x0040 // Watch screen interaction (button or touch)
#define ACTIVITY_EVENT_RESTART             0x0080 // First epoch after device restart (or event logging restarted?)
#define ACTIVITY_EVENT_NOT_WORN            0x0100 // Activity: Device considered not worn
#define ACTIVITY_EVENT_ASLEEP              0x0200 // Activity: Wearer considered asleep
#define ACTIVITY_EVENT_CUE_DISABLED        0x0400 // Cue: scheduled cueing disabled
#define ACTIVITY_EVENT_CUE_CONFIGURATION   0x0800 // Cue: new configuration written
#define ACTIVITY_EVENT_CUE_OPENED          0x1000 // Cue: user opened app
#define ACTIVITY_EVENT_CUE_MANUAL          0x2000 // Cue: temporary manual cueing in use
#define ACTIVITY_EVENT_CUE_SNOOZE          0x4000 // Cue: temporary manual snooze in use
#define ACTIVITY_EVENT_FACE_DOWN           0x8000 // Activity: Watch was detected as face-down during the epoch 

#define ACTIVITY_HEADER_SIZE 30
#define ACTIVITY_PAYLOAD_SIZE (ACTIVITY_BLOCK_SIZE - ACTIVITY_HEADER_SIZE - 2) // 480 (512 - 30 bytes header - 2 bytes checksum)
#define ACTIVITY_SAMPLE_SIZE 8

#ifdef CUEBAND_MAXIMUM_SAMPLES_PER_BLOCK
      // Overridden for debug purposes
      #define ACTIVITY_MAX_SAMPLES (((ACTIVITY_PAYLOAD_SIZE / ACTIVITY_SAMPLE_SIZE) < (CUEBAND_MAXIMUM_SAMPLES_PER_BLOCK)) ? (ACTIVITY_PAYLOAD_SIZE / ACTIVITY_SAMPLE_SIZE) : (CUEBAND_MAXIMUM_SAMPLES_PER_BLOCK))
#else
      #define ACTIVITY_MAX_SAMPLES (ACTIVITY_PAYLOAD_SIZE / ACTIVITY_SAMPLE_SIZE) // 60 (480 / 8)
#endif

// If streaming resampled data, the buffer needs to hold a whole resampled FIFOs worth -- otherwise the chunks can be small
#ifdef CUEBAND_STREAM_RESAMPLED
  #define ACTIVITY_RESAMPLE_BUFFER_SIZE   ((CUEBAND_SAMPLE_MAX * ACTIVITY_RATE + CUEBAND_BUFFER_EFFECTIVE_RATE - 1) / CUEBAND_BUFFER_EFFECTIVE_RATE + 2)
#else
  #define ACTIVITY_RESAMPLE_BUFFER_SIZE   8
#endif

// Per-file metadata
struct ActivityMeta {
      uint32_t blockCount;
      uint32_t lastLogicalBlock;
      int err;                      // diagnostic
};



namespace Pinetime {
  namespace Controllers {
    class ActivityController {
    public:
      ActivityController(Pinetime::Controllers::Settings& settingsController, Pinetime::Controllers::FS& fs
#ifdef CUEBAND_TRACK_MOTOR_TIMES
            , Pinetime::Controllers::DateTime& dateTimeController
            , Pinetime::Controllers::MotorController& motorController
#endif
#ifdef CUEBAND_HR_EPOCH
            , Pinetime::Controllers::HeartRateController& heartRateController
#endif
      );

#if defined(CUEBAND_BUFFER_ENABLED)
      // Add multiple samples at the original source rate
      void AddSamples(MotionController &motionController);
#endif
      // Add single sample at (ACTIVITY_RATE) Hz
      void AddSingleSample(int16_t x, int16_t y, int16_t z);
      void TimeChanged(uint32_t time);

      void Init(uint32_t time, std::array<uint8_t, 6> deviceAddress, uint8_t accelerometerInfo);
      void DestroyData();

      bool IsInitialized() { return isInitialized; }
      void DebugText(char *debugText, bool additionalInfo);  // requires ~200 byte buffer
      void DebugTextConfig(char *debugText);

      // Get range of blocks available, read
      uint32_t BlockCount();
      uint32_t EarliestLogicalBlock();
      uint32_t ActiveLogicalBlock();
      uint32_t EpochIndex() { return countEpochs; }
      uint32_t EpochInterval() { return epochInterval; }
      uint32_t BlockTimestamp() { return blockStartTime; }
      uint32_t BlockSize() { return ACTIVITY_BLOCK_SIZE; }
      uint32_t MaxSamplesPerBlock() { return ACTIVITY_MAX_SAMPLES; }
      bool ReadLogicalBlock(uint32_t logicalBlockNumber, uint8_t *buffer);
      void FinishedReading();  // Call when no longer reading

      // Updates
      bool IsSampling();
      void Event(int16_t eventType);
      void PromptConfigurationChanged(uint32_t promptConfigurationId);
      void PromptGiven(bool muted, bool unworn);
      void AddSteps(uint32_t steps);
      void SensorValues(int8_t battery, int8_t temperature); // (0xff, 0x80);

#ifdef CUEBAND_STREAM_RESAMPLED
      // To allow streaming of resampled data
      void GetBufferData(int16_t **accelValues, unsigned int *lastCount, unsigned int *totalSamples);
#endif
#ifdef CUEBAND_ACTIVITY_STATS
      int statsIndex = -1;
      int statsMin[CUEBAND_AXES] = {0};
      int statsMax[CUEBAND_AXES] = {0};
#endif
#ifdef CUEBAND_DETECT_FACE_DOWN
      bool IsFaceDown();
      unsigned int faceDownTime = 0;
#endif
#ifdef CUEBAND_DETECT_WEAR_TIME
      bool IsUnmovingActivity();
      unsigned int unmoving[CUEBAND_AXES] = {0};
#endif
      // Device interaction -- clear wear/face-down detections
      void DeviceInteraction();

      uint32_t temp_transmit_count_all = 0;   // TODO: Remove this
      uint32_t temp_transmit_count = 0;   // TODO: Remove this

      // Public configuration
      uint16_t getFormat() { return format; }
      uint16_t getEpochInterval() { return epochInterval; }
      uint16_t getHrmInterval() { return hrmInterval; }
      uint16_t getHrmDuration() { return hrmDuration; }
      bool ChangeConfig(uint16_t format, uint16_t epochInterval, uint16_t hrmInterval, uint16_t hrmDuration);     // Temporarily change config, returns true if there was a difference (caller should now call DeferWriteConfig)
      void DeferWriteConfig();
      void NewBlock();                            // New block required (old full, or config changed): if any epochs are stored, write block; begin new block.

    private:
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::FS& fs;
#ifdef CUEBAND_TRACK_MOTOR_TIMES
      Pinetime::Controllers::DateTime& dateTimeController;
      Pinetime::Controllers::MotorController& motorController;
#endif
#ifdef CUEBAND_HR_EPOCH
      Pinetime::Controllers::HeartRateController& heartRateController;
      // Only for debugging status
      bool hrWithinSampling = false;
      uint32_t hrEpochOffset = 0;
      int debugMeanBpm = -1, debugDeltaMin = -1, debugDeltaMax = -1;
#endif

      std::array<uint8_t, 6> deviceAddress;

      bool DeleteFile(int file);
      bool OpenFileReading(int file);
      uint32_t LogicalBlockToPhysicalBlock(uint32_t logicalBlockNumber, int *physicalFile);
      uint32_t ReadPhysicalBlock(int physicalFile, uint32_t physicalBlockNumber, uint8_t *buffer);  // Get physical block number and, if buffer given, read block data
      bool AppendPhysicalBlock(int physicalFile, uint32_t logicalBlockNumber, uint8_t *buffer);

      void StartNewBlock();
      bool WriteActiveBlock();                    // Write active block
      bool FinalizeBlock(uint32_t logicalIndex);  // Write into buffer (even if partial) -- used before storing/transmitting

      bool InitialFileScan();

      void StartEpoch();
      bool WriteEpoch();

      bool isInitialized = false;

      // Activity Configuration
      uint16_t format = CUEBAND_FORMAT_VERSION;  // Algorithm and logging format (see `format` below, =0x0002) -- read back status to see the actual format in use (the requested one may not be supported)
      uint16_t epochInterval = CUEBAND_ACTIVITY_EPOCH_INTERVAL; // 60;  // Epoch interval (=60 seconds) -- read back status to see the actual interval in use (the requested one may not be supported)
      uint16_t hrmInterval = 0;                 // HR periodic sampling interval (seconds, 0=no interval)
      uint16_t hrmDuration = 0;                 // HR periodic sampling duration (seconds, 0=disabled, >0 && >=interval continuous)

      uint32_t configChanged = 0;               // Debounce config changes before writing

      void InitConfig();
      int ReadConfig();
      int WriteConfig();

      ActivityMeta meta[CUEBAND_ACTIVITY_FILES] = {0};

      uint32_t activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
      int activeFile = 0;

      uint32_t currentTime = 0;
      uint8_t accelerometerInfo = 0;

      resampler_t resampler;

      // Block-level data
      uint8_t activeBlock[ACTIVITY_BLOCK_SIZE] __attribute__((aligned(8)));
      int8_t lastBattery = 0xff;
      int8_t lastTemperature = 0x80;
      uint32_t promptConfigurationId = (uint32_t)-1;
      uint32_t blockStartTime = 0;
      uint32_t countEpochs = 0;

      // Current epoch
      uint32_t epochStartTime = 0;
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
      uint32_t epochSumFilteredSvmMO = 0;
#else
      uint32_t epochSumSvm = 0;
#endif
      uint32_t epochSumSvmMO = 0;
      uint32_t epochSumCount = 0;
      uint16_t epochEvents = 0x0000;
      uint32_t epochSteps = 0;
      uint32_t epochPromptCount = 0;
      uint32_t epochSnoozeMutedPromptCount = 0;
      uint32_t epochUnwornMutedPromptCount = 0;

      // Reading (file stays open until written to, or FinishedReading() called)
      int readingFile = -1;
      lfs_file_t file_p = {0};

      // Debug error tracking
      uint32_t errWrite = 0, errWriteLast = 0, errWriteLastInitial=0;
      uint32_t errRead = 0, errReadLast = 0, errReadLogicalLast = 0;
      uint32_t errScan = 0;

      // Resample buffer
      int16_t outputBuffer[CUEBAND_AXES * ACTIVITY_RESAMPLE_BUFFER_SIZE];
      unsigned int totalSamples = 0;
      unsigned int lastCount = 0;

#ifdef CUEBAND_BUFFER_ENABLED
      unsigned int lastTotalSamples = 0;
#endif

    };
  }
}

#endif
