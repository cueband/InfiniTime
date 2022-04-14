#pragma once
#include "cueband.h"
#include <FreeRTOS.h>
#include <date/date.h>
#include <queue.h>
#include <task.h>
#include <memory>
#include <systemtask/Messages.h>
#include "displayapp/Apps.h"
#include "displayapp/LittleVgl.h"
#include "displayapp/TouchEvents.h"
#include "components/brightness/BrightnessController.h"
#include "components/motor/MotorController.h"
#include "components/firmwarevalidator/FirmwareValidator.h"
#include "components/settings/Settings.h"
#include "displayapp/screens/Screen.h"
#include "components/timer/TimerController.h"
#include "components/alarm/AlarmController.h"
#include "touchhandler/TouchHandler.h"

#include "displayapp/Messages.h"
#include "BootErrors.h"

#ifdef CUEBAND_ACTIVITY_ENABLED
#include "components/activity/ActivityController.h"
#endif
#ifdef CUEBAND_CUE_ENABLED
#include "components/cue/CueController.h"
#endif

namespace Pinetime {

  namespace Drivers {
    class St7789;
    class Cst816S;
    class WatchdogView;
  }
  namespace Controllers {
    class Settings;
    class Battery;
    class Ble;
    class DateTime;
    class NotificationManager;
    class HeartRateController;
    class MotionController;
    class TouchHandler;
  }

  namespace System {
    class SystemTask;
  };
  namespace Applications {
    class DisplayApp {
    public:
      enum class States { Idle, Running };
      enum class FullRefreshDirections { None, Up, Down, Left, Right, LeftAnim, RightAnim };

      DisplayApp(Drivers::St7789& lcd,
                 Components::LittleVgl& lvgl,
                 Drivers::Cst816S&,
                 Controllers::Battery& batteryController,
                 Controllers::Ble& bleController,
                 Controllers::DateTime& dateTimeController,
                 Drivers::WatchdogView& watchdog,
                 Pinetime::Controllers::NotificationManager& notificationManager,
                 Pinetime::Controllers::HeartRateController& heartRateController,
                 Controllers::Settings& settingsController,
                 Pinetime::Controllers::MotorController& motorController,
                 Pinetime::Controllers::MotionController& motionController,
                 Pinetime::Controllers::TimerController& timerController,
                 Pinetime::Controllers::AlarmController& alarmController,
                 Pinetime::Controllers::BrightnessController& brightnessController,
                 Pinetime::Controllers::TouchHandler& touchHandler
#if (defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_INFO_APP_ENABLED)) && defined(CUEBAND_ACTIVITY_ENABLED)
                 , Pinetime::Controllers::ActivityController& activityController
#endif
#if (defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_INFO_APP_ENABLED)) && defined(CUEBAND_CUE_ENABLED)
                 , Pinetime::Controllers::CueController& cueController
#endif
                 );
      void Start(System::BootErrors error);
      void PushMessage(Display::Messages msg);

      void StartApp(Apps app, DisplayApp::FullRefreshDirections direction);

      void SetFullRefresh(FullRefreshDirections direction);

      void Register(Pinetime::System::SystemTask* systemTask);

#if (defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_INFO_APP_ENABLED)) && defined(CUEBAND_CUE_ENABLED)
      Pinetime::Controllers::CueController& GetCueController() { return cueController; }
#endif
#ifdef CUEBAND_TRUSTED_CONNECTION
      Pinetime::Controllers::Ble& GetBleController() { return bleController; }
#endif
      // [cueband] HACK: No!
      Pinetime::System::SystemTask* GetSystemTask() { return systemTask; }

    private:
      Pinetime::Drivers::St7789& lcd;
      Pinetime::Components::LittleVgl& lvgl;
      Pinetime::Drivers::Cst816S& touchPanel;
      Pinetime::Controllers::Battery& batteryController;
      Pinetime::Controllers::Ble& bleController;
      Pinetime::Controllers::DateTime& dateTimeController;
      Pinetime::Drivers::WatchdogView& watchdog;
      Pinetime::System::SystemTask* systemTask = nullptr;
      Pinetime::Controllers::NotificationManager& notificationManager;
      Pinetime::Controllers::HeartRateController& heartRateController;
      Pinetime::Controllers::Settings& settingsController;
      Pinetime::Controllers::MotorController& motorController;
      Pinetime::Controllers::MotionController& motionController;
      Pinetime::Controllers::TimerController& timerController;
      Pinetime::Controllers::AlarmController& alarmController;
      Pinetime::Controllers::BrightnessController &brightnessController;
      Pinetime::Controllers::TouchHandler& touchHandler;
#if (defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_INFO_APP_ENABLED)) && defined(CUEBAND_ACTIVITY_ENABLED)
      Pinetime::Controllers::ActivityController& activityController;
#endif
#if (defined(CUEBAND_APP_ENABLED) || defined(CUEBAND_INFO_APP_ENABLED)) && defined(CUEBAND_CUE_ENABLED)
      Pinetime::Controllers::CueController& cueController;
#endif

      Pinetime::Controllers::FirmwareValidator validator;

      TaskHandle_t taskHandle;

      States state = States::Running;
      QueueHandle_t msgQueue;

      static constexpr uint8_t queueSize = 10;
      static constexpr uint8_t itemSize = 1;

      std::unique_ptr<Screens::Screen> currentScreen;

      Apps currentApp = Apps::None;
      Apps returnToApp = Apps::None;
      FullRefreshDirections returnDirection = FullRefreshDirections::None;
      TouchEvents returnTouchEvent = TouchEvents::None;

      TouchEvents GetGesture();
      static void Process(void* instance);
      void InitHw();
      void Refresh();
      void ReturnApp(Apps app, DisplayApp::FullRefreshDirections direction, TouchEvents touchEvent);
      void LoadApp(Apps app, DisplayApp::FullRefreshDirections direction);
      void PushMessageToSystemTask(Pinetime::System::Messages message);

      Apps nextApp = Apps::None;
      DisplayApp::FullRefreshDirections nextDirection;
      System::BootErrors bootError;
    };
  }
}
