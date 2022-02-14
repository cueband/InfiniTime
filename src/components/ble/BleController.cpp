#include "components/ble/BleController.h"

using namespace Pinetime::Controllers;

void Ble::Connect() {
  isConnected = true;
#if defined(CUEBAND_TRUSTED_CONNECTION)
  trusted = false;
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
