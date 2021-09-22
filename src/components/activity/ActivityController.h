#pragma once

#include "cueband.h"
#include "resampler.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#include "components/settings/Settings.h"
#include "components/fs/FS.h"

#if defined(CUEBAND_BUFFER_ENABLED)
#include "src/components/motion/MotionController.h"
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
#define ACTIVITY_EVENT_RESERVED_1          0x0400 // (Reserved 1)
#define ACTIVITY_EVENT_CUE_CONFIGURATION   0x0800 // Cue: new configuration written
#define ACTIVITY_EVENT_CUE_OPENED          0x1000 // Cue: user opened app
#define ACTIVITY_EVENT_CUE_TEMPORARY       0x2000 // Cue: temporary changed of prompting configuration
#define ACTIVITY_EVENT_CUE_SNOOZED         0x4000 // Cue: user snoozed cueing
#define ACTIVITY_EVENT_RESERVED_2          0x8000 // (Reserved 2)

#define ACTIVITY_RATE 30    // Common activity monitor rate: 32 Hz (Philips Actiwatch Spectrum+/Pro/2, CamNtech Actiwave Motion, Minisun IDEEA, Fit.life Fitmeter, BodyMedia SenseWear); or 30 Hz (ActiGraph GT3X/GT1M)

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
      ActivityController(Pinetime::Controllers::Settings& settingsController, Pinetime::Controllers::FS& fs);

#if defined(CUEBAND_BUFFER_ENABLED)
      // Add multiple samples at the original source rate
      void AddSamples(MotionController &motionController);
#endif
      // Add single sample at (ACTIVITY_RATE) Hz
      void AddSingleSample(int16_t x, int16_t y, int16_t z);
      void TimeChanged(uint32_t time);

      void Init(uint32_t time, std::array<uint8_t, 6> deviceAddress, uint8_t accelerometerInfo);
      void DestroyData();

      void DebugText(char *debugText);  // requires ~200 byte buffer

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
      void PromptConfigurationChanged(int16_t promptConfigurationId);
      void PromptGiven(bool muted);
      void AddSteps(uint32_t steps);
      void SensorValues(int8_t battery, int8_t temperature); // (0xff, 0x80);

#ifdef CUEBAND_STREAM_RESAMPLED
      // To allow streaming of resampled data
      void GetBufferData(int16_t **accelValues, unsigned int *lastCount, unsigned int *totalSamples);
#endif

      uint32_t temp_transmit_count_all = 0;   // TODO: Remove this
      uint32_t temp_transmit_count = 0;   // TODO: Remove this

    private:
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::FS& fs;

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

      ActivityMeta meta[CUEBAND_ACTIVITY_FILES] = {0};

      uint32_t activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
      int activeFile = 0;

      uint32_t currentTime = 0;
      uint8_t accelerometerInfo = 0;

      resampler_t resampler;

      // Config
      uint32_t epochInterval = CUEBAND_ACTIVITY_EPOCH_INTERVAL; // 60;

      // Block-level data
      uint8_t activeBlock[ACTIVITY_BLOCK_SIZE] __attribute__((aligned));
      int8_t lastBattery = 0xff;
      int8_t lastTemperature = 0x80;
      int16_t promptConfigurationId = 0xffff;
      uint32_t blockStartTime = 0;
      uint32_t countEpochs = 0;

      // Current epoch
      uint32_t epochStartTime = 0;
      uint32_t epochSumSvm = 0;
      uint32_t epochSumCount = 0;
      uint16_t epochEvents = 0x0000;
      uint32_t epochSteps = 0;
      uint32_t epochPromptCount = 0;
      uint32_t epochMutedPromptCount = 0;

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
