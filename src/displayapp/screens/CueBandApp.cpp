#include "cueband.h"

#ifdef CUEBAND_APP_ENABLED

#include "CueBandApp.h"
#include <lvgl/lvgl.h>
#include "../DisplayApp.h"
#include "Symbols.h"

using namespace Pinetime::Applications::Screens;

static void lv_update_task(struct _lv_task_t* task) {
  auto user_data = static_cast<CueBandApp*>(task->user_data);
  user_data->Update();
}

CueBandApp::CueBandApp(Pinetime::Applications::DisplayApp* app,
             System::SystemTask& systemTask,
             Pinetime::Controllers::DateTime& dateTimeController,
             Controllers::MotionController& motionController,
             Controllers::Settings& settingsController
#ifdef CUEBAND_ACTIVITY_ENABLED
             , Controllers::ActivityController& activityController
#endif
             )
  : Screen(app), systemTask {systemTask}, dateTimeController {dateTimeController}, motionController {motionController}, settingsController {settingsController}
#ifdef CUEBAND_ACTIVITY_ENABLED
  , activityController {activityController}
#endif
   {

  lInfo = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_recolor(lInfo, true);
  lv_label_set_text_fmt(lInfo, "-"); //lv_label_set_text_fmt(lInfo, "%li", infoCount);
  lv_obj_align(lInfo, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);
  lv_label_set_long_mode(lInfo, LV_LABEL_LONG_EXPAND);

  taskUpdate = lv_task_create(lv_update_task, 250, LV_TASK_PRIO_LOW, this);
  Update();
}

CueBandApp::~CueBandApp() {
  lv_task_del(taskUpdate);
  lv_obj_clean(lv_scr_act());
}

int CueBandApp::ScreenCount() {
  int screenCount = 0;
  screenCount++;  // info
#ifdef CUEBAND_ACTIVITY_ENABLED
  screenCount++;  // activity debug
#endif
#ifdef CUEBAND_DEBUG_ADV
  screenCount++;  // advertising debug
#endif
  return screenCount;
}

void CueBandApp::Update() {
  int thisScreen = 0;

  static char debugText[200];
  sprintf(debugText, "-");

  if (screen == thisScreen++) {
    uint32_t uptime = dateTimeController.Uptime().count();
    uint32_t uptimeSeconds = uptime % 60;
    uint32_t uptimeMinutes = (uptime / 60) % 60;
    uint32_t uptimeHours = (uptime / (60 * 60)) % 24;
    uint32_t uptimeDays = uptime / (60 * 60 * 24);

    sprintf(debugText,    // ~165 bytes
            "#FFFF00 InfiniTime#\n\n"
            "#444444 Version# %ld.%ld.%ld\n"
            "#444444 Short Ref# %s\n"
            "#444444 Build date#\n"
            "%s %s\n"
            "#444444 Application#"     // CUEBAND_INFO_SYSTEM has "\n" prefix
#ifdef CUEBAND_INFO_SYSTEM
            CUEBAND_INFO_SYSTEM "\n"
#endif
            "\n"
            "#444444 Uptime# %lud %02lu:%02lu:%02lu\n"
            ,
            Version::Major(), Version::Minor(), Version::Patch(),
            Version::GitCommitHash(),
            __DATE__, __TIME__,
            uptimeDays, uptimeHours, uptimeMinutes, uptimeSeconds);
  }

#ifdef CUEBAND_ACTIVITY_ENABLED
  if (screen == thisScreen++) {
    activityController.DebugText(debugText);
  }
#endif

#ifdef CUEBAND_DEBUG_ADV
  if (screen == thisScreen++) {
    systemTask.nimble().DebugText(debugText);
  }
#endif

  lv_label_set_text_fmt(lInfo, "%s", debugText);
}

void CueBandApp::Refresh() {
  ;
}

bool CueBandApp::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  switch (event) {
    case Pinetime::Applications::TouchEvents::LongTap:
      screen++;
      if (screen >= ScreenCount()) screen = 0;
      return true;
    default:
      return false;
  }
}

#endif
