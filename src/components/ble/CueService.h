// Cue Schedule Configuration Service
// Dan Jackson, 2021-2022

#pragma once

#include "cueband.h" // Overall config for cue.band additions

#ifdef CUEBAND_CUE_ENABLED

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
#include "components/cue/CueController.h"

// faa20000-3a02-417d-90a7-23f4a9c6745f
#define CUE_SERVICE_UUID_BASE       { 0x5f, 0x74, 0xc6, 0xa9, 0xf4, 0x23, 0xa7, 0x90, 0x7d, 0x41, 0x02, 0x3a, 0x00, 0x00, 0xa2, 0xfa }

// faa20001-3a02-417d-90a7-23f4a9c6745f
#define CUE_SERVICE_UUID_STATUS     { 0x5f, 0x74, 0xc6, 0xa9, 0xf4, 0x23, 0xa7, 0x90, 0x7d, 0x41, 0x02, 0x3a, 0x01, 0x00, 0xa2, 0xfa }

// faa20002-3a02-417d-90a7-23f4a9c6745f
#define CUE_SERVICE_UUID_DATA       { 0x5f, 0x74, 0xc6, 0xa9, 0xf4, 0x23, 0xa7, 0x90, 0x7d, 0x41, 0x02, 0x3a, 0x02, 0x00, 0xa2, 0xfa }

namespace Pinetime {
  namespace System {
    class SystemTask;
  }
  namespace Controllers {

    class CueService {
    public:
      explicit CueService(Pinetime::System::SystemTask& system, 
        Controllers::Ble& bleController,
        Controllers::Settings& settingsController,
        Pinetime::Controllers::CueController& cueController
      );

      void Init();
      void Disconnect();

      int OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt);

    private:

      void ResetState();

      ble_uuid128_t cueUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = CUE_SERVICE_UUID_BASE};

      ble_uuid128_t cueStatusCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = CUE_SERVICE_UUID_STATUS};
      ble_uuid128_t cueDataCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = CUE_SERVICE_UUID_DATA};

      // For status_flags
      Pinetime::Controllers::FirmwareValidator firmwareValidator;

      struct ble_gatt_chr_def characteristicDefinition[3];
      struct ble_gatt_svc_def serviceDefinition[2];

      Pinetime::System::SystemTask& m_system;
      Pinetime::Controllers::Ble& bleController;
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::CueController& cueController;

      uint16_t statusHandle;
      uint16_t dataHandle;

      size_t readIndex = 0;

    };
  }
}

#endif
