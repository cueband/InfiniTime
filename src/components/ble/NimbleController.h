#pragma once

#include "cueband.h"

#include <cstdint>

#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#undef max
#undef min
#include "components/ble/AlertNotificationClient.h"
#include "components/ble/AlertNotificationService.h"
#include "components/ble/BatteryInformationService.h"
#include "components/ble/CurrentTimeClient.h"
#include "components/ble/CurrentTimeService.h"
#include "components/ble/DeviceInformationService.h"
#include "components/ble/DfuService.h"
#include "components/ble/ImmediateAlertService.h"
#include "components/ble/MusicService.h"
#include "components/ble/NavigationService.h"
#include "components/ble/ServiceDiscovery.h"
#include "components/ble/HeartRateService.h"
#include "components/ble/MotionService.h"
#include "components/ble/weather/WeatherService.h"

#ifdef CUEBAND_SERVICE_UART_ENABLED
#include "UartService.h"
#include "components/settings/Settings.h"
#include "components/motor/MotorController.h"
#include "components/motion/MotionController.h"
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
#include "components/activity/ActivityController.h"
#include "components/ble/ActivityService.h"
#endif
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

namespace Pinetime {
  namespace Drivers {
    class SpiNorFlash;
  }

  namespace System {
    class SystemTask;
  }

  namespace Controllers {
    class Ble;
    class DateTime;
    class NotificationManager;

    class NimbleController {

    public:
      NimbleController(Pinetime::System::SystemTask& systemTask,
                       Pinetime::Controllers::Ble& bleController,
                       DateTime& dateTimeController,
                       Pinetime::Controllers::NotificationManager& notificationManager,
                       Controllers::Battery& batteryController,
                       Pinetime::Drivers::SpiNorFlash& spiNorFlash,
                       Controllers::HeartRateController& heartRateController,
                       Controllers::MotionController& motionController
#ifdef CUEBAND_SERVICE_UART_ENABLED
                       , Controllers::Settings& settingsController
                       , Pinetime::Controllers::MotorController& motorController
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
                       , Pinetime::Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
                       , Pinetime::Controllers::CueController& cueController
#endif
                       );
      void Init();
      void StartAdvertising();
#ifdef CUEBAND_POLL_START_ADVERTISING
      void PollStartAdvertising();
#endif
      int OnGAPEvent(ble_gap_event* event);

      int OnDiscoveryEvent(uint16_t i, const ble_gatt_error* pError, const ble_gatt_svc* pSvc);
      int OnCTSCharacteristicDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_chr* characteristic);
      int OnANSCharacteristicDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_chr* characteristic);
      int OnCurrentTimeReadResult(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* attribute);
      int OnANSDescriptorDiscoveryEventCallback(uint16_t connectionHandle,
                                                const ble_gatt_error* error,
                                                uint16_t characteristicValueHandle,
                                                const ble_gatt_dsc* descriptor);

      void StartDiscovery();

      Pinetime::Controllers::MusicService& music() {
        return musicService;
      };
      Pinetime::Controllers::NavigationService& navigation() {
        return navService;
      };
      Pinetime::Controllers::AlertNotificationService& alertService() {
        return anService;
      };
      Pinetime::Controllers::WeatherService& weather() {
        return weatherService;
      };

      uint16_t connHandle();
      void NotifyBatteryLevel(uint8_t level);

      void RestartFastAdv() {
        fastAdvCount = 0;
      }

      bool IsSending();
#ifdef CUEBAND_STREAM_ENABLED
      bool IsStreaming();
      bool Stream();
#endif
#ifdef CUEBAND_DEBUG_ADV
      void DebugText(char *debugText);  // requires ~200 byte buffer
#endif
#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
      size_t GetMtu();
#endif
#ifdef CUEBAND_POLL_START_ADVERTISING
      volatile bool wantToStartAdvertising = false;
      volatile int advertisingStartBackOff = 0;
#endif

    private:
#ifdef CUEBAND_DEVICE_NAME
      char deviceName[32] = CUEBAND_DEVICE_NAME;
#else
      static constexpr const char* deviceName = "InfiniTime";
#endif
      Pinetime::System::SystemTask& systemTask;
      Pinetime::Controllers::Ble& bleController;
      DateTime& dateTimeController;
      Pinetime::Controllers::NotificationManager& notificationManager;
      Pinetime::Drivers::SpiNorFlash& spiNorFlash;
      Pinetime::Controllers::DfuService dfuService;

      DeviceInformationService deviceInformationService;
      CurrentTimeClient currentTimeClient;
      AlertNotificationService anService;
      AlertNotificationClient alertNotificationClient;
      CurrentTimeService currentTimeService;
      MusicService musicService;
      WeatherService weatherService;
      NavigationService navService;
      BatteryInformationService batteryInformationService;
      ImmediateAlertService immediateAlertService;
      HeartRateService heartRateService;
      MotionService motionService;
#ifdef CUEBAND_SERVICE_UART_ENABLED
      UartService uartService;
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      ActivityService activityService;
#endif

      uint8_t addrType; // 1 = Random, 0 = PUBLIC
      uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE;
      uint8_t fastAdvCount = 0;

      ble_uuid128_t dfuServiceUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00}};

      ServiceDiscovery serviceDiscovery;
    };

  static NimbleController* nptr;
  }
}
