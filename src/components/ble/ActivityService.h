// UART Service
// Dan Jackson, 2021

#pragma once

#include "cueband.h" // Overall config for cue.band additions

#ifdef CUEBAND_ACTIVITY_ENABLED

#include <cstdint>
#include <string>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_uuid.h>
#undef max
#undef min

#include "components/ble/BleController.h"
#include "components/settings/Settings.h"
#include "components/activity/ActivityController.h"
#include "../firmwarevalidator/FirmwareValidator.h"


// 0e1d0000-9d33-4e5e-aead-e062834bd8bb
#define ACTIVITY_SERVICE_UUID_BASE       { 0xbb, 0xd8, 0x4b, 0x83, 0x62, 0xe0, 0xad, 0xae, 0x5e, 0x4e, 0x33, 0x9d, 0x00, 0x00, 0x1d, 0x0e }

// 0e1d0001-9d33-4e5e-aead-e062834bd8bb
#define ACTIVITY_SERVICE_UUID_STATUS     { 0xbb, 0xd8, 0x4b, 0x83, 0x62, 0xe0, 0xad, 0xae, 0x5e, 0x4e, 0x33, 0x9d, 0x01, 0x00, 0x1d, 0x0e }

// 0e1d0002-9d33-4e5e-aead-e062834bd8bb
#define ACTIVITY_SERVICE_UUID_BLOCK_ID   { 0xbb, 0xd8, 0x4b, 0x83, 0x62, 0xe0, 0xad, 0xae, 0x5e, 0x4e, 0x33, 0x9d, 0x02, 0x00, 0x1d, 0x0e }

// 0e1d0003-9d33-4e5e-aead-e062834bd8bb
#define ACTIVITY_SERVICE_UUID_BLOCK_DATA { 0xbb, 0xd8, 0x4b, 0x83, 0x62, 0xe0, 0xad, 0xae, 0x5e, 0x4e, 0x33, 0x9d, 0x03, 0x00, 0x1d, 0x0e }

namespace Pinetime {
  namespace System {
    class SystemTask;
  }
  namespace Controllers {

    class ActivityService {
    public:
      explicit ActivityService(Pinetime::System::SystemTask& system, 
        Controllers::Settings& settingsController,
        Pinetime::Controllers::ActivityController& activityController
      );

      void Init();
      void Disconnect();
      void TxNotification(ble_gap_event* event);
      int OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt);

      bool IsSending();
      void Idle();

    private:
      ble_uuid128_t activityUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = ACTIVITY_SERVICE_UUID_BASE};

      ble_uuid128_t activityStatusCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = ACTIVITY_SERVICE_UUID_STATUS};
      ble_uuid128_t activityBlockIdCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = ACTIVITY_SERVICE_UUID_BLOCK_ID};
      ble_uuid128_t activityBlockDataCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = ACTIVITY_SERVICE_UUID_BLOCK_DATA};

      struct ble_gatt_chr_def characteristicDefinition[4];
      struct ble_gatt_svc_def serviceDefinition[2];

      Pinetime::System::SystemTask& m_system;
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::ActivityController& activityController;

      Pinetime::Controllers::FirmwareValidator firmwareValidator;

      uint16_t statusHandle;
      uint16_t transmitHandle;

      bool readPending = false;
      void StartRead();
      void SendNextPacket();

      uint32_t readLogicalBlockIndex = ACTIVITY_BLOCK_INVALID;
      uint8_t *blockBuffer = nullptr;
      size_t blockLength = 0;
      size_t blockOffset = 0;
      volatile bool packetTransmitting = false;
      uint16_t tx_conn_handle;

    };
  }
}

#endif
