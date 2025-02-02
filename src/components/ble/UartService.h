// UART Service
// Dan Jackson, 2021

#pragma once

#include "cueband.h" // Overall config for cue.band additions

#ifdef CUEBAND_SERVICE_UART_ENABLED

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

// Avoid circular dependency
//#include "components/battery/BatteryController.h"
namespace Pinetime {
  namespace Controllers {
    class Battery;
  }
}

#include "components/datetime/DateTimeController.h"
#include "components/motor/MotorController.h"
#include "components/motion/MotionController.h"
#include "components/heartrate/HeartRateController.h"
#include "../firmwarevalidator/FirmwareValidator.h"
#ifdef CUEBAND_ACTIVITY_ENABLED
#include "components/activity/ActivityController.h"
#endif
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif


// 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
#define UART_SERVICE_UUID_BASE { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E }

// 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
#define UART_SERVICE_UUID_RX { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E }

// 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
#define UART_SERVICE_UUID_TX { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E }

namespace Pinetime {
  namespace System {
    class SystemTask;
  }
  namespace Controllers {

    class UartService {
    public:
      explicit UartService(Pinetime::System::SystemTask& system, 
        Controllers::Ble& bleController,
        Controllers::Settings& settingsController,
        Controllers::Battery& batteryController,
        Controllers::DateTime& dateTimeController,
        Pinetime::Controllers::MotorController& motorController,
        Pinetime::Controllers::MotionController& motionController,
        Pinetime::Controllers::HeartRateController& heartRateController
#ifdef CUEBAND_ACTIVITY_ENABLED
        , Pinetime::Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
        , Pinetime::Controllers::CueController& cueController
#endif
      );

      void Init();
      void Disconnect();
      void TxNotification(ble_gap_event* event);
      int OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt);

#ifdef CUEBAND_STREAM_ENABLED
      bool IsStreaming();
      bool Stream();
#endif

      bool IsSending();
      void Idle();

      //std::string getLine();

#ifdef CUEBAND_LOG
      bool IsLogging() { return tx_conn_handle != BLE_HS_CONN_HANDLE_NONE && logging; }
      size_t Log(const char *data);
#endif

    private:
      ble_uuid128_t uartUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = UART_SERVICE_UUID_BASE};

      ble_uuid128_t uartRxCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = UART_SERVICE_UUID_RX};
      ble_uuid128_t uartTxCharUuid {.u = {.type = BLE_UUID_TYPE_128}, .value = UART_SERVICE_UUID_TX};

      struct ble_gatt_chr_def characteristicDefinition[3];
      struct ble_gatt_svc_def serviceDefinition[2];

      //std::string m_line;

      Pinetime::System::SystemTask& m_system;
      Pinetime::Controllers::Ble& bleController;
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::Battery& batteryController;
      Pinetime::Controllers::DateTime& dateTimeController;
      Pinetime::Controllers::MotorController& motorController;
      Pinetime::Controllers::MotionController& motionController;
      Pinetime::Controllers::HeartRateController& heartRateController;
#ifdef CUEBAND_ACTIVITY_ENABLED
      Pinetime::Controllers::ActivityController& activityController;
#endif
#ifdef CUEBAND_CUE_ENABLED
      Pinetime::Controllers::CueController& cueController;
#endif

      Pinetime::Controllers::FirmwareValidator firmwareValidator;

      uint16_t transmitHandle;

      void SendNextPacket();

      bool StreamAppend(const uint8_t *data, size_t length);
      bool StreamAppendString(const char *data);

      static uint8_t streamBuffer[];
      uint8_t *sendBuffer = nullptr;
      static const size_t sendCapacity = 512 + 32;
      volatile size_t blockLength = 0;
      volatile size_t blockOffset = 0;
      uint8_t *blockBuffer = nullptr;
      volatile bool packetTransmitting = false;
      uint16_t tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      unsigned int transmitErrorCount = 0;
#ifdef CUEBAND_LOG
      bool logging = false;
#endif

#ifdef CUEBAND_ACTIVITY_ENABLED
      uint32_t readLogicalBlockIndex = 0xffffffff;
#endif
#ifdef CUEBAND_STREAM_ENABLED
      void StopStreaming();
      bool StreamSamples(const int16_t *samples, size_t count);
      unsigned int lastTotalSamples = 0;

#ifdef CUEBAND_BUFFER_RAW_HR
      bool streamingHr = false;
      size_t hrCursor = 0;
#endif

      // Streaming
      bool streamFlag;
      int streamSampleIndex;    // -1=not started, 0=sent header, once >=25 send CRLF
      unsigned int streamOptions;
      unsigned int streamRawSampleCount;
      uint16_t streamConnectionHandle;
      TickType_t streamStartTicks;
#endif

    };
  }
}

#endif
