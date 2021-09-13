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

void CueBandApp::Update() {
#ifdef CUEBAND_ACTIVITY_ENABLED
  lv_label_set_text_fmt(lInfo, "%s", activityController.DebugText());
#endif
}

void CueBandApp::Refresh() {
  ;
}

#endif
