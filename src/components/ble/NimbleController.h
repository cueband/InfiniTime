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
#include "components/ble/FSService.h"
#include "components/ble/HeartRateService.h"
#include "components/ble/ImmediateAlertService.h"
#include "components/ble/MusicService.h"
#include "components/ble/NavigationService.h"
#include "components/ble/ServiceDiscovery.h"
#include "components/ble/MotionService.h"
#include "components/ble/weather/WeatherService.h"
#include "components/fs/FS.h"

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
#include "components/ble/CueService.h"
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
                       Ble& bleController,
                       DateTime& dateTimeController,
                       NotificationManager& notificationManager,
                       Battery& batteryController,
                       Pinetime::Drivers::SpiNorFlash& spiNorFlash,
                       HeartRateController& heartRateController,
                       MotionController& motionController,
                       FS& fs
#ifdef CUEBAND_SERVICE_UART_ENABLED
                       , Controllers::Settings& settingsController
                       , MotorController& motorController
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
                       , ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
                       , CueController& cueController
#endif
);
      void Init();
      void StartAdvertising();
      int OnGAPEvent(ble_gap_event* event);
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
      };

      void EnableRadio();
      void DisableRadio();

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
#if defined(CUEBAND_TRUSTED_CONNECTION)
      Pinetime::Controllers::Ble& GetBleController() { return bleController; }
#endif
#if defined(CUEBAND_FIX_DFU_LARGE_PACKETS)
      Pinetime::Controllers::DfuService& GetDfuService() { return dfuService; }
#endif

    private:
      void PersistBond(struct ble_gap_conn_desc& desc);
      void RestoreBond();

#ifdef CUEBAND_DEVICE_NAME
      char deviceName[32] = CUEBAND_DEVICE_NAME;
#else
      static constexpr const char* deviceName = "InfiniTime";
#endif
      Pinetime::System::SystemTask& systemTask;
      Ble& bleController;
      DateTime& dateTimeController;
      Pinetime::Drivers::SpiNorFlash& spiNorFlash;
      FS& fs;
      DfuService dfuService;

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
      FSService fsService;
      ServiceDiscovery serviceDiscovery;
      
#ifdef CUEBAND_SERVICE_UART_ENABLED
      UartService uartService;
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      ActivityService activityService;
#endif
#ifdef CUEBAND_CUE_ENABLED
      CueService cueService;
#endif

      uint8_t addrType;
      uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE;
      uint8_t fastAdvCount = 0;
      uint8_t bondId[16] = {0};

      ble_uuid128_t dfuServiceUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00}};
    };

    static NimbleController* nptr;
  }
}
