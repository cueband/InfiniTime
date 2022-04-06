#include "cueband.h"

#ifdef CUEBAND_INFO_APP_ENABLED

#include "InfoApp.h"
#include <lvgl/lvgl.h>
#include "../DisplayApp.h"
#include "Symbols.h"

#ifdef CUEBAND_INFO_APP_ID
#include "components/qrtiny/qrtiny.h"
#endif

using namespace Pinetime::Applications::Screens;

static void lv_update_task(struct _lv_task_t* task) {
  auto user_data = static_cast<InfoApp*>(task->user_data);
  user_data->Update();
}

InfoApp::InfoApp(Pinetime::Applications::DisplayApp* app,
             System::SystemTask& systemTask,
             Pinetime::Controllers::DateTime& dateTimeController,
             Controllers::MotionController& motionController,
             Controllers::Settings& settingsController
#ifdef CUEBAND_ACTIVITY_ENABLED
             , Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
             , Controllers::CueController& cueController
#endif
             )
  : Screen(app), systemTask {systemTask}, dateTimeController {dateTimeController}, motionController {motionController}, settingsController {settingsController}
#ifdef CUEBAND_ACTIVITY_ENABLED
  , activityController {activityController}
#endif
#ifdef CUEBAND_CUE_ENABLED
  , cueController {cueController}
#endif
   {

  lInfo = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_recolor(lInfo, true);
  lv_label_set_text_fmt(lInfo, "-"); //lv_label_set_text_fmt(lInfo, "%li", infoCount);
  lv_obj_align(lInfo, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);
  lv_label_set_long_mode(lInfo, LV_LABEL_LONG_EXPAND);

#ifdef CUEBAND_INFO_APP_ID
  std::array<uint8_t, 6> bleAddr = systemTask.GetBleController().Address();
  sprintf(longAddress, "%02x:%02x:%02x:%02x:%02x:%02x", bleAddr[5], bleAddr[4], bleAddr[3], bleAddr[2], bleAddr[1], bleAddr[0]);
  sprintf(shortAddress, "%02X%02X%02X%02X%02X%02X", bleAddr[5], bleAddr[4], bleAddr[3], bleAddr[2], bleAddr[1], bleAddr[0]);

  // Use a (26 byte) buffer for holding the encoded payload and ECC calculations
  uint8_t qrBuffer[QRTINY_BUFFER_SIZE] = { 0 };

  // Encode one or more segments text to the buffer
  size_t payloadLength = 0;
  payloadLength += QrTinyWriteAlphanumeric(qrBuffer, payloadLength, shortAddress);

  // Choose a format for the QR Code: a mask pattern (binary `000` to `111`) and an error correction level (`LOW`, `MEDIUM`, `QUARTILE`, `HIGH`).
  uint16_t formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_QUARTILE;  // Max alphanumeric: High=10, Quartile=16, Medium=20, Low=26 

  // Compute the remaining buffer contents: any required padding and the calculated error-correction information
  bool result = QrTinyGenerate(qrBuffer, payloadLength, formatInfo);

  // Clear image data
  memset(data_qr, 0, sizeof(data_qr));

  // Set image palette entries
  data_qr[0] = 0x00; data_qr[1] = 0x00; data_qr[2] = 0x00; data_qr[3] = 0xff; // Color of index 0
  data_qr[4] = 0xff; data_qr[5] = 0xff; data_qr[6] = 0xff; data_qr[7] = 0xff; // Color of index 1

  // Set image pixels from QR code
  if (result) {
    for (int y = 0; y < QR_IMAGE_DIMENSION; y++) {
      for (int x = 0; x < QR_IMAGE_DIMENSION; x++) {
        int module = QrTinyModuleGet(qrBuffer, formatInfo, x - QRTINY_QUIET_STANDARD, y - QRTINY_QUIET_STANDARD);
        int ofs = QR_IMAGE_PALETTE + ((y * QR_IMAGE_DIMENSION) >> 3) + (x >> 3);
        int mask = 1 << (7 - (x & 0x07));
        if (module) {
          data_qr[ofs] &= ~mask;  // 1=dark (swapped if inverted)
        } else {
          data_qr[ofs] |= mask;   // 0=light (swapped if inverted)
        }
      }
    }
  }

  // Set image header
  image_qr.header.always_zero = 0;
  image_qr.header.w = QR_IMAGE_DIMENSION;
  image_qr.header.h = QR_IMAGE_DIMENSION;
  image_qr.data_size = QR_IMAGE_SIZE;
  image_qr.header.cf = LV_IMG_CF_INDEXED_1BIT;
  image_qr.data = data_qr;

  // Create image object
  qr_obj = lv_img_create(lv_scr_act(), NULL);
  lv_img_set_src(qr_obj, &image_qr);
  lv_img_set_antialias(qr_obj, false);
  lv_img_set_zoom(qr_obj, 4*256);   // 256=normal, 512=double
  lv_obj_align(qr_obj, NULL, LV_ALIGN_CENTER, 0, 0);
#endif

  taskUpdate = lv_task_create(lv_update_task, 250, LV_TASK_PRIO_LOW, this);
  Update();
}

InfoApp::~InfoApp() {
  lv_task_del(taskUpdate);
  lv_obj_clean(lv_scr_act());
}

int InfoApp::ScreenCount() {
  int screenCount = 0;
#ifdef CUEBAND_INFO_APP_ID
  screenCount++;  // id (address etc)
#endif
  screenCount++;  // info
#ifdef CUEBAND_ACTIVITY_ENABLED
  screenCount++;  // activity debug - basic
  #ifdef CUEBAND_DEBUG_ACTIVITY
    screenCount++;  // activity debug - additional
  #endif
#endif
#ifdef CUEBAND_DEBUG_ADV
  screenCount++;  // advertising debug
#endif
  return screenCount;
}


extern "C" uint32_t os_cputime_get32(void);

void InfoApp::Update() {
  int thisScreen = 0;

  static char debugText[200];
  sprintf(debugText, "%s", "");

#ifdef CUEBAND_INFO_APP_ID
  if (screen == thisScreen++) {
    int battery = systemTask.GetBatteryController().PercentRemaining();
    sprintf(debugText, "%s [%d%%]\n%s", CUEBAND_INFO_SYSTEM + 1, battery, longAddress);
    lv_obj_set_hidden(qr_obj, false);
  } else {
    lv_obj_set_hidden(qr_obj, true);
  }
#endif

  if (screen == thisScreen++) {
    uint32_t uptime = dateTimeController.Uptime().count();
    uint32_t uptimeSeconds = uptime % 60;
    uint32_t uptimeMinutes = (uptime / 60) % 60;
    uint32_t uptimeHours = (uptime / (60 * 60)) % 24;
    uint32_t uptimeDays = uptime / (60 * 60 * 24);

    //uint32_t tmr = os_cputime_get32();

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
            "#444444 Uptime# %lud %02lu:%02lu:%02lu\n"
            //"T@%02x_%02x_%02x_%02x\n"
            ,
            Version::Major(), Version::Minor(), Version::Patch(),
            Version::GitCommitHash(),
            __DATE__, __TIME__,
            uptimeDays, uptimeHours, uptimeMinutes, uptimeSeconds
            //, (uint8_t)(tmr >> 24), (uint8_t)(tmr >> 16), (uint8_t)(tmr >> 8), (uint8_t)(tmr >> 0)
            );
  }

#ifdef CUEBAND_CUE_ENABLED
  if (screen == thisScreen++) {
    char *p = debugText;
#if defined(CUEBAND_TRUSTED_CONNECTION)
    // Connection overview
    p += sprintf(p, "BLE: %s %s\n", 
      systemTask.nimble().GetBleController().IsConnected() ? "con" : "n/c", 
      systemTask.nimble().GetBleController().IsTrusted() ? "trust" : "untrust"
    );
#endif    
    cueController.DebugText(p);   // basic info
  }
#endif

#ifdef CUEBAND_ACTIVITY_ENABLED
  if (screen == thisScreen++) {
    activityController.DebugText(debugText, false);   // basic info
  }
  #ifdef CUEBAND_DEBUG_ACTIVITY
    if (screen == thisScreen++) {
      activityController.DebugText(debugText, true);    // additional info
    }
  #endif
#endif

#ifdef CUEBAND_DEBUG_ADV
  if (screen == thisScreen++) {
    systemTask.nimble().DebugText(debugText);
  }
#endif

  lv_label_set_text_fmt(lInfo, "%s", debugText);
}

void InfoApp::Refresh() {
  ;
}

bool InfoApp::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
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
