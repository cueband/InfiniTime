#include "cueband.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#include "ActivityController.h"

using namespace Pinetime::Controllers;

#define ACTIVITY_DATA_FILENAME "ACTIVITY.BIN"


ActivityController::ActivityController(Controllers::Settings& settingsController, Pinetime::Controllers::FS& fs) : settingsController {settingsController}, fs {fs} {

  resampler_init(&this->resampler, CUEBAND_BUFFER_EFFECTIVE_RATE, ACTIVITY_RATE, CUEBAND_AXES);

}

static uint16_t sum_16(uint8_t *data, size_t count) {
  uint16_t sum = 0;
  for (count >>= 1; count; --count, data += 2)
    sum += data[0] + ((uint16_t)data[1] << 8);
  return sum;
}

// Derived from Microchip AppNote 91040a by Ross M. Fosler, via https://stackoverflow.com/questions/1100090
static uint16_t int_sqrt32(uint32_t x) {
    uint16_t add = UINT16_C(0x8000), res = 0;
    for(int i = 0; i < 16; i++) {
        uint16_t temp = res | add;
        uint32_t g2 = (uint32_t)temp * temp;
        if (x >= g2) res = temp;
        add >>= 1;
    }
    return res;
}

// To allow streaming of resampled data
#ifdef CUEBAND_STREAM_RESAMPLED
void ActivityController::GetBufferData(int16_t **accelValues, unsigned int *lastCount, unsigned int *totalSamples) {
  *accelValues = this->outputBuffer;
  *lastCount = this->lastCount;
  *totalSamples = this->totalSamples;
  return;
}
#endif


#if defined(CUEBAND_BUFFER_ENABLED)
// Add multiple samples at the original source rate
void ActivityController::AddSamples(MotionController &motionController) {
  int16_t *accelValues = NULL;
  unsigned int lastCount = 0;
  unsigned int totalSamples = 0;
  motionController.GetBufferData(&accelValues, &lastCount, &totalSamples);

  // Only add samples if they're new
  if (totalSamples != lastTotalSamples) {
    lastTotalSamples = totalSamples;
#if (CUEBAND_BUFFER_EFFECTIVE_RATE == ACTIVITY_RATE)
    for (unsigned int i = 0; i < lastCount; i++) {
      AddSingleSample(accelValues[CUEBAND_AXES * i + 0], accelValues[CUEBAND_AXES * i + 1], accelValues[CUEBAND_AXES * i + 2]);
    }
#else
    // Filter/resample data to a rate of (ACTIVITY_RATE) Hz
    resampler_input(&resampler, accelValues, lastCount);
    
    size_t outputCount;
    while ((outputCount = resampler_output(&resampler, this->outputBuffer, sizeof(this->outputBuffer) / sizeof(this->outputBuffer[0]) / CUEBAND_AXES)) != 0) {  // ACTIVITY_RESAMPLE_BUFFER_SIZE
      const int16_t *out = this->outputBuffer;
      for (unsigned int i = 0; i < outputCount; i++) {
        AddSingleSample(out[0], out[1], out[2]);
        out += CUEBAND_AXES;
      }
      this->lastCount = outputCount;
      this->totalSamples += outputCount;
    }
#endif
  }
}
#endif

// Add a single sample at (ACTIVITY_RATE) Hz
void ActivityController::AddSingleSample(int16_t x, int16_t y, int16_t z) {
  if (!isInitialized) return;

  uint32_t sum_squares = 0;

  #if CUEBAND_BUFFER_16BIT_SCALE != 4096
    #warning "This was written with the assumption that CUEBAND_BUFFER_16BIT_SCALE (1 g at 16-bit) = 4096 -- check through for possible overflows etc. as CUEBAND_BUFFER_SAMPLE_RANGE != 8."
  #endif

  #if CUEBAND_BUFFER_SAMPLE_RANGE > 8
    #error "The data should be clipped to at most +/- 8 g if the sensor is configured higher"
  #endif

  // Each square is at most (-32768 * -32768 =) 1073741824; so the sum-of-squares is at most 3221225472; sqrt is at most 56755.
  sum_squares += (uint32_t)((int32_t)x * x);
  sum_squares += (uint32_t)((int32_t)y * y);
  sum_squares += (uint32_t)((int32_t)z * z);
  uint16_t svm = int_sqrt32(sum_squares);

  // 60 second epoch at (ACTIVITY_RATE = 30 or 32) Hz;  Worst-case (32 Hz): gives (60*32=) 1920 samples; maximum sum of svm (60*32*56755=) 108969600 (27-bit).
  // CONSIDER: Storing as mean raw value (maximum of 56755), only 16-bit, freeing 16-bits for an alternative calculation (ENMO?)
  epochSumSvm += svm;
  epochSumCount++;

}

void ActivityController::StartEpoch() {
  epochStartTime = currentTime;
  epochSumSvm = 0;
  epochSumCount = 0;
  epochEvents = 0x0000;
  epochSteps = 0;
  epochPromptCount = 0;
  epochMutedPromptCount = 0;
}

bool ActivityController::WriteEpoch() {
  if (countEpochs < ACTIVITY_MAX_SAMPLES)
  {
    uint8_t *data = activeBlock + ACTIVITY_HEADER_SIZE + (countEpochs * ACTIVITY_SAMPLE_SIZE);

    uint16_t steps = 0;
    steps |= (epochPromptCount > 7 ? 7 : epochPromptCount) << 13;
    steps |= (epochMutedPromptCount > 7 ? 7 : epochMutedPromptCount) << 10;
    steps |= (epochSteps > 1023) ? 1023 : epochSteps;

    // @0 Event flags
    data[0] = (uint8_t)epochEvents; data[1] = (uint8_t)(epochEvents >> 8);

    // @2 Lower 10-bits: step count; next 3-bits: muted prompts count (0-7 saturates); top 3-bits: prompt count (0-7 saturates).
    data[2] = (uint8_t)steps; data[3] = (uint8_t)(steps >> 8);

    // Calculate the mean SVM value
    uint32_t meanSvm;
    if (epochSumCount >= ACTIVITY_RATE * epochInterval * 50 / 100) {
      // At least half of the expected samples were made, use the mean value.
      meanSvm = epochSumSvm / epochSumCount;
      // Saturate/clip to maximum allowed
      if (meanSvm > 0xfffe) meanSvm = 0xfffe;
    } else {
      // Less than half of the expected samples were made, use the invalid value
      meanSvm = 0xffff;
    }

    // @4 Mean of the SVM values for the entire epoch
    data[4] = (uint8_t)meanSvm; data[5] = (uint8_t)(meanSvm >> 8);

    // @6 Alternative activity calculation (e.g. for sleep or PAEE; possibly heart-rate?)
    // TODO: Calculate an alternative activity value
    uint32_t movement = 0;
    data[6] = (uint8_t)movement; data[7] = (uint8_t)(movement >> 8);

    countEpochs++;
  }
  
  return true;
}

void ActivityController::TimeChanged(uint32_t time) {
  currentTime = time;

  if (!isInitialized) return;

  uint32_t currentEpoch = epochStartTime / epochInterval;
  uint32_t epochNow = currentTime / epochInterval;
  // Epoch has changed from the current one
  if (epochNow != currentEpoch) {
    // Write this epoch
    WriteEpoch();

    uint32_t blockEpoch = blockStartTime / epochInterval;

    // If the block is full, or we're not in the correct sequence (e.g. the time changed)...
    if (countEpochs >= ACTIVITY_MAX_SAMPLES || epochNow - blockEpoch != countEpochs) {
      // ...store to drive
      bool written = WriteActiveBlock();
      // Advance
      if (written) {
        activeBlockLogicalIndex++;
        activeBlockPhysicalIndex++;
      } else {
        errWrite++;
      }
      // Begin a new block
      StartNewBlock();
    } else {
      StartEpoch();
    }
  }


}


void ActivityController::FinishedReading() {
  if (isReading) {
    fs.FileClose(&file_p);
    isReading = false;
  }
}

void ActivityController::DestroyData() {
  // Ensure we've not got the file open for reading
  FinishedReading();

  // Remove activity file
  fs.FileDelete(ACTIVITY_DATA_FILENAME);

  // Clear stats
  fileSize = 0;
  blockCount = 0;

  // Reset active block
  activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
  activeBlockPhysicalIndex = ACTIVITY_BLOCK_INVALID;

  isInitialized = true;

  StartNewBlock();
}

bool ActivityController::FinalizeBlock(uint32_t logicalIndex) {
  uint16_t formatVersion = 0x0000;

  // Header
  activeBlock[0] = 'A'; activeBlock[1] = 'D';                                                                           // @0  ASCII 'A' and 'D' as little-endian (= 0x4441)
  activeBlock[2] = (uint8_t)(ACTIVITY_BLOCK_SIZE - 4); activeBlock[3] = (uint8_t)((ACTIVITY_BLOCK_SIZE - 4) >> 8);      // @2  Bytes following the type/length (BLOCK_SIZE-4=252)
  activeBlock[4] = (uint8_t)formatVersion; activeBlock[5] = (uint8_t)(formatVersion >> 8);                              // @4  0x00 = current format (8-bytes per sample)
  activeBlock[6] = (uint8_t)logicalIndex; activeBlock[7] = (uint8_t)(logicalIndex >> 8);                                // @6  Logical block identifier
  activeBlock[8] = (uint8_t)(logicalIndex >> 16); activeBlock[9] = (uint8_t)(logicalIndex >> 24);                       //     ...
  activeBlock[10] = deviceAddress[5]; activeBlock[11] = deviceAddress[4]; activeBlock[12] = deviceAddress[3];           // @10 Device ID (address)
  activeBlock[13] = deviceAddress[2]; activeBlock[14] = deviceAddress[1]; activeBlock[15] = deviceAddress[0];           //     ...
  activeBlock[16] = (uint8_t)blockStartTime; activeBlock[17] = (uint8_t)(blockStartTime >> 8);                          // @16 Seconds since epoch for the first sample
  activeBlock[18] = (uint8_t)(blockStartTime >> 16); activeBlock[19] = (uint8_t)(blockStartTime >> 24);                 //     ...
  activeBlock[20] = (uint8_t)(countEpochs > 255 ? 255 : countEpochs);                                                   // @20 Number of valid samples (up to 28 samples when 8-bytes each in a 256-byte block)
  activeBlock[21] = (uint8_t)(epochInterval > 255 ? 255 : epochInterval);                                               // @21 Epoch interval (seconds, = 60)
  activeBlock[22] = (uint8_t)promptConfigurationId; activeBlock[25] = (uint8_t)(promptConfigurationId >> 8);            // @22 Active prompt configuration ID (may remove: this is just as a diagnostic as it can change during epoch)
  activeBlock[24] = (uint8_t)(promptConfigurationId >> 16); activeBlock[25] = (uint8_t)(promptConfigurationId >> 24);   //     ...
  activeBlock[26] = lastBattery;                                                                                        // @26 Battery (0xff=unknown; percentage?)
  activeBlock[27] = accelerometerInfo;                                                                                  // @27 Accelerometer (bottom 2 bits sensor type; next 2 bits reserved for future use; next 2 bits reserved for rate information; top 2 bits reserved for scaling information).
  activeBlock[28] = lastTemperature;                                                                                    // @28 Temperature (degrees C, signed 8-bit value, 0x80=unknown)
  activeBlock[29] = CUEBAND_VERSION_NUMBER;                                                                             // @29 Firmware version

  // Payload
  //memset(activeBlock + ACTIVITY_HEADER_SIZE, 0x00, ACTIVITY_PAYLOAD_SIZE);

  // Checksum
  uint16_t checksum = (uint16_t)(-sum_16(activeBlock, ACTIVITY_BLOCK_SIZE - 2));
  activeBlock[ACTIVITY_BLOCK_SIZE - 2] = (uint8_t)checksum;
  activeBlock[ACTIVITY_BLOCK_SIZE - 1] = (uint8_t)(checksum >> 8);

  return true;
}

void ActivityController::StartNewBlock() {
  if (!isInitialized) return;

  // Bootstrap empty system
  if (activeBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) {
    // CONSIDER: Randomize top bits of block index when data is cleared?
    activeBlockLogicalIndex = 0;
  }
  if (activeBlockPhysicalIndex == ACTIVITY_BLOCK_INVALID) {
    activeBlockPhysicalIndex = 0;
  }

  // Erase block data
  memset(activeBlock, 0x00, ACTIVITY_HEADER_SIZE);                          // Header
  memset(activeBlock + ACTIVITY_HEADER_SIZE, 0xff, ACTIVITY_PAYLOAD_SIZE);  // Payload
  memset(activeBlock + ACTIVITY_BLOCK_SIZE - 2, 0x00, 2);                   // Checksum

  blockStartTime = currentTime;
  countEpochs = 0;

  StartEpoch();
}

bool ActivityController::OpenFileReading() {
  int ret;
  if (isReading) return true;
  ret = fs.FileOpen(&file_p, ACTIVITY_DATA_FILENAME, LFS_O_RDONLY|LFS_O_CREAT);
  if (ret != LFS_ERR_OK) return false;
  ret = fs.FileSize(&file_p);
  if (ret < 0) {
    fs.FileClose(&file_p);
    return false;
  }
  fileSize = ret;
  blockCount = fileSize / ACTIVITY_BLOCK_SIZE;
  isReading = true;
  return true;
}

uint32_t ActivityController::EarliestLogicalBlock() {
  uint32_t earliestLogicalBlock = activeBlockLogicalIndex - blockCount;
  return earliestLogicalBlock;
}

uint32_t ActivityController::ActiveLogicalBlock() {
  return activeBlockLogicalIndex;
}

uint32_t ActivityController::LogicalBlockToPhysicalBlock(uint32_t logicalBlockNumber) {
  // Must be initialized
  if (!isInitialized) return ACTIVITY_BLOCK_INVALID;

  // The invalid logical block maps to the invalid physical block
  if (logicalBlockNumber == ACTIVITY_BLOCK_INVALID) return ACTIVITY_BLOCK_INVALID;

  // If there is no active block...
  if (activeBlockLogicalIndex == ACTIVITY_BLOCK_INVALID || activeBlockPhysicalIndex == ACTIVITY_BLOCK_INVALID) return ACTIVITY_BLOCK_INVALID;

  // If we have no physical blocks, the block cannot be found
  if (blockCount == 0) return ACTIVITY_BLOCK_INVALID;

  // The currently-active logical block is activeBlockLogicalIndex, stored at activeBlockPhysicalIndex.
  uint32_t earliestLogicalBlock = activeBlockLogicalIndex - blockCount;
  uint32_t earliestPhysicalBlock = activeBlockPhysicalIndex % blockCount;

  // Logical block before the earliest available, or at or after the current active block
  if (logicalBlockNumber < earliestLogicalBlock || logicalBlockNumber >= activeBlockLogicalIndex) {
    return ACTIVITY_BLOCK_INVALID;
  }

  // Logical-to-physical mapping
  uint32_t offset = logicalBlockNumber - earliestLogicalBlock;
  uint32_t physicalBlockNumber = (earliestPhysicalBlock + offset) % blockCount;

  return physicalBlockNumber;
}

// Always fills buffer if specified (0xff not found), returns id of block actually read, otherwise ACTIVITY_BLOCK_INVALID.
uint32_t ActivityController::ReadPhysicalBlock(uint32_t physicalBlockNumber, uint8_t *buffer) {
  int ret;

  // Must not be asking for the invalid block
  if (physicalBlockNumber == ACTIVITY_BLOCK_INVALID) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 1;
    return ACTIVITY_BLOCK_INVALID;
  }

  // The file must be successfully open for reading
  if (!OpenFileReading()) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 2;
    return ACTIVITY_BLOCK_INVALID;
  }

  // If out of range of whole blocks present
  if (physicalBlockNumber >= blockCount) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 3;
    return ACTIVITY_BLOCK_INVALID;
  }
  
  // Calculate block offset
  uint32_t offset = physicalBlockNumber * ACTIVITY_BLOCK_SIZE;

  // Seek to location (if required)
  uint32_t location = fs.FileTell(&file_p);
  if (location != offset) {
    location = fs.FileSeek(&file_p, offset);
    if (location != offset) {
      if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
      errRead++;
      errReadLast = 4;
      return ACTIVITY_BLOCK_INVALID;
    }
  }

  uint8_t headerBuffer[10];
  uint8_t *header;
  if (buffer == nullptr) {
    // Just read header
    ret = fs.FileRead(&file_p, headerBuffer, sizeof(headerBuffer));
    if (ret != sizeof(headerBuffer)) {
      errRead++;
      errReadLast = 5;
      return ACTIVITY_BLOCK_INVALID;
    }
    header = headerBuffer;
  } else {
    // Read whole block
    ret = fs.FileRead(&file_p, buffer, ACTIVITY_BLOCK_SIZE);
    if (ret != ACTIVITY_BLOCK_SIZE) {
      memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
      errRead++;
      errReadLast = 6;
      return ACTIVITY_BLOCK_INVALID;
    }
    header = buffer;
  }

  // Check header
  if (header[0] != 'A' || header[1] != 'D' || header[2] + (header[3] << 8) != (ACTIVITY_BLOCK_SIZE - 4)) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 7;
    return ACTIVITY_BLOCK_INVALID;
  }

  // Ignore version: header[4] header[5]
  
  // Extract block id
  uint32_t readBlockId = (uint32_t)header[6] | ((uint32_t)header[7] << 8) | ((uint32_t)header[8] << 16) | ((uint32_t)header[9] << 24);
  return readBlockId;
}

// Read logical block, can include active block, always fills buffer (0xff if not found), returns true if requested block was read correctly.
bool ActivityController::ReadLogicalBlock(uint32_t logicalBlockNumber, uint8_t *buffer) {
  if (buffer == nullptr) {
    return false;
  }

  if (!isInitialized) {
    return false;
  }

  // Catch a read for the invalid block
  if (logicalBlockNumber == ACTIVITY_BLOCK_INVALID) {
    memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    return false;
  }

  // Allow read of current active block
  if (logicalBlockNumber == activeBlockLogicalIndex) {
    FinalizeBlock(activeBlockLogicalIndex);  // Update header and checksum
    memcpy(buffer, activeBlock, ACTIVITY_BLOCK_SIZE);
    return true;
  }

  // Find physical block location
  uint32_t physicalBlockNumber = LogicalBlockToPhysicalBlock(logicalBlockNumber);
  if (physicalBlockNumber == ACTIVITY_BLOCK_INVALID) {
    memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    return false;
  }

  // Read the block
  uint32_t readBlockId = ReadPhysicalBlock(physicalBlockNumber, buffer);
  if (readBlockId != logicalBlockNumber) return false;

  return true;
}


bool ActivityController::WritePhysicalBlock(uint32_t physicalBlockNumber, uint8_t *buffer) {
  int ret;

  // Must be initialized, and not writing the invalid block, and have a valid buffer
  if (!isInitialized || physicalBlockNumber == ACTIVITY_BLOCK_INVALID || buffer == nullptr) {
    errWriteLast = 1;
    return false;
  }

  // If out of range of whole blocks present + 1
  if (physicalBlockNumber > blockCount) {
    errWriteLast = 2;
    return false;
  }

  // Calculate block offset
  uint32_t offset = physicalBlockNumber * ACTIVITY_BLOCK_SIZE;

  // Ensure we've not got the file open for reading
  FinishedReading();

  // Open for writing
  ret = fs.FileOpen(&file_p, ACTIVITY_DATA_FILENAME, LFS_O_WRONLY|LFS_O_CREAT);
  if (ret != LFS_ERR_OK) {
    errWriteLast = 3;
    return false;
  }

  // Seek to location (if required)
  uint32_t location = fs.FileTell(&file_p);
  if (location != offset) {
    location = fs.FileSeek(&file_p, offset);
    if (location != offset) {
      fs.FileClose(&file_p);
      errWriteLast = 4;
      return false;
    }
  }

  // Write block
  ret = fs.FileWrite(&file_p, buffer, ACTIVITY_BLOCK_SIZE);

  if (ret != ACTIVITY_BLOCK_SIZE) {
    fs.FileClose(&file_p);
    errWriteLast = 5;
    return false;
  }

  // Update size and block count
  fileSize = fs.FileSize(&file_p);
  fs.FileClose(&file_p);
  blockCount = fileSize / ACTIVITY_BLOCK_SIZE;

  return true;
}

const char * ActivityController::DebugText() {
  char *p = debugText;
#ifdef CUEBAND_WRITE_TEST_FILE
  p += sprintf(p, "TW:%d et:%d\n", testWrite, errTest);
#endif
  p += sprintf(p, "I:%s BC:%lu es:%d\n", isInitialized ? "t" : "f", blockCount, errScan);
  p += sprintf(p, "Al:%ld Ap:%ld\n", ActiveLogicalBlock(), activeBlockPhysicalIndex);   // debug output as signed to spot invalid=-1
  p += sprintf(p, "F:%ld\n", EarliestLogicalBlock());   // debug as signed to spot invalid=-1
  p += sprintf(p, "S:%lu\n", blockStartTime);
  p += sprintf(p, "N:%lu\n", currentTime);
  p += sprintf(p, "E:%lu C:%lu\n", countEpochs, epochSumCount);
  p += sprintf(p, "sum:%lu\n", epochSumSvm);
  p += sprintf(p, "I:%lu er:%lu/%lu ew:%lu/%lu/%lu\n", epochInterval, errRead, errReadLast, errWrite, errWriteLastAppend, errWriteLastWithin);
  int elapsed = currentTime - epochStartTime;
  int estimatedRate = -1;
  if (elapsed > 0) estimatedRate = epochSumCount / elapsed;
  p += sprintf(p, "e:%d r:%d\n", elapsed, estimatedRate);
  return debugText;
}

// Write block: seek to block location (append additional bytes if location after end, or wrap-around if file maximum size or no more drive space)
bool ActivityController::WriteActiveBlock() {
  FinalizeBlock(activeBlockLogicalIndex);

  bool written = false;
  
  // If the index is just past the end of the file, and not greater than maximum size...
  if (activeBlockPhysicalIndex >= blockCount && activeBlockPhysicalIndex < ACTIVITY_MAXIMUM_BLOCKS) {
    // Try to append past the end of the file
    errWriteLast = 0;
    written = WritePhysicalBlock(activeBlockPhysicalIndex, activeBlock);
    if (!written) errWriteLastAppend = errWriteLast;
  }

  // If not appended, we'll be writing within the file
  if (!written) {
    // Write inside file, wrapping-around if needed
    if (blockCount > 0) {
      activeBlockPhysicalIndex %= blockCount;
    } else {
      activeBlockPhysicalIndex = 0;
    }
    written = WritePhysicalBlock(activeBlockPhysicalIndex, activeBlock);
    if (!written) errWriteLastWithin = errWriteLast;
  }

  return written;
}


#ifdef CUEBAND_WRITE_TEST_FILE
#warning "This build replaces the data with a test file of a specific configuration."
// Writing dummy data file for testing
void ActivityController::AppendTestFile() {
    int ret;

    // State: finished or error
    if (testWrite < 0) {
      return;
    }

    // Initial state
    if (testWrite == 0) {
      // Check the current file
      if (OpenFileReading()) {
        // If the file is the required size...
        if (blockCount > 0) {
          // Check the first and last blocks
          uint32_t firstBlock = ReadPhysicalBlock(0, nullptr);
          uint32_t lastBlock = ReadPhysicalBlock(blockCount - 1, nullptr);

          // It appears to match the test file we'd like -- do not rewite
          if (blockCount == ACTIVITY_MAXIMUM_BLOCKS && firstBlock == CUEBAND_WRITE_TEST_FILE && lastBlock == CUEBAND_WRITE_TEST_FILE + ACTIVITY_MAXIMUM_BLOCKS - 1) {
            FinishedReading();
            testWrite = -3;       // not rewriting existing test file of correct specification
            InitialFileScan();    // Only now scan the file
            return;
          } else {
            errTest = 9000;       // diagnostic: reason the existing file was discarded
            if (blockCount != ACTIVITY_MAXIMUM_BLOCKS) errTest += 100;
            if (firstBlock != CUEBAND_WRITE_TEST_FILE) errTest += 10;
            if (lastBlock != CUEBAND_WRITE_TEST_FILE + ACTIVITY_MAXIMUM_BLOCKS - 1) errTest += 1;
          }
        } else errTest = -1;    // diagnostic: existing file was empty

        FinishedReading();
      } else errTest = -2;  // diagnostic: existing file could not be opened

      // DestroyData();
      fs.FileDelete(ACTIVITY_DATA_FILENAME);

      // Create file for writing
      ret = fs.FileOpen(&file_p, ACTIVITY_DATA_FILENAME, LFS_O_WRONLY|LFS_O_CREAT);
      if (ret != LFS_ERR_OK) {
        testWrite = -10; // error opening file
        return;
      }
    }

    // Check current file size
    ret = fs.FileSize(&file_p);
    if (ret < 0) {
      testWrite = -11; // error getting size of file
      fs.FileClose(&file_p);
      return;
    }

    // If finished...
    if (ret >= ACTIVITY_MAXIMUM_BLOCKS * ACTIVITY_BLOCK_SIZE) {
      testWrite = -2;       // finished test write
      fs.FileClose(&file_p);
      InitialFileScan();    // Only now scan the file
      return;
    }

    // Write the next block
    memset(activeBlock + ACTIVITY_HEADER_SIZE, 0xff, ACTIVITY_PAYLOAD_SIZE);  // Payload
    FinalizeBlock(testWrite + CUEBAND_WRITE_TEST_FILE);
    ret = fs.FileWrite(&file_p, activeBlock, ACTIVITY_BLOCK_SIZE);
    if (ret != ACTIVITY_BLOCK_SIZE) {
      testWrite = -12;       // error writing to file
      fs.FileClose(&file_p);
      return;
    }

    testWrite++;
}
#endif


bool ActivityController::IsSampling() {
#ifdef CUEBAND_WRITE_TEST_FILE
  // HACK: Use this as an opportunity to make progress writing the test file
  this->AppendTestFile();
#endif
  return isInitialized;
}

void ActivityController::Event(int16_t eventType) {
  epochEvents |= eventType;
}

void ActivityController::PromptConfigurationChanged(int16_t promptConfigurationId) {
  this->promptConfigurationId = promptConfigurationId;
}

void ActivityController::PromptGiven(bool muted) {
  if (muted) {
    epochMutedPromptCount++;
  } else {
    epochPromptCount++;
  }
}

void ActivityController::AddSteps(uint32_t steps) {
  epochSteps += steps;
}

void ActivityController::SensorValues(int8_t battery, int8_t temperature) { // (0xff, 0x80);
  lastBattery = battery; // 0xff
  lastTemperature = temperature; // 0x80;
}

// Scan for most recent logical block and its physical index, returns if data successfully continued
bool ActivityController::InitialFileScan() {
  uint32_t lastBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
  uint32_t lastBlockPhysicalIndex = ACTIVITY_BLOCK_INVALID;

  bool err = false;
  if (!OpenFileReading()) {
    if (!err) errScan = -1;    // diagnostic: error opening for reading or determining file size
    err = true;
  } else {

    // Begin scan for most recent logical block and its physical index
    if (blockCount != 0) {
      // Read start block
      uint32_t startBlockPhysicalIndex = 0;
      uint32_t startBlockLogicalIndex = ReadPhysicalBlock(startBlockPhysicalIndex, nullptr);
      if (startBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) {
        if (!err) errScan = 1000 + startBlockPhysicalIndex;    // diagnostic: error reading start block
        err = true;
      } else if (lastBlockLogicalIndex == ACTIVITY_BLOCK_INVALID || startBlockLogicalIndex > lastBlockLogicalIndex) {
        lastBlockLogicalIndex = startBlockLogicalIndex;
        lastBlockPhysicalIndex = startBlockPhysicalIndex;
      }

      // Read end block
      uint32_t endBlockPhysicalIndex = blockCount - 1;
      uint32_t endBlockLogicalIndex = ReadPhysicalBlock(endBlockPhysicalIndex, nullptr);
      if (endBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) {
        if (!err) errScan = 2000 + endBlockPhysicalIndex;    // diagnostic: error reading end block
        err = true;
      } else if (lastBlockLogicalIndex == ACTIVITY_BLOCK_INVALID || endBlockLogicalIndex > lastBlockLogicalIndex) {
        lastBlockLogicalIndex = endBlockLogicalIndex;
        lastBlockPhysicalIndex = endBlockPhysicalIndex;
      }

      // Recursively bisect the range to find any discontinuity
      for (int maxIterations = 24; maxIterations > 0; maxIterations--) {    // safety in the event of a logical error
        // If there are errors or no interval left, stop
        if (err || startBlockPhysicalIndex >= endBlockPhysicalIndex) break;

        // Find the mid-point
        uint32_t midBlockPhysicalIndex = startBlockPhysicalIndex + ((endBlockPhysicalIndex - startBlockPhysicalIndex) / 2);
        uint32_t midBlockLogicalIndex = ReadPhysicalBlock(midBlockPhysicalIndex, nullptr);
        if (midBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) {
          if (!err) errScan = 3000 + midBlockPhysicalIndex;    // diagnostic: error reading end block
          err = true;
          break;
        } else if (lastBlockLogicalIndex == ACTIVITY_BLOCK_INVALID || midBlockLogicalIndex > lastBlockLogicalIndex) {
          lastBlockLogicalIndex = midBlockLogicalIndex;
          lastBlockPhysicalIndex = midBlockPhysicalIndex;
        }

        if (startBlockLogicalIndex + (midBlockPhysicalIndex - startBlockPhysicalIndex) != midBlockLogicalIndex) {
          // If the discontinuity is in the first half, bisect to the first half
          if (endBlockPhysicalIndex == midBlockPhysicalIndex) break;  // Don't check the same interval again (mid point rounds down)
          endBlockPhysicalIndex = midBlockPhysicalIndex;
          endBlockLogicalIndex = midBlockLogicalIndex;
        } else if (midBlockLogicalIndex + (endBlockPhysicalIndex - midBlockPhysicalIndex) != endBlockLogicalIndex) {
          // If the discontinuity is in the second half, bisect to the second half
          if (startBlockPhysicalIndex == midBlockPhysicalIndex) break;  // Don't check the same interval again (mid point rounds down)
          startBlockPhysicalIndex = midBlockPhysicalIndex;
          startBlockLogicalIndex = midBlockLogicalIndex;
        } else {
          // Otherwise there is no discontinuity within the interval, stop
          break;
        }
      }
    }
  }

  FinishedReading();

  // #if (CUEBAND_ACTIVITY_EPOCH_INTERVAL < 60)
  //   // If using a debug epoch size, and our data is larger than the maximum size, fake an initialization error so the data is wiped
  //   if (blockCount > ACTIVITY_MAXIMUM_BLOCKS) err = true;
  // #endif

  // If we located the last logical block (and its physical location), the active block will be the next block index
  if (!err && lastBlockLogicalIndex != ACTIVITY_BLOCK_INVALID && lastBlockPhysicalIndex != ACTIVITY_BLOCK_INVALID) {
    activeBlockLogicalIndex = lastBlockLogicalIndex + 1;
    activeBlockPhysicalIndex = lastBlockPhysicalIndex + 1;
    isInitialized = true;
  } else if (!err && blockCount == 0) {
    // ...otherwise, if the file is empty, we're good to go from the start condition (these indexes are set to zero later)
    activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
    activeBlockPhysicalIndex = ACTIVITY_BLOCK_INVALID;
    isInitialized = true;
    if (errScan == 0) errScan = -123;    // diagnostic: no other issue reported, but the data file seems to be empty
  } else {
    // ...otherwise, we have a problem -- start a new data file
    if (errScan == 0) { // diagnostic: the data had to be destroyed
      errScan = 90000;
      if (lastBlockPhysicalIndex == ACTIVITY_BLOCK_INVALID) errScan += 1000;
      if (lastBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) errScan += 100;
      if (blockCount == 0) errScan += 10;
      if (err) errScan += 1;
    }
    DestroyData();
  }

  StartNewBlock();

  return !err;
}


void ActivityController::Init(uint32_t time, std::array<uint8_t, 6> deviceAddress, uint8_t accelerometerInfo) {
  this->currentTime = time;
  this->deviceAddress = deviceAddress;
  this->accelerometerInfo = accelerometerInfo;

  activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
  activeBlockPhysicalIndex = ACTIVITY_BLOCK_INVALID;
  isInitialized = false;

#ifdef CUEBAND_WRITE_TEST_FILE
  // If writing a test file (incrementally, as it may be large), don't perform the initial scan until after that is done
  testWrite = 0;
#else
  InitialFileScan();
#endif
}

#endif
