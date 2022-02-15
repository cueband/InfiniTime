#include "components/ble/BleController.h"

using namespace Pinetime::Controllers;

#if defined(CUEBAND_TRUSTED_CONNECTION)
#if defined(__has_include)
#if __has_include("cueband_key.local.h")
#include "cueband_key.local.h"    // static const char *cueband_key = "...";
#else
#warning "cueband_key.local.h not found -- using empty key"
static const char *cueband_key = "";
#endif
#else
#warning "__has_include not supported -- using empty key"
static const char *cueband_key = "";
#endif

#include <string.h>
// Jenkins's "one_at_a_time" hash
static uint32_t hash(const uint8_t *data, size_t length) {
  uint32_t hash = 0;
  for (size_t i = 0; i < length; i++) {
    hash += data[i];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

static uint32_t GetResponse(uint32_t challenge) {
  uint8_t data[4 + 32];
  size_t length = 4;
  data[0] = (uint8_t)(challenge);
  data[1] = (uint8_t)(challenge >> 8);
  data[2] = (uint8_t)(challenge >> 16);
  data[3] = (uint8_t)(challenge >> 24);

  memcpy(data + length, cueband_key, strlen(cueband_key));
  length += strlen(cueband_key);

  return hash(data, length);
}

uint32_t Ble::GetChallenge() {
  std::array<uint8_t, 6> addr = this->Address();        // using BleAddress = std::array<uint8_t, 6>;
  uint8_t data[10];
  data[0] = (uint8_t)(connectedTime);
  data[1] = (uint8_t)(connectedTime >> 8);
  data[2] = (uint8_t)(connectedTime >> 16);
  data[3] = (uint8_t)(connectedTime >> 24);
  data[4] = (uint8_t)addr[0];
  data[5] = (uint8_t)addr[1];
  data[6] = (uint8_t)addr[2];
  data[7] = (uint8_t)addr[3];
  data[8] = (uint8_t)addr[4];
  data[9] = (uint8_t)addr[5];
  return hash(data, sizeof(data));
}

bool Ble::ProvideChallengeResponse(uint32_t response) {
  uint32_t challenge = GetChallenge();
  uint32_t compareResponse = GetResponse(challenge);
  if (response == compareResponse)
  {
    SetTrusted();
    return true;
  } else {
    return false;
  }
}
#endif

void Ble::Connect() {
  isConnected = true;
#if defined(CUEBAND_TRUSTED_CONNECTION)
  connectedElapsed = elapsed;
  connectedTime = now;
  trusted = false;
  bonded = false;
  if (trustSoonElapsed != 0xffffffff) {
    // Trust a connection within two minutes
    if (trustSoonElapsed + 2 * 60 < elapsed) {
      trusted = true;
    }
    // Do not trust additional connections
    trustSoonElapsed = 0xffffffff;
  }
#endif
}

void Ble::Disconnect() {
  isConnected = false;
#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
  SetMtu(23);   // reset to base MTU
#endif
#if defined(CUEBAND_TRUSTED_CONNECTION)
  trusted = false;
#endif
}

void Ble::StartFirmwareUpdate() {
  isFirmwareUpdating = true;
}

void Ble::StopFirmwareUpdate() {
  isFirmwareUpdating = false;
}

void Ble::FirmwareUpdateTotalBytes(uint32_t totalBytes) {
  firmwareUpdateTotalBytes = totalBytes;
}

void Ble::FirmwareUpdateCurrentBytes(uint32_t currentBytes) {
  firmwareUpdateCurrentBytes = currentBytes;
}
