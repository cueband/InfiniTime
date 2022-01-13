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
             Controllers::Settings& settingsController
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
             )
  : Screen(app), systemTask {systemTask}, settingsController {settingsController}
#ifdef CUEBAND_CUE_ENABLED
  , cueController {cueController}
#endif
   {

  lInfo = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_recolor(lInfo, true);
  lv_label_set_text_fmt(lInfo, "-"); //lv_label_set_text_fmt(lInfo, "%li", infoCount);
  lv_obj_align(lInfo, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);
  lv_label_set_long_mode(lInfo, LV_LABEL_LONG_EXPAND);

  taskUpdate = lv_task_create(lv_update_task, 1000, LV_TASK_PRIO_LOW, this);
  Update();
}

CueBandApp::~CueBandApp() {
  lv_task_del(taskUpdate);
  lv_obj_clean(lv_scr_act());
}

void CueBandApp::Update() {
  static char text[200];
  char *p = text;
  *p = '\0';

  p += sprintf(text, "Cue.Band\n");
  p += sprintf(text, "\n");
  p += sprintf(text, "%s\n", cueController.Description());

  lv_label_set_text_fmt(lInfo, "%s", text);
}

void CueBandApp::Refresh() {
  ;
}

bool CueBandApp::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  switch (event) {
    case Pinetime::Applications::TouchEvents::LongTap:
      //return true;
      return false;
    default:
      return false;
  }
}

#endif
