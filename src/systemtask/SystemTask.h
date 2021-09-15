#pragma once

#include "cueband.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <heartratetask/HeartRateTask.h>
#include <components/settings/Settings.h>
#include <drivers/Bma421.h>
#include <drivers/PinMap.h>
#include <components/motion/MotionController.h>

#include "SystemMonitor.h"
#include "components/battery/BatteryController.h"
#include "components/ble/NimbleController.h"
#include "components/ble/NotificationManager.h"
#include "components/motor/MotorController.h"
#include "components/timer/TimerController.h"
#include "components/fs/FS.h"
#include "touchhandler/TouchHandler.h"

#ifdef CUEBAND_ACTIVITY_ENABLED
#include "components/activity/ActivityController.h"
#endif
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

#ifdef PINETIME_IS_RECOVERY
  #include "displayapp/DisplayAppRecovery.h"
  #include "displayapp/DummyLittleVgl.h"
#else
  #include "components/settings/Settings.h"
  #include "displayapp/DisplayApp.h"
  #include "displayapp/LittleVgl.h"
#endif

#include "drivers/Watchdog.h"
#include "Messages.h"

namespace Pinetime {
  namespace Drivers {
    class Cst816S;
    class SpiMaster;
    class SpiNorFlash;
    class St7789;
    class TwiMaster;
    class Hrs3300;
  }
  namespace Controllers {
    class TouchHandler;
  }
  namespace System {
    class SystemTask {
    public:
      SystemTask(Drivers::SpiMaster& spi,
                 Drivers::St7789& lcd,
                 Pinetime::Drivers::SpiNorFlash& spiNorFlash,
                 Drivers::TwiMaster& twiMaster,
                 Drivers::Cst816S& touchPanel,
                 Components::LittleVgl& lvgl,
                 Controllers::Battery& batteryController,
                 Controllers::Ble& bleController,
                 Controllers::DateTime& dateTimeController,
                 Controllers::TimerController& timerController,
                 Drivers::Watchdog& watchdog,
                 Pinetime::Controllers::NotificationManager& notificationManager,
                 Pinetime::Controllers::MotorController& motorController,
                 Pinetime::Drivers::Hrs3300& heartRateSensor,
                 Pinetime::Controllers::MotionController& motionController,
                 Pinetime::Drivers::Bma421& motionSensor,
                 Controllers::Settings& settingsController,
                 Pinetime::Controllers::HeartRateController& heartRateController,
                 Pinetime::Applications::DisplayApp& displayApp,
                 Pinetime::Applications::HeartRateTask& heartRateApp,
                 Pinetime::Controllers::FS& fs,
                 Pinetime::Controllers::TouchHandler& touchHandler
#ifdef CUEBAND_ACTIVITY_ENABLED
                 , Pinetime::Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
                 , Pinetime::Controllers::CueController& cueController
#endif
                 );

      void Start();
      void PushMessage(Messages msg);

      void OnButtonPushed();
      void OnTouchEvent();

      void OnIdle();
      void OnDim();

      Pinetime::Controllers::NimbleController& nimble() {
        return nimbleController;
      };

      bool IsSleeping() const {
        return isSleeping;
      }

#ifdef CUEBAND_ACTIVITY_ENABLED
      void CommsActivity() {
            commsCount++;
      }
#endif

    private:
      TaskHandle_t taskHandle;

      Pinetime::Drivers::SpiMaster& spi;
      Pinetime::Drivers::St7789& lcd;
      Pinetime::Drivers::SpiNorFlash& spiNorFlash;
      Pinetime::Drivers::TwiMaster& twiMaster;
      Pinetime::Drivers::Cst816S& touchPanel;
      Pinetime::Components::LittleVgl& lvgl;
      Pinetime::Controllers::Battery& batteryController;

      Pinetime::Controllers::Ble& bleController;
      Pinetime::Controllers::DateTime& dateTimeController;
      Pinetime::Controllers::TimerController& timerController;
      QueueHandle_t systemTasksMsgQueue;
      std::atomic<bool> isSleeping {false};
      std::atomic<bool> isGoingToSleep {false};
      std::atomic<bool> isWakingUp {false};
      std::atomic<bool> isDimmed {false};
      Pinetime::Drivers::Watchdog& watchdog;
      Pinetime::Controllers::NotificationManager& notificationManager;
      Pinetime::Controllers::MotorController& motorController;
      Pinetime::Drivers::Hrs3300& heartRateSensor;
      Pinetime::Drivers::Bma421& motionSensor;
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::HeartRateController& heartRateController;
      Pinetime::Controllers::MotionController& motionController;

      Pinetime::Applications::DisplayApp& displayApp;
      Pinetime::Applications::HeartRateTask& heartRateApp;
      Pinetime::Controllers::FS& fs;
      Pinetime::Controllers::TouchHandler& touchHandler;
#ifdef CUEBAND_ACTIVITY_ENABLED
      Pinetime::Controllers::ActivityController& activityController;
#endif
#ifdef CUEBAND_CUE_ENABLED
      Pinetime::Controllers::CueController& cueController;
#endif
      Pinetime::Controllers::NimbleController nimbleController;

      static void Process(void* instance);
      void Work();
      void ReloadIdleTimer();
      bool isBleDiscoveryTimerRunning = false;
      uint8_t bleDiscoveryTimer = 0;
      TimerHandle_t dimTimer;
      TimerHandle_t idleTimer;
      TimerHandle_t measureBatteryTimer;
      bool sendBatteryNotification = false;
      bool doNotGoToSleep = false;

      void GoToRunning();
      void UpdateMotion();
      bool stepCounterMustBeReset = false;
      static constexpr TickType_t batteryMeasurementPeriod = pdMS_TO_TICKS(10 * 60 * 1000);
      TickType_t lastBatteryNotificationTime = 0;

#if defined(CUEBAND_CUE_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED) 
      int delayStart = CUEBAND_DELAY_START;
#endif

#ifdef CUEBAND_CUE_ENABLED
      uint32_t cueLastSecond = 0;
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      uint32_t activityLastSecond = 0;                // Last time activity updated

      #define CUEBAND_STEPS_INVALID 0xffffffff
      uint32_t currentSteps = CUEBAND_STEPS_INVALID;  // Current step counter
      uint32_t previousSteps = CUEBAND_STEPS_INVALID; // Last known step counter

      uint32_t interactionCount = 0;                  // Count of interaction events
      uint32_t lastInteractionCount = 0;              // Last recorded count of interaction events

      uint32_t commsCount = 0;                        // Count of communication event
      uint32_t lastCommsCount = 0;                    // Last recorded count of communication events

      uint8_t lastBattery = 0xff;                     // 0xff = unknown
      uint8_t lastTemperature = 0x80;                 // 0x80 = unknown

      bool latterEpoch = false;
      bool wasConnected = false;
      bool wasPowered = false;
      bool wasSleeping = false;
#endif

#if defined(CUEBAND_POLLED_ENABLED) || defined(CUEBAND_FIFO_ENABLED)
      bool IsSampling();
      uint32_t samplingTickIndex = 0;
#endif

#if configUSE_TRACE_FACILITY == 1
      SystemMonitor<FreeRtosMonitor> monitor;
#else
      SystemMonitor<DummyMonitor> monitor;
#endif
    };
  }
}
