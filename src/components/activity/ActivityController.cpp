#include "cueband.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#include "ActivityController.h"

#define IIR_RATE ACTIVITY_RATE
#include "iir.h"

#include "components/fs/FS.h"

using namespace Pinetime::Controllers;

#define ACTIVITY_CONFIG_FILENAME "ACTIVITY.CFG"
#define ACTIVITY_CONFIG_MIN_VERSION 1
#define ACTIVITY_CONFIG_VERSION 1
#define ACTIVITY_CONFIG_SIZE (8 + 24)

#define ACTIVITY_DATA_FILENAME "ACTV%04d.BIN"

#ifdef CUEBAND_DEBUG_ACTIVITY
static struct {
  int16_t lastX, lastY, lastZ;
  uint32_t lastSumSquares;
  uint16_t lastSVM;
  int32_t lastSVMMO;
  uint16_t lastAbsSVMMO;
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
  int32_t lastFilteredSVMMO;
  int32_t lastAbsFilteredSVMMO;
#endif
} activity_debug_info;
#endif

ActivityController::ActivityController(Controllers::Settings& settingsController, Pinetime::Controllers::FS& fs
#ifdef CUEBAND_TRACK_MOTOR_TIMES
            , Pinetime::Controllers::DateTime& dateTimeController
            , Pinetime::Controllers::MotorController& motorController
#endif
#ifdef CUEBAND_HR_EPOCH
            , Pinetime::Controllers::HeartRateController& heartRateController
#endif
) : settingsController {settingsController}, fs {fs} 
#ifdef CUEBAND_TRACK_MOTOR_TIMES
            , dateTimeController { dateTimeController }
            , motorController { motorController }
#endif
#ifdef CUEBAND_HR_EPOCH
            , heartRateController { heartRateController }
#endif
{

  resampler_init(&this->resampler, CUEBAND_BUFFER_EFFECTIVE_RATE, ACTIVITY_RATE, 0, CUEBAND_AXES);
  InitConfig();
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



void ActivityController::InitConfig() {
    ChangeConfig(ACTIVITY_CONFIG_DEFAULT, ACTIVITY_CONFIG_DEFAULT, ACTIVITY_CONFIG_DEFAULT, ACTIVITY_CONFIG_DEFAULT);
    this->configChanged = 0;
}

int ActivityController::ReadConfig() {
    int ret;
    this->configChanged = 0;

    // Restore previous config
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, ACTIVITY_CONFIG_FILENAME, LFS_O_RDONLY);
    if (ret == LFS_ERR_CORRUPT) fs.FileDelete(ACTIVITY_CONFIG_FILENAME);    // No other sensible action?
    if (ret != LFS_ERR_OK) {
        return 1;
    }

    // Read header and config
    uint8_t buffer[ACTIVITY_CONFIG_SIZE];
    ret = fs.FileRead(&file_p, buffer, sizeof(buffer));
    if (ret != sizeof(buffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    // Parse header
    bool headerValid = (buffer[0] == 'A' && buffer[1] == 'C' && buffer[2] == 'T' && buffer[3] == 'V');
    unsigned int fileVersion = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
    if (!headerValid || fileVersion < ACTIVITY_CONFIG_MIN_VERSION || fileVersion > ACTIVITY_CONFIG_VERSION) {
        fs.FileClose(&file_p);
        return 3;
    }

    fs.FileClose(&file_p);

    // Config
    uint16_t format  = buffer[8] | (buffer[9] << 8);
    uint16_t epochInterval  = buffer[10] | (buffer[11] << 8);
    uint16_t hrmInterval  = buffer[12] | (buffer[13] << 8);
    uint16_t hrmDuration  = buffer[14] | (buffer[15] << 8);
    ChangeConfig(format, epochInterval, hrmInterval, hrmDuration);

    return 0;
}

void ActivityController::DeferWriteConfig() {
    if (this->configChanged == 0) {
        this->configChanged = 1;
    }
}

bool ActivityController::ChangeConfig(uint16_t format, uint16_t epochInterval, uint16_t hrmInterval, uint16_t hrmDuration) {

  // Default values
  if (format == ACTIVITY_CONFIG_DEFAULT) format = CUEBAND_FORMAT_VERSION;
  if (epochInterval == ACTIVITY_CONFIG_DEFAULT) epochInterval = CUEBAND_ACTIVITY_EPOCH_INTERVAL;
  if (hrmInterval == ACTIVITY_CONFIG_DEFAULT) hrmInterval = CUEBAND_ACTIVITY_HRM_INTERVAL_DEFAULT;
  if (hrmDuration == ACTIVITY_CONFIG_DEFAULT) hrmDuration = CUEBAND_ACTIVITY_HRM_DURATION_DEFAULT;

  // Only specific formats are valid
  if (format < CUEBAND_FORMAT_VERSION_MIN) format = CUEBAND_FORMAT_VERSION_MIN;
  if (format > CUEBAND_FORMAT_VERSION_MAX) format = CUEBAND_FORMAT_VERSION_MAX;

  // Changes -- user should call: DeferWriteConfig();
  bool changed = (format != this->format || epochInterval != this->epochInterval || hrmInterval != this->hrmInterval || hrmDuration != this->hrmDuration);

  this->format = format;
  this->epochInterval = epochInterval;
  this->hrmInterval = hrmInterval;
  this->hrmDuration = hrmDuration;

  return changed;
}

int ActivityController::WriteConfig() {
    int ret;
    if (!isInitialized) return 9;
    this->configChanged = 0;

    uint8_t buffer[ACTIVITY_CONFIG_SIZE];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'A'; buffer[1] = 'C'; buffer[2] = 'T'; buffer[3] = 'V'; 
    buffer[4] = (uint8_t)ACTIVITY_CONFIG_VERSION; buffer[5] = (uint8_t)(ACTIVITY_CONFIG_VERSION >> 8); buffer[6] = (uint8_t)(ACTIVITY_CONFIG_VERSION >> 16); buffer[7] = (uint8_t)(ACTIVITY_CONFIG_VERSION >> 24); 
    buffer[8] = (uint8_t)format; buffer[9] = (uint8_t)(format >> 8);
    buffer[10] = (uint8_t)epochInterval; buffer[11] = (uint8_t)(epochInterval >> 8);
    buffer[12] = (uint8_t)hrmInterval; buffer[13] = (uint8_t)(hrmInterval >> 8);
    buffer[14] = (uint8_t)hrmDuration; buffer[15] = (uint8_t)(hrmDuration >> 8);
    
    // Open control points file for writing
    lfs_file_t file_p = {0};
    ret = fs.FileOpen(&file_p, ACTIVITY_CONFIG_FILENAME, LFS_O_WRONLY|LFS_O_CREAT|LFS_O_TRUNC|LFS_O_APPEND);
    if (ret == LFS_ERR_CORRUPT) fs.FileDelete(ACTIVITY_CONFIG_FILENAME);    // No other sensible action?
    if (ret != LFS_ERR_OK) {
        return 1;
    }

    // Write header + buffer
    ret = fs.FileWrite(&file_p, buffer, sizeof(buffer));
    if (ret != sizeof(buffer)) {
        fs.FileClose(&file_p);
        return 2;
    }

    fs.FileClose(&file_p);

    return 0;
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

#ifdef CUEBAND_DETECT_FACE_DOWN
bool ActivityController::IsFaceDown() {
  return faceDownTime >= CUEBAND_DETECT_FACE_DOWN;
}
#endif
#ifdef CUEBAND_DETECT_WEAR_TIME
bool ActivityController::IsUnmovingActivity() {
  int axesUnmoving = 0;
  for (int chan = 0; chan < CUEBAND_AXES; chan++) {
    if (this->unmoving[chan] >= CUEBAND_DETECT_WEAR_TIME) axesUnmoving++;
  }
  return axesUnmoving >= 2;   // at least two of the axes
}
#endif

// Device interaction -- clear wear/face-down detections
void ActivityController::DeviceInteraction() {
#ifdef CUEBAND_DETECT_FACE_DOWN
  this->faceDownTime = 0;
#endif
#ifdef CUEBAND_DETECT_WEAR_TIME
  for (int chan = 0; chan < CUEBAND_AXES; chan++) {
    this->unmoving[chan] = 0;
  }
#endif
  return;
}

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

#ifdef CUEBAND_TRACK_MOTOR_TIMES
    uptime1024_t now = dateTimeController.Uptime1024();
    unsigned int sampleTicks = lastCount * 1024 / CUEBAND_BUFFER_EFFECTIVE_RATE;
    uptime1024_t firstSampleTime = (now < sampleTicks) ? 0 : (now - sampleTicks);
    uptime1024_t lastMovement = motorController.GetLastMovement();
    uptime1024_t overlap = (firstSampleTime < lastMovement) ? (lastMovement - firstSampleTime) : 0;

    // If some of this buffer overlaps the movement, discard the start of the buffer (up to all of it)
    if (overlap > 0 && overlap < (60 * CUEBAND_BUFFER_EFFECTIVE_RATE * 1024)) {    // Do not believe a large amount of overlap
      unsigned int skip = ((unsigned int)overlap * CUEBAND_BUFFER_EFFECTIVE_RATE / 1024);
      if (skip > lastCount) skip = lastCount;

#if (CUEBAND_DEBUG_TRACK_MOTOR_TIMES == 2 || CUEBAND_DEBUG_TRACK_MOTOR_TIMES == 3 || CUEBAND_DEBUG_TRACK_MOTOR_TIMES == 4)
      // (Optionally) clear streamed data and do not actually skip the data
      for (unsigned int i = 0; i < skip; i++) {
#if (CUEBAND_DEBUG_TRACK_MOTOR_TIMES == 3 || CUEBAND_DEBUG_TRACK_MOTOR_TIMES == 4)
        if ((i & 7) >= 7) continue;   // ...in a pattern to visualize overlap
#endif
        accelValues[CUEBAND_AXES * i + 0] = 0;
        accelValues[CUEBAND_AXES * i + 1] = 0;
        accelValues[CUEBAND_AXES * i + 2] = 0;
      }
#else
      accelValues += skip * CUEBAND_AXES;
      lastCount -= skip;
#endif
    }
#endif

#ifdef CUEBAND_ACTIVITY_STATS
    for (unsigned int i = 0; i < lastCount; i++) {

      // At 1 Hz
      if (this->statsIndex < 0 || this->statsIndex >= CUEBAND_BUFFER_EFFECTIVE_RATE) {
        // If we have data (not just the initial state)
        if (this->statsIndex >= 0) {
#ifdef CUEBAND_DETECT_FACE_DOWN
          // min/max of x < 0.4, y < 0.4, z > 0.9
          bool isFaceDown =    this->statsMin[0] <= CUEBAND_SCALE_MILLI_G(400) && this->statsMax[0] <= CUEBAND_SCALE_MILLI_G(400)   // x <= 0.4 g
                            && this->statsMin[1] <= CUEBAND_SCALE_MILLI_G(400) && this->statsMax[1] <= CUEBAND_SCALE_MILLI_G(400)   // y <= 0.4 g
                            && this->statsMin[2] >= CUEBAND_SCALE_MILLI_G(900) && this->statsMax[2] >= CUEBAND_SCALE_MILLI_G(900)   // z >= 0.9 g
                            ;
          if (isFaceDown) {
            this->faceDownTime++;
          } else {
            this->faceDownTime = 0;
          }
#endif
#ifdef CUEBAND_DETECT_WEAR_TIME
          // range <= 0.05 g for at least 2 out of the 3 axes
          for (int chan = 0; chan < CUEBAND_AXES; chan++) {
            bool unmoving = (this->statsMax[chan] - this->statsMin[chan]) <= CUEBAND_SCALE_MILLI_G(50);
            if (unmoving) {
              this->unmoving[chan]++;
            } else {
              this->unmoving[chan] = 0;
            }
          }
#endif
          ;
        }
        // Reset stats
        this->statsIndex = 0;
      }

      // Add current stats
      for (int chan = 0; chan < CUEBAND_AXES; chan++) {
        int value = accelValues[CUEBAND_AXES * i + chan];
        if (this->statsIndex == 0 || value < this->statsMin[chan]) this->statsMin[chan] = value;
        if (this->statsIndex == 0 || value > this->statsMax[chan]) this->statsMax[chan] = value;
      }
      
      this->statsIndex++;
    }
    
#endif

#if (CUEBAND_BUFFER_EFFECTIVE_RATE == ACTIVITY_RATE)
    #if defined(CUEBAND_STREAM_RESAMPLED)
      unsigned int count = (lastCount > ACTIVITY_RESAMPLE_BUFFER_SIZE) ? ACTIVITY_RESAMPLE_BUFFER_SIZE : lastCount;
      memcpy(this->outputBuffer, accelValues, count * CUEBAND_AXES * sizeof(accelValues[0]));
      this->lastCount = count;
      this->totalSamples += count;
    #endif
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

  // Each square is at most (-32768 * -32768 =) 1073741824; so the sum-of-squares is at most 3221225472
  sum_squares += (uint32_t)((int32_t)x * x);
  sum_squares += (uint32_t)((int32_t)y * y);
  sum_squares += (uint32_t)((int32_t)z * z);
  uint16_t svm = int_sqrt32(sum_squares); // sqrt is at most 56755.
  int32_t svmmo = (int32_t)svm - CUEBAND_BUFFER_16BIT_SCALE;  // at 1g=4096; in the interval (-4096, 52659)
  uint16_t abs_svmmo = (svmmo < 0) ? (uint16_t)-svmmo : (uint16_t)svmmo; // Absolute magnitude: abs(SVM-1), at 1g=4096, abs(svm-1) is at most 52659.

  // 60 second epoch at (ACTIVITY_RATE = 30/32/40/50) Hz;  Worst-case (50 Hz): gives (60*50=) 3000 samples; maximum sum of raw svm (60*50*56755=) 170265000 (28-bit).
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
  int32_t filtered_svmmo = iir_order2_highpass0_5(svmmo);
  int32_t abs_filtered_svmmo = (filtered_svmmo < 0) ? (int32_t)-filtered_svmmo : (int32_t)filtered_svmmo; // Absolute magnitude: abs(SVM-1)
  epochSumFilteredSvmMO += abs_filtered_svmmo;
#else
  epochSumSvm += svm;
#endif
  epochSumSvmMO += abs_svmmo;
  epochSumCount++;

#ifdef CUEBAND_DEBUG_ACTIVITY
  activity_debug_info.lastX = x;
  activity_debug_info.lastY = y;
  activity_debug_info.lastZ = z;
  activity_debug_info.lastSumSquares = sum_squares;
  activity_debug_info.lastSVM = svm;
  activity_debug_info.lastSVMMO = svmmo;
  activity_debug_info.lastAbsSVMMO = abs_svmmo;
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
  activity_debug_info.lastFilteredSVMMO = filtered_svmmo;
  activity_debug_info.lastAbsFilteredSVMMO = abs_filtered_svmmo;
#endif
#endif
}

void ActivityController::StartEpoch() {
  epochStartTime = currentTime;
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
  epochSumFilteredSvmMO = 0;
#else
  epochSumSvm = 0;
#endif
  epochSumSvmMO = 0;
  epochSumCount = 0;
  epochEvents = 0x0000;
  epochSteps = 0;
  epochPromptCount = 0;
  epochSnoozeMutedPromptCount = 0;
  epochUnwornMutedPromptCount = 0;
}

bool ActivityController::WriteEpoch() {
  if (countEpochs < ACTIVITY_MAX_SAMPLES)
  {
    uint8_t *data = activeBlock + ACTIVITY_HEADER_SIZE + (countEpochs * ACTIVITY_SAMPLE_SIZE);

    // `PPUUZZSS SSSSSSSS`
    uint16_t steps = 0;
    steps |= (epochPromptCount > 3 ? 3 : epochPromptCount) << 14;
    steps |= (epochUnwornMutedPromptCount > 3 ? 3 : epochUnwornMutedPromptCount) << 12;
    steps |= (epochSnoozeMutedPromptCount > 3 ? 3 : epochSnoozeMutedPromptCount) << 10;
    steps |= (epochSteps > 1023) ? 1023 : epochSteps;

    // @0 Event flags
    data[0] = (uint8_t)epochEvents; data[1] = (uint8_t)(epochEvents >> 8);

    // @2 Lower 10-bits: step count; next 2-bits: snooze-muted prompt count (0-3, saturates); next 2-bits: unworn-muted prompt count (0-3, saturates); top 2-bits: prompt count (0-3, saturates).
    data[2] = (uint8_t)steps; data[3] = (uint8_t)(steps >> 8);

    // Summary values
    uint16_t summary1 = 0xffff;
    uint16_t summary2 = 0xffff;
    
    // Calculate the mean SVM & SVMMO values
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
    uint32_t meanFilteredSvmMO;
#else
    uint32_t meanSvm;
#endif
    uint32_t meanSvmMO;
    if (epochSumCount >= ACTIVITY_RATE * epochInterval * 50 / 100) {
      // At least half of the expected samples were made, use the mean value.
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
      meanFilteredSvmMO = epochSumFilteredSvmMO / epochSumCount;
#else
      meanSvm = epochSumSvm / epochSumCount;
#endif
      meanSvmMO = epochSumSvmMO / epochSumCount;
      // Saturate/clip to maximum allowed
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
      if (meanFilteredSvmMO > 0xfffe) meanFilteredSvmMO = 0xfffe;
#else
      if (meanSvm > 0xfffe) meanSvm = 0xfffe;
#endif
      if (meanSvmMO > 0xfffe) meanSvmMO = 0xfffe;
    } else {
      // Less than half of the expected samples were made, use the invalid value
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
      meanFilteredSvmMO = 0xffff;
#else
      meanSvm = 0xffff;
#endif
      meanSvmMO = 0xffff;
    }

#ifdef CUEBAND_HR_EPOCH
debugMeanBpm = -1; debugDeltaMin = -1; debugDeltaMax = -1;
#endif

    // @4 Mean of the SVM values for the entire epoch
    if (format == CUEBAND_FORMAT_VERSION_ORIGINAL_ACTIVITY_0002) {
      #ifdef CUEBAND_ACTIVITY_HIGH_PASS
        summary1 = meanFilteredSvmMO;
      #else
        summary1 = meanSvm;
      #endif
    } else if (format == CUEBAND_FORMAT_VERSION_HR_RANGE_0003) {

#ifdef CUEBAND_HR_EPOCH
      // Get heart rate tracker stats and clear
      int meanBpm = -1, minBpm = -1, maxBpm = -1;
      int hrCount = heartRateController.HrStats(&meanBpm, &minBpm, &maxBpm, true);
      bool hasData = hrCount > 0;

      int deltaMin = 0, deltaMax = 0;
      if (hasData) {
        // Calculate delta
        deltaMin = meanBpm - minBpm;
        deltaMax = maxBpm - meanBpm;

        // Saturate
        if (meanBpm < 0) meanBpm = 0;
        if (meanBpm > 254) meanBpm = 254;
        if (deltaMin < 0x0) deltaMin = 0x0;
        if (deltaMin > 0xf) deltaMin = 0xf;
        if (deltaMax < 0x0) deltaMax = 0x0;
        if (deltaMax > 0xf) deltaMax = 0xf;
      } else {
        meanBpm = 0xff;
        deltaMin = 0xf;
        deltaMax = 0xf;
      }

debugMeanBpm = meanBpm; debugDeltaMin = deltaMin; debugDeltaMax = deltaMax;

      // heart_rate, XXXXNNNN MMMMMMMM
      // lowest 8-bits mean HR bpm (saturated to 254, 255=invalid)
      // next 4-bits minimum delta HR below mean (saturate to 15 bpm)
      // next 4-bits maximum delta HR above mean (saturate to 15 bpm).
      summary1 = (deltaMax << 12) | (deltaMin << 8) | meanBpm;
#else
      // Format reports HR, but HR sampling not enabled
      summary1 = 0xffff;
#endif

    } else {
      summary1 = 0xffff;
    }

    // Mean of the abs(SVM-1) values for the entire epoch
    if (format == CUEBAND_FORMAT_VERSION_ORIGINAL_ACTIVITY_0002) {
      summary2 = meanSvmMO;
    } else if (format == CUEBAND_FORMAT_VERSION_HR_RANGE_0003) {
      summary2 = meanSvmMO;
    } else {
      summary2 = 0xffff;
    }

    // @4 Summary1
    data[4] = (uint8_t)summary1; data[5] = (uint8_t)(summary1 >> 8);

    // @6 Summary2
    data[6] = (uint8_t)summary2; data[7] = (uint8_t)(summary2 >> 8);

    countEpochs++;
  }
  
  return true;
}

void ActivityController::TimeChanged(uint32_t time) {
  currentTime = time;

  if (!isInitialized) return;

#ifdef CUEBAND_HR_EPOCH
  hrWithinSampling = false;
  if (hrmInterval > 0) {
    hrEpochOffset = currentTime % hrmInterval;
    hrWithinSampling = (hrEpochOffset < hrmDuration);
  }
  heartRateController.SetHrEpoch(hrWithinSampling);
#endif

  if (epochInterval > 0) {
    uint32_t currentEpoch = epochStartTime / epochInterval;
    uint32_t epochNow = currentTime / epochInterval;
    // Epoch has changed from the current one
    if (epochNow != currentEpoch) {
      // Write this epoch
      WriteEpoch();

      uint32_t blockEpoch = blockStartTime / epochInterval;

      // If the block is full, or we're not in the correct sequence (e.g. the time changed)...
      if (countEpochs >= ACTIVITY_MAX_SAMPLES || epochNow - blockEpoch != countEpochs) {
        NewBlock();
      } else {
        StartEpoch();
      }
    }
  }

  // Config change debounce
  if (this->configChanged != 0) {
      if (++this->configChanged >= 10) {
          this->configChanged = 0;
          WriteConfig();
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

  // Erase config file and use default config
  fs.FileDelete(ACTIVITY_CONFIG_FILENAME);
  InitConfig();

  // Reset active block
  activeBlockLogicalIndex = 0;
  activeFile = 0;

  isInitialized = true;

  StartNewBlock();
}

bool ActivityController::FinalizeBlock(uint32_t logicalIndex) {
  // Header
  activeBlock[0] = 'A'; activeBlock[1] = 'D';                                                                           // @0  ASCII 'A' and 'D' as little-endian (= 0x4441)
  activeBlock[2] = (uint8_t)(ACTIVITY_BLOCK_SIZE - 4); activeBlock[3] = (uint8_t)((ACTIVITY_BLOCK_SIZE - 4) >> 8);      // @2  Bytes following the type/length (BLOCK_SIZE-4=252)
  activeBlock[4] = (uint8_t)format; activeBlock[5] = (uint8_t)(format >> 8);                                            // @4  0x00 = current format (8-bytes per sample)
  activeBlock[6] = (uint8_t)logicalIndex; activeBlock[7] = (uint8_t)(logicalIndex >> 8);                                // @6  Logical block identifier
  activeBlock[8] = (uint8_t)(logicalIndex >> 16); activeBlock[9] = (uint8_t)(logicalIndex >> 24);                       //     ...
  activeBlock[10] = deviceAddress[5]; activeBlock[11] = deviceAddress[4]; activeBlock[12] = deviceAddress[3];           // @10 Device ID (address)
  activeBlock[13] = deviceAddress[2]; activeBlock[14] = deviceAddress[1]; activeBlock[15] = deviceAddress[0];           //     ...
  activeBlock[16] = (uint8_t)blockStartTime; activeBlock[17] = (uint8_t)(blockStartTime >> 8);                          // @16 Seconds since epoch for the first sample
  activeBlock[18] = (uint8_t)(blockStartTime >> 16); activeBlock[19] = (uint8_t)(blockStartTime >> 24);                 //     ...
  activeBlock[20] = (uint8_t)(countEpochs > 255 ? 255 : countEpochs);                                                   // @20 Number of valid samples (up to 28 samples when 8-bytes each in a 256-byte block)
  activeBlock[21] = (uint8_t)(epochInterval > 255 ? 255 : epochInterval);                                               // @21 Epoch interval (seconds, = 60)

  if (format == CUEBAND_FORMAT_VERSION_ORIGINAL_ACTIVITY_0002) {
    activeBlock[22] = (uint8_t)promptConfigurationId; 
    activeBlock[23] = (uint8_t)(promptConfigurationId >> 8);            // @22 Active prompt configuration ID (may remove: this is just as a diagnostic as it can change during epoch)
    activeBlock[24] = (uint8_t)(promptConfigurationId >> 16); 
    activeBlock[25] = (uint8_t)(promptConfigurationId >> 24);   //     ...
  } else if (format == CUEBAND_FORMAT_VERSION_HR_RANGE_0003) {
    activeBlock[22] = hrmInterval > 255 ? 255 : hrmInterval;
    activeBlock[23] = hrmDuration > 255 ? 255 : hrmDuration;
    activeBlock[24] = 0xff;
    activeBlock[25] = 0xff;
  } else {
    activeBlock[22] = 0xff;
    activeBlock[23] = 0xff;
    activeBlock[24] = 0xff;
    activeBlock[25] = 0xff;
  }

  activeBlock[26] = lastBattery;                                                                                        // @26 Battery (0xff=unknown; bottom 7-bits: percentage, top-bit: power present)
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

void ActivityController::DebugTextConfig(char *debugText) {
  char *p = debugText;
  p += sprintf(p, "fmt:%04X e:%d\n", format, epochInterval);
  p += sprintf(p, "hr:%d/%d\n", hrmInterval, hrmDuration);
  p += sprintf(p, "samp:%s @%d\n", hrWithinSampling ? "t" : "f", (int)hrEpochOffset);
  
  // Get heart rate tracker stats and clear
  int meanBpm = -1, minBpm = -1, maxBpm = -1;
  bool hrCount = heartRateController.HrStats(&meanBpm, &minBpm, &maxBpm, false);
  p += sprintf(p, "data:%d bpm:%d\n", hrCount, meanBpm);
  p += sprintf(p, "min:%d max:%d\n", minBpm, maxBpm);

  // Last recorded
  p += sprintf(p, "last:%d -%d +%d\n", debugMeanBpm, debugDeltaMin, debugDeltaMax);
}

void ActivityController::DebugText(char *debugText, bool additionalInfo) {
  char *p = debugText;

  if (!additionalInfo) {

    p += sprintf(p, "bc:");
    for (int f = 0; f < CUEBAND_ACTIVITY_FILES; f++) {
      p += sprintf(p, "%s%ld", f > 0 ? "/" : "", meta[f].blockCount);
    }
    p += sprintf(p, "\n");

    //p += sprintf(p, "");
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
    
    p += sprintf(p, "Af:%d bc:%ld Al:%ld\n", activeFile, (activeFile < 0 ? -1 : meta[activeFile].blockCount), ActiveLogicalBlock());   // debug output as signed to spot invalid=-1
    p += sprintf(p, "I:%s BC:%ld F:%ld\n", isInitialized ? "t" : "f", BlockCount(), EarliestLogicalBlock());   // debug as signed to spot invalid=-1
    p += sprintf(p, "S:%lu E:%lu C:%lu\n", blockStartTime, countEpochs, epochSumCount);
    p += sprintf(p, "smo:%lu\n", epochSumSvmMO);
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
    p += sprintf(p, "i:%d fts:%lu\n", epochInterval, epochSumFilteredSvmMO);
#else
    p += sprintf(p, "i:%d sum:%lu\n", epochInterval, epochSumSvm);
#endif
    p += sprintf(p, "er:%lu/%lu/%lu ew:%lu/%lu/%lu\n", errRead, errReadLast, errReadLogicalLast, errWrite, errWriteLastInitial, errWriteLast);
    int elapsed = currentTime - epochStartTime;
    int estimatedRate = -1;
    if (elapsed > 0) estimatedRate = epochSumCount / elapsed;
    p += sprintf(p, "e:%d r:%d %ld/%ld", elapsed, estimatedRate, temp_transmit_count, temp_transmit_count_all);

  } else {  // additionalInfo

#ifdef CUEBAND_DEBUG_ACTIVITY
    p += sprintf(p, "#:%u e:%u/%u\n", (unsigned int)epochSumCount, (unsigned int)(currentTime - epochStartTime), (unsigned int)epochInterval);
    p += sprintf(p, "%+4d%+4d%+4d\n", activity_debug_info.lastX, activity_debug_info.lastY, activity_debug_info.lastZ);
    p += sprintf(p, "SSq:%u\n", (unsigned int)activity_debug_info.lastSumSquares);
    p += sprintf(p, "SVM:%u\n", (unsigned int)activity_debug_info.lastSVM);
    p += sprintf(p, "MO:%+d\n", (int)activity_debug_info.lastSVMMO);
    p += sprintf(p, "A:  %u\n", (unsigned int)activity_debug_info.lastAbsSVMMO);
    p += sprintf(p, "M:  %u\n", (unsigned int)(epochSumSvmMO / (epochSumCount > 0 ? epochSumCount : 1)));
#ifdef CUEBAND_ACTIVITY_HIGH_PASS
    p += sprintf(p, "Fl:%+d\n", (int)activity_debug_info.lastFilteredSVMMO);
    p += sprintf(p, "FA: %u\n", (unsigned int)activity_debug_info.lastAbsFilteredSVMMO);
    p += sprintf(p, "FM: %u\n", (unsigned int)(epochSumFilteredSvmMO / (epochSumCount > 0 ? epochSumCount : 1)));
#endif
#endif

  }
  return;
}

// New block required: if any epochs are stored, write block; begin new block.
void ActivityController::NewBlock() {
  if (!isInitialized) return;
  // If any epochs are stored
  if (countEpochs > 0) {
    // ...store to drive
    bool written = WriteActiveBlock();
    // Advance
    if (written) {
      activeBlockLogicalIndex++;
    } else {
      errWrite++;
    }
  }
  // Begin a new block
  StartNewBlock();
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

void ActivityController::PromptConfigurationChanged(uint32_t promptConfigurationId) {
  this->promptConfigurationId = promptConfigurationId;
}

void ActivityController::PromptGiven(bool muted, bool unworn) {
  if (muted) {
    if (unworn) {
      epochUnwornMutedPromptCount++;
    } else {
      epochSnoozeMutedPromptCount++;
    }
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

  ReadConfig();

  InitialFileScan();
}

#endif
