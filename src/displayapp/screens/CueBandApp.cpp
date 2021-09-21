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
             Controllers::MotionController& motionController,
             Controllers::Settings& settingsController
#ifdef CUEBAND_ACTIVITY_ENABLED
             , Controllers::ActivityController& activityController
#endif
             )
  : Screen(app), systemTask {systemTask}, motionController {motionController}, settingsController {settingsController}
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

  if (screen == thisScreen++) {
    lv_label_set_text_fmt(lInfo,
                          "#FFFF00 InfiniTime#\n\n"
                          "#444444 Version# %ld.%ld.%ld\n"
                          "#444444 Short Ref# %s\n"
                          "#444444 Build date#\n"
                          "%s %s\n"
#ifdef CUEBAND_INFO_SYSTEM
                          CUEBAND_INFO_SYSTEM
#endif
                          ,
                          Version::Major(), Version::Minor(), Version::Patch(),
                          Version::GitCommitHash(),
                          __DATE__, __TIME__);
    return;
  }

#ifdef CUEBAND_ACTIVITY_ENABLED
  if (screen == thisScreen++) {
    const char * activityText = activityController.DebugText();
    lv_label_set_text_fmt(lInfo, "%s", activityText);
    return;
  }
#endif

#ifdef CUEBAND_DEBUG_ADV
  if (screen == thisScreen++) {
    const char * advDebugText = systemTask.nimble().DebugText();
    lv_label_set_text_fmt(lInfo, "%s", advDebugText);
    return;
  }
#endif

  lv_label_set_text_fmt(lInfo, "-");
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
