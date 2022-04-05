#include "drivers/Bma421.h"
#include <libraries/delay/nrf_delay.h>
#include <libraries/log/nrf_log.h>
#include "drivers/TwiMaster.h"
#include <drivers/Bma421_C/bma423.h>

using namespace Pinetime::Drivers;

#ifdef CUEBAND_FIFO_ENABLED
// Static storage buffers for the FIFO buffer
uint8_t Bma421::fifo_buff[CUEBAND_FIFO_BUFFER_LENGTH];
#endif

#ifdef CUEBAND_BUFFER_ENABLED
// Static storage buffers for the accelerometer data
int16_t Bma421::accel_buffer[CUEBAND_SAMPLE_MAX * CUEBAND_AXES];
#endif

namespace {
  int8_t user_i2c_read(uint8_t reg_addr, uint8_t* reg_data, uint32_t length, void* intf_ptr) {
    auto bma421 = static_cast<Bma421*>(intf_ptr);
    bma421->Read(reg_addr, reg_data, length);
    return 0;
  }

  int8_t user_i2c_write(uint8_t reg_addr, const uint8_t* reg_data, uint32_t length, void* intf_ptr) {
    auto bma421 = static_cast<Bma421*>(intf_ptr);
    bma421->Write(reg_addr, reg_data, length);
    return 0;
  }

  void user_delay(uint32_t period_us, void* intf_ptr) {
    nrf_delay_us(period_us);
  }
}

Bma421::Bma421(TwiMaster& twiMaster, uint8_t twiAddress) : twiMaster {twiMaster}, deviceAddress {twiAddress} {
  bma.intf = BMA4_I2C_INTF;
  bma.bus_read = user_i2c_read;
  bma.bus_write = user_i2c_write;
  bma.variant = BMA42X_VARIANT;
  bma.intf_ptr = this;
  bma.delay_us = user_delay;
  bma.read_write_len = 16;
}

void Bma421::Init() {
  if (not isResetOk)
    return; // Call SoftReset (and reset TWI device) first!

  auto ret = bma423_init(&bma);
  if (ret != BMA4_OK)
    return;

  switch(bma.chip_id) {
    case BMA423_CHIP_ID: deviceType = DeviceTypes::BMA421; break;
    case BMA425_CHIP_ID: deviceType = DeviceTypes::BMA425; break;
    default: deviceType = DeviceTypes::Unknown; break;
  }

  ret = bma423_write_config_file(&bma);
  if (ret != BMA4_OK)
    return;

  ret = bma4_set_interrupt_mode(BMA4_LATCH_MODE, &bma);
  if (ret != BMA4_OK)
    return;

  ret = bma423_feature_enable(BMA423_STEP_CNTR, 1, &bma);
  if (ret != BMA4_OK)
    return;

  ret = bma423_step_detector_enable(0, &bma);
  if (ret != BMA4_OK)
    return;

  ret = bma4_set_accel_enable(1, &bma);
  if (ret != BMA4_OK)
    return;

  struct bma4_accel_config accel_conf;
#ifdef CUEBAND_BUFFER_ENABLED
  #if (CUEBAND_BUFFER_SAMPLE_RATE == 50)
    accel_conf.odr = BMA4_OUTPUT_DATA_RATE_50HZ;
  #else
    #error "Unhandled rate"
  #endif

  #if (CUEBAND_BUFFER_SAMPLE_RANGE == 8)
    accel_conf.range = BMA4_ACCEL_RANGE_8G;
  #else
    #error "Unhandled range"
  #endif
#else // Use original values
  accel_conf.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
  accel_conf.range = BMA4_ACCEL_RANGE_2G;
#endif
  accel_conf.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
  accel_conf.perf_mode = BMA4_CIC_AVG_MODE;
  ret = bma4_set_accel_config(&accel_conf, &bma);
  if (ret != BMA4_OK)
    return;

#ifdef CUEBAND_FIFO_ENABLED
  // Initialize hardware FIFO
  ret = bma4_set_advance_power_save(BMA4_DISABLE, &bma); // Disable advanced power save mode for FIFO
  if (ret != BMA4_OK) return;
  ret = bma4_set_fifo_config(BMA4_FIFO_ALL, BMA4_DISABLE, &bma); // Clear FIFO config
  if (ret != BMA4_OK) return;
  ret = bma4_set_fifo_config(BMA4_FIFO_ACCEL, BMA4_ENABLE, &bma); // (BMA4_FIFO_ACCEL | BMA4_FIFO_STOP_ON_FULL | BMA4_FIFO_HEADER) Configure FIFO
  if (ret != BMA4_OK) return;

  // Initialize FIFO buffer
  memset(&fifo_frame, 0, sizeof(fifo_frame));
  fifo_frame.data = fifo_buff;
  fifo_frame.length = sizeof(fifo_buff);
#endif

#ifdef CUEBAND_BUFFER_ENABLED
  totalSamples = 0;
  totalDropped = 0;
  lastCount = 0;
#endif

  isOk = true;
}

void Bma421::Reset() {
  uint8_t data = 0xb6;
  twiMaster.Write(deviceAddress, 0x7E, &data, 1);
}

void Bma421::Read(uint8_t registerAddress, uint8_t* buffer, size_t size) {
  twiMaster.Read(deviceAddress, registerAddress, buffer, size);
}

void Bma421::Write(uint8_t registerAddress, const uint8_t* data, size_t size) {
  twiMaster.Write(deviceAddress, registerAddress, data, size);
}

Bma421::Values Bma421::Process() {
  if (not isOk)
    return {};
#ifdef CUEBAND_FIFO_ENABLED
  // TODO: Fix properly!  A quick hack to make this static if using FIFO, so the old value can be retained even if no new data was read
  static struct bma4_accel data;
#else
  struct bma4_accel data;
#endif

#ifdef CUEBAND_BUFFER_ENABLED

#ifdef CUEBAND_FIFO_ENABLED
  int8_t ret;

  // Read the FIFO length
  uint16_t fifo_length = 0;
  ret = bma4_get_fifo_length(&fifo_length, &bma);
  if (ret == BMA4_OK && fifo_length > 0) {
    // Adjust the frame length to match (up to the buffer size)
    if (fifo_length <= sizeof(fifo_buff)) {
      fifo_frame.length = fifo_length;
    } else {
      fifo_frame.length = sizeof(fifo_buff);
    }

    // Read FIFO (relies on `frame` members .data/.length, internally calls reset_fifo_data_structure(), sets .fifo_header_enable/.fifo_data_enable)
    ret = bma4_read_fifo_data(&fifo_frame, &bma);
    if (ret == BMA4_OK) {
      // Extract from frames in chunks
      const uint16_t chunkMax = CUEBAND_SAMPLE_MAX;
      struct bma4_accel accel_data[chunkMax];
      size_t written = 0;
      size_t dropped = 0;
      // While there is still data left in the FIFO
      for (;;) {  
        // Extract the next chunk from the frames
        uint16_t chunkSize = chunkMax;
        ret = bma4_extract_accel(accel_data, &chunkSize, &fifo_frame, &bma);
        if (ret != BMA4_OK) break;

        // Use: accel_data[0..count].{x|y|z}
        for (uint16_t i = 0; i < chunkSize; i++) {
          if (written < sizeof(accel_buffer) / sizeof(accel_buffer[0] * CUEBAND_AXES)) {
            // Scale 12-bit data to 16-bit width
            accel_buffer[CUEBAND_AXES * written + 0] = accel_data[i].x << 4;
            accel_buffer[CUEBAND_AXES * written + 1] = accel_data[i].y << 4;
            accel_buffer[CUEBAND_AXES * written + 2] = accel_data[i].z << 4;
            written++;
          } else {
            dropped++;
          }
        }

        // Last chunk will be non-full
        if (chunkSize < chunkMax) break;
      }
      lastCount = written;
      totalSamples += lastCount;
      totalDropped += dropped;
    }
  }

#else  // Single-sample buffer
  bma4_read_accel_xyz(&data, &bma);

  // Place in (single sample) buffer
  // Scale buffer as if 16-bit
  accel_data[0].x = data.x << 4;
  accel_data[0].y = data.y << 4;
  accel_data[0].z = data.z << 4;
  
  lastCount = 1;
  totalSamples += lastCount;
#endif

  // Scale last-used data to match original values (although not clipping to same range) and treat as single sample value
  if (lastCount > 0) {
    #ifdef CUEBAND_BUFFER_COMPATIBLE_SAMPLE_FIRST
      const int idx = 0;
    #else
      const int idx = lastCount - 1;
    #endif
    data.x = accel_buffer[CUEBAND_AXES * idx + 0] * CUEBAND_ORIGINAL_SCALE / CUEBAND_BUFFER_12BIT_SCALE / 16;
    data.y = accel_buffer[CUEBAND_AXES * idx + 1] * CUEBAND_ORIGINAL_SCALE / CUEBAND_BUFFER_12BIT_SCALE / 16;
    data.z = accel_buffer[CUEBAND_AXES * idx + 2] * CUEBAND_ORIGINAL_SCALE / CUEBAND_BUFFER_12BIT_SCALE / 16;
  }

#else // Original single-sample
  bma4_read_accel_xyz(&data, &bma);
#endif

  uint32_t steps = 0;
  bma423_step_counter_output(&steps, &bma);

#if !defined(CUEBAND_DONT_READ_UNUSED_ACCELEROMETER_VALUES) || defined(CUEBAND_MOTION_INCLUDE_TEMPERATURE)
  int32_t temperature;
  bma4_get_temperature(&temperature, BMA4_DEG, &bma);
#if !defined(CUEBAND_MOTION_INCLUDE_TEMPERATURE)  // Leave at full scale
  temperature = temperature / 1000;
#endif
#endif

#ifndef CUEBAND_DONT_READ_UNUSED_ACCELEROMETER_VALUES
  uint8_t activity = 0;
  bma423_activity_output(&activity, &bma);
#endif

  // X and Y axis are swapped because of the way the sensor is mounted in the PineTime
  return {steps, data.y, data.x, data.z
#ifdef CUEBAND_MOTION_INCLUDE_TEMPERATURE
  , temperature
#endif
  };
}
bool Bma421::IsOk() const {
  return isOk;
}

void Bma421::ResetStepCounter() {
  bma423_reset_step_counter(&bma);
}

void Bma421::SoftReset() {
  auto ret = bma4_soft_reset(&bma);
  if (ret == BMA4_OK) {
    isResetOk = true;
    nrf_delay_ms(1);
  }
}
Bma421::DeviceTypes Bma421::DeviceType() const {
  return deviceType;
}

#ifdef CUEBAND_BUFFER_ENABLED
void Bma421::GetBufferData(int16_t **accelValues, unsigned int *lastCount, unsigned int *totalSamples) {
  *accelValues = this->accel_buffer;
  *lastCount = this->lastCount;
  *totalSamples = this->totalSamples;
  return;
}
#endif
