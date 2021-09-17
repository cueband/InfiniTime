#include "cueband.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#include "ActivityController.h"

using namespace Pinetime::Controllers;

#define ACTIVITY_DATA_FILENAME "ACTV%04d.BIN"


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
  if (readingFile >= 0) {
    fs.FileClose(&file_p);
    readingFile = -1;
  }
}

bool ActivityController::DeleteFile(int file) {
  // Ensure we've not got any files open for reading
  FinishedReading();

  char filename[16] = {0};
  sprintf(filename, ACTIVITY_DATA_FILENAME, file);

  int ret = fs.FileDelete(filename);

  meta[file].blockCount = 0;
  meta[file].lastLogicalBlock = ACTIVITY_BLOCK_INVALID;
  //meta[file].err    // leave diagnostic error values to be sticky

  return (ret == LFS_ERR_OK);
}

void ActivityController::DestroyData() {
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    DeleteFile(file);
  }

  // Reset active block
  activeBlockLogicalIndex = 0;
  activeFile = 0;

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

  // Erase block data
  memset(activeBlock, 0x00, ACTIVITY_HEADER_SIZE);                          // Header
  memset(activeBlock + ACTIVITY_HEADER_SIZE, 0xff, ACTIVITY_PAYLOAD_SIZE);  // Payload
  memset(activeBlock + ACTIVITY_BLOCK_SIZE - 2, 0x00, 2);                   // Checksum

  blockStartTime = currentTime;
  countEpochs = 0;

  StartEpoch();
}

bool ActivityController::OpenFileReading(int file) {
  int ret;
  if (readingFile == file) return true;
  FinishedReading();
  char filename[16] = {0};
  sprintf(filename, ACTIVITY_DATA_FILENAME, file);
  ret = fs.FileOpen(&file_p, filename, LFS_O_RDONLY|LFS_O_CREAT);
  if (ret == LFS_ERR_CORRUPT) fs.FileDelete(filename);    // No other sensible action?
  if (ret != LFS_ERR_OK) return false;
  readingFile = file;
  return true;
}

uint32_t ActivityController::BlockCount() {
  uint32_t countBlocks = 0;
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    countBlocks += meta[file].blockCount;
  }
  return countBlocks;
}

uint32_t ActivityController::EarliestLogicalBlock() {
  if (activeFile < 0 || activeBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) return ACTIVITY_BLOCK_INVALID;
  uint32_t countBlocks = BlockCount();
  return activeBlockLogicalIndex - countBlocks;
}

uint32_t ActivityController::ActiveLogicalBlock() {
  return activeBlockLogicalIndex;
}

uint32_t ActivityController::LogicalBlockToPhysicalBlock(uint32_t logicalBlockNumber, int *physicalFile) {
  // Must be initialized
  if (!isInitialized) return ACTIVITY_BLOCK_INVALID;

  // The invalid logical block maps to the invalid physical block
  if (logicalBlockNumber == ACTIVITY_BLOCK_INVALID) return ACTIVITY_BLOCK_INVALID;

  // If there is no active block...
  if (activeBlockLogicalIndex == ACTIVITY_BLOCK_INVALID || activeFile < 0) return ACTIVITY_BLOCK_INVALID;

  // Scan through the files to find the correct range
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    if (meta[file].blockCount > 0) {
      uint32_t firstBlockLogicalIndex = meta[file].lastLogicalBlock + 1 - meta[file].blockCount;
      uint32_t lastBlockLogicalIndex = meta[file].lastLogicalBlock;
      if (logicalBlockNumber >= firstBlockLogicalIndex && logicalBlockNumber <= lastBlockLogicalIndex) {
        uint32_t physicalBlockNumber = logicalBlockNumber - firstBlockLogicalIndex;
        *physicalFile = file;
        return physicalBlockNumber;
      }
    }
  }

  // Not found
  return ACTIVITY_BLOCK_INVALID;
}

// Always fills buffer if specified (0xff not found), returns id of block actually read, otherwise ACTIVITY_BLOCK_INVALID.
uint32_t ActivityController::ReadPhysicalBlock(int physicalFile, uint32_t physicalBlockNumber, uint8_t *buffer) {
  int ret;

  // Must not be asking for the invalid block
  if (physicalBlockNumber == ACTIVITY_BLOCK_INVALID) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 1;
    return ACTIVITY_BLOCK_INVALID;
  }

  // The file must be successfully open for reading
  if (!OpenFileReading(physicalFile)) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 2;
    return ACTIVITY_BLOCK_INVALID;
  }

  // If out of range of whole blocks present
  ret = fs.FileSize(&file_p);
  if (ret < 0) {
    if (buffer != nullptr) memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    errRead++;
    errReadLast = 8;
    return ACTIVITY_BLOCK_INVALID;
  }
  uint32_t blockCount = ret / ACTIVITY_BLOCK_SIZE;
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
  errReadLogicalLast = 0;   // diagnostic

  if (buffer == nullptr) {
    errReadLogicalLast = 1;
    return false;
  }

  if (!isInitialized) {
    errReadLogicalLast = 2;
    return false;
  }

  // Catch a read for the invalid block
  if (logicalBlockNumber == ACTIVITY_BLOCK_INVALID) {
    errReadLogicalLast = 3;
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
  int physicalFile = -1;
  uint32_t physicalBlockNumber = LogicalBlockToPhysicalBlock(logicalBlockNumber, &physicalFile);
  if (physicalBlockNumber == ACTIVITY_BLOCK_INVALID || physicalFile < 0 || physicalFile >= CUEBAND_ACTIVITY_FILES) {
    errReadLogicalLast = 4;
    memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    return false;
  }

  // Read the block
  uint32_t readBlockId = ReadPhysicalBlock(physicalFile, physicalBlockNumber, buffer);
  if (readBlockId != logicalBlockNumber) {
    errReadLogicalLast = 5;
    memset(buffer, 0xFF, ACTIVITY_BLOCK_SIZE);
    return false;
  }

  return true;
}


bool ActivityController::AppendPhysicalBlock(int physicalFile, uint32_t logicalBlockNumber, uint8_t *buffer) {
  int ret;

  // Must be initialized, and not writing the invalid block, and have a valid buffer
  if (!isInitialized || logicalBlockNumber == ACTIVITY_BLOCK_INVALID || buffer == nullptr || physicalFile < 0 || physicalFile >= CUEBAND_ACTIVITY_FILES) {
    errWriteLast = 1;
    return false;
  }

  // Can only append to the file
  uint32_t physicalBlockNumber = meta[physicalFile].blockCount;

  if (meta[physicalFile].lastLogicalBlock != ACTIVITY_BLOCK_INVALID && logicalBlockNumber != meta[physicalFile].lastLogicalBlock + 1) {
    errWriteLast = 2;
    return false;
  }

  // Calculate block offset
  uint32_t offset = physicalBlockNumber * ACTIVITY_BLOCK_SIZE;

  // Ensure we've not got the file open for reading
  FinishedReading();

  // Open for writing
  char filename[16] = {0};
  sprintf(filename, ACTIVITY_DATA_FILENAME, physicalFile);
  ret = fs.FileOpen(&file_p, filename, LFS_O_WRONLY|LFS_O_CREAT|LFS_O_APPEND);
  if (ret != LFS_ERR_OK) {
    errWriteLast = 3;
    return false;
  }

  // Check (append) location is the end of the file, seek if not
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
  ret = fs.FileSize(&file_p);
  if (ret < 0) { ret = location + ACTIVITY_BLOCK_SIZE; }    // should probably be an error...
  meta[physicalFile].blockCount = ret / ACTIVITY_BLOCK_SIZE;
  meta[physicalFile].lastLogicalBlock = logicalBlockNumber;

  fs.FileClose(&file_p);

  return true;
}

const char * ActivityController::DebugText() {
  char *p = debugText;

  p += sprintf(p, "bc:");
  for (int f = 0; f < CUEBAND_ACTIVITY_FILES; f++) {
    p += sprintf(p, "%s%ld", f > 0 ? "/" : "", meta[f].blockCount);
  }
  p += sprintf(p, "\n");

  p += sprintf(p, "L:");
  for (int f = 0; f < CUEBAND_ACTIVITY_FILES; f++) {
    p += sprintf(p, "%s%ld", f > 0 ? "/" : "", meta[f].lastLogicalBlock);
  }
  p += sprintf(p, "\n");
  
  //p += sprintf(p, "es:%lx\n", errScan);
  p += sprintf(p, "es:");
  for (int f = 0; f < CUEBAND_ACTIVITY_FILES; f++) {
    p += sprintf(p, "%s%02x", f > 0 ? "/" : "", meta[f].err);
  }
  p += sprintf(p, "\n");
  
  p += sprintf(p, "Al:%ld Af:%d bc:%ld\n", ActiveLogicalBlock(), activeFile, (activeFile < 0 ? -1 : meta[activeFile].blockCount));   // debug output as signed to spot invalid=-1
  p += sprintf(p, "F:%ld BC:%ld\n", EarliestLogicalBlock(), BlockCount());   // debug as signed to spot invalid=-1
  p += sprintf(p, "I:%s S:%lu\n", isInitialized ? "t" : "f", blockStartTime);
  p += sprintf(p, "E:%lu C:%lu\n", countEpochs, epochSumCount);
  p += sprintf(p, "I:%lu sum:%lu\n", epochInterval, epochSumSvm);
  p += sprintf(p, "er:%lu/%lu/%lu ew:%lu/%lu/%lu\n", errRead, errReadLast, errReadLogicalLast, errWrite, errWriteLastInitial, errWriteLast);
  int elapsed = currentTime - epochStartTime;
  int estimatedRate = -1;
  if (elapsed > 0) estimatedRate = epochSumCount / elapsed;
  p += sprintf(p, "e:%d r:%d %ld/%ld", elapsed, estimatedRate, temp_transmit_count, temp_transmit_count_all);
  return debugText;
}

// Write block: seek to block location (append additional bytes if location after end, or wrap-around if file maximum size or no more drive space)
bool ActivityController::WriteActiveBlock() {

  // Cannot write
  if (activeFile < 0 || activeBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) {
    errWriteLast = 10;  // diagnostic: invalid state
    return false;
  }

  FinalizeBlock(activeBlockLogicalIndex);

  // If the index is just past the end of the file, and not greater than maximum size...
  if (meta[activeFile].blockCount >= CUEBAND_ACTIVITY_MAXIMUM_BLOCKS) {
    // Wrap to oldest file
    activeFile = (activeFile + 1) % CUEBAND_ACTIVITY_FILES;
    // Remove that file (if exists)
    DeleteFile(activeFile);
  }

  // Retry as required until all old files are removed (in case storage is full)
  bool written = false;
  errWriteLastInitial = 0;
  for (int f = 0; f < CUEBAND_ACTIVITY_FILES && !written; f++) {
    // Try to append past the end of the file
    errWriteLast = 0;
    written = AppendPhysicalBlock(activeFile, activeBlockLogicalIndex, activeBlock);
    if (f == 0) { errWriteLastInitial = errWriteLast; }
    if (written) break;

    // Otherwise, if not written, assume storage could be full, so need to erase next block
    if (!written) {
      // Wrap to oldest file
      activeFile = (activeFile + 1) % CUEBAND_ACTIVITY_FILES;
      // Remove that file (if exists)
      DeleteFile(activeFile);
    }
  }

  return written;
}

bool ActivityController::IsSampling() {
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

  // Determine metadata for each file
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    int err = 0;
    uint32_t fileSize = 0;
    uint32_t blockCount = 0;
    uint32_t firstBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
    uint32_t lastBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;

    if (OpenFileReading(file)) {
      int ret = fs.FileSize(&file_p);
      if (ret >= 0) {
        fileSize = ret;
        if (fileSize > 0) {
          blockCount = fileSize / ACTIVITY_BLOCK_SIZE;
          firstBlockLogicalIndex = ReadPhysicalBlock(file, 0, nullptr);
          lastBlockLogicalIndex = ReadPhysicalBlock(file, blockCount - 1, nullptr);
        } // else: empty file is valid
      } else {
        err |= 0x02;  // diagnostic: file size could not be determined
      }
      FinishedReading();
    } else {
      err |= 0x01;  // diagnostic: file could not be opened
    }

    if (fileSize > 0) {
      uint32_t logicalCount = lastBlockLogicalIndex - firstBlockLogicalIndex + 1;
      if (firstBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) { err |= 0x04; }    // diagnostic: first block could not be read or is invalid
      if (lastBlockLogicalIndex == ACTIVITY_BLOCK_INVALID) { err |= 0x08; }     // diagnostic: last block could not be read or is invalid
      if (fileSize != logicalCount * ACTIVITY_BLOCK_SIZE || logicalCount != blockCount) { err |= 0x10; }  // diagnostic: logical block range does not match physical block range
    }
    
    // Store metadata
    meta[file].blockCount = blockCount;
    meta[file].lastLogicalBlock = lastBlockLogicalIndex;
    meta[file].err = err;
  }

  // Find maximum logical index and associated file
  uint32_t maxLogicalIndex = ACTIVITY_BLOCK_INVALID;
  int maxLogicalFile = -1;
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    if (meta[file].err == 0 && meta[file].blockCount > 0 && meta[file].lastLogicalBlock != ACTIVITY_BLOCK_INVALID) {
      if (maxLogicalIndex == ACTIVITY_BLOCK_INVALID || meta[file].lastLogicalBlock > maxLogicalIndex) {
        maxLogicalIndex = meta[file].lastLogicalBlock;
        maxLogicalFile = file;
      }
    }
  }

  // If we have any blocks, check all files fit the sequence
  if (maxLogicalFile != -1 && maxLogicalIndex != ACTIVITY_BLOCK_INVALID) {
    // Starting at the current file and working backwards...
    uint32_t lastBlockLogicalIndex = maxLogicalIndex;
    for (int f = 0; f < CUEBAND_ACTIVITY_FILES; f++) {
      int file = (maxLogicalFile + CUEBAND_ACTIVITY_FILES - f) % CUEBAND_ACTIVITY_FILES;
      if (meta[file].lastLogicalBlock != lastBlockLogicalIndex) {
        meta[file].err |= 0x20; // diagnostic: file does not fit sequence
      }
      lastBlockLogicalIndex -= meta[file].blockCount;
    }
  }

  // Check if any files have an error
  bool anyErrors = false;
  errScan = 0;
  for (int file = 0; file < CUEBAND_ACTIVITY_FILES; file++) {
    errScan <<= 8;  // diagnostic value
    if (meta[file].err != 0) {
      anyErrors = true;
      errScan |= meta[file].err;
    }
  }

  // Any errors: remove all data
  if (anyErrors) {
    DestroyData();
  } else {

    // If we have data, continue with next logical block for the current file
    if (maxLogicalFile != -1 && maxLogicalIndex != ACTIVITY_BLOCK_INVALID) {
      activeBlockLogicalIndex = maxLogicalIndex + 1;
      activeFile = maxLogicalFile;
    } else {
      // If no data, start from scratch
      activeBlockLogicalIndex = 0;
      activeFile = 0;
    }

    isInitialized = true;

    StartNewBlock();    
  }

  return !anyErrors;
}


void ActivityController::Init(uint32_t time, std::array<uint8_t, 6> deviceAddress, uint8_t accelerometerInfo) {
  this->currentTime = time;
  this->deviceAddress = deviceAddress;
  this->accelerometerInfo = accelerometerInfo;

  activeBlockLogicalIndex = ACTIVITY_BLOCK_INVALID;
  activeFile = -1;
  isInitialized = false;

  InitialFileScan();
}

#endif
