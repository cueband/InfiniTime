#include "cueband.h"
#include "UartService.h"

#include "NimbleController.h"
#include <hal/nrf_rtc.h>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_id.h>
#include <host/util/util.h>
#undef max
#undef min
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/datetime/DateTimeController.h"
#include "systemtask/SystemTask.h"

using namespace Pinetime::Controllers;


// --- temporary debugging info for advertising ---
#ifdef CUEBAND_DEBUG_ADV
  #define CUEBAND_DEBUG_ADV_NOT_SET 999

  // Event ordering
  volatile int debugAdvSequence = 0;

  // Init
  int debugAdvInitTime = -1;
  int debugAdvInitCount = 0;

  // Reset
  int debugAdvResetTime = -1;
  int debugAdvResetCount = 0;
  int debugAdvResetLastReason = CUEBAND_DEBUG_ADV_NOT_SET;

  // Sync
  int debugAdvSyncTime = -1;
  int debugAdvSyncCount = 0;
  // ble_hs_synced()

  // Connected
  // bleController.IsConnected()

  // Is advertising
  // ble_gap_adv_active()

  // StartAdvertising()
  // fastAdvCount
  int debugAdvLastStartTime = -1;
  int debugAdvCount = 0;
  int debugAdvLastStartResult = CUEBAND_DEBUG_ADV_NOT_SET;

  // Advertising complete
  int debugAdvCompleteTime = -1;
  int debugAdvCompleteCount = 0;
  int debugAdvCompleteLastReason = CUEBAND_DEBUG_ADV_NOT_SET;

  // Connect
  int debugAdvConnectTime = -1;
  int debugAdvConnectCount = 0;
  int debugAdvConnectLastStatus = CUEBAND_DEBUG_ADV_NOT_SET;

  // Disconnect
  int debugAdvDisconnectTime = -1;
  int debugAdvDisconnectCount = 0;
  int debugAdvDisconnectLastReason = CUEBAND_DEBUG_ADV_NOT_SET;

  // DEBUG: hal_timer
  struct my_nrf52_hal_timer {
      uint8_t tmr_enabled;
      uint8_t tmr_irq_num;
      uint8_t tmr_rtc;
      uint8_t tmr_pad;
      uint32_t tmr_cntr;
      uint32_t timer_isrs;
      uint32_t tmr_freq;
      void *tmr_reg;
      //TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_q;
  };
  extern struct my_nrf52_hal_timer nrf52_hal_timer5;
  static my_nrf52_hal_timer *bsptimer = &nrf52_hal_timer5;
  static NRF_RTC_Type *rtctimer = (NRF_RTC_Type *)bsptimer->tmr_reg;

  void NimbleController::DebugText(char *debugText) {
    char *p = debugText;

    p += sprintf(p, "I:@%d ##%d | %d\n", debugAdvInitTime, debugAdvInitCount, debugAdvSequence);
    p += sprintf(p, "R:@%d ##%d %d\n", debugAdvResetTime, debugAdvResetCount, debugAdvResetLastReason);
    p += sprintf(p, "S:@%d ##%d %s\n", debugAdvSyncTime, debugAdvSyncCount, ble_hs_synced() ? "t" : "f");
    p += sprintf(p, "+:a=%s c=%s/%s f=%d\n", ble_gap_adv_active() ? "t" : "f", (connectionHandle != BLE_HS_CONN_HANDLE_NONE) ? "t" : "f", bleController.IsConnected() ? "t" : "f", (int)fastAdvCount);
    p += sprintf(p, "A:@%d ##%d %d\n", debugAdvLastStartTime, debugAdvCount, debugAdvLastStartResult);
    p += sprintf(p, "a:@%d ##%d %d\n", debugAdvCompleteTime, debugAdvCompleteCount, debugAdvCompleteLastReason);
    p += sprintf(p, "C:@%d ##%d %d\n", debugAdvConnectTime, debugAdvConnectCount, debugAdvConnectLastStatus);
    p += sprintf(p, "D:@%d ##%d %d\n", debugAdvDisconnectTime, debugAdvDisconnectCount, debugAdvDisconnectLastReason);
#ifdef CUEBAND_USE_FULL_MTU
    p += sprintf(p, "m:%d/%d %s\n", (int16_t)GetMtu(), (int16_t)bleController.GetMtu(), 
      #ifdef CUEBAND_USE_FULL_MTU
        "t"
      #else
        "f"
      #endif
      );
#endif
    // rtctimer | NRF_RTC0
    uint32_t counter = NRF_RTC0->COUNTER;
    p += sprintf(p, "T:%02x_%02x%02x%02x E%d %d%d\n", (uint8_t)(bsptimer->tmr_cntr >> 24), (uint8_t)(counter >> 16), (uint8_t)(counter >> 8), (uint8_t)(counter), bsptimer->tmr_enabled, (int)(NRF_RTC0->EVENTS_OVRFLW), (int)(rtctimer->EVENTS_COMPARE[2]));
    p += sprintf(p, "i%u e%d %04x%04x\n", (unsigned int)bsptimer->timer_isrs, (int)(NRF_RTC0->EVTEN)?1:0, (uint16_t)(NRF_RTC0->CC[2] >> 16), (uint16_t)(NRF_RTC0->CC[2]));   // 2=NRF_RTC_TIMER_CC_INT

    return;
  }

#endif


NimbleController::NimbleController(Pinetime::System::SystemTask& systemTask,
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
                                   )
  : systemTask {systemTask},
    bleController {bleController},
    dateTimeController {dateTimeController},
    notificationManager {notificationManager},
    spiNorFlash {spiNorFlash},
    dfuService {systemTask, bleController, spiNorFlash},
    currentTimeClient {dateTimeController},
    anService {systemTask, notificationManager},
    alertNotificationClient {systemTask, notificationManager},
    currentTimeService {dateTimeController},
    musicService {systemTask},
    navService {systemTask},
    batteryInformationService {batteryController},
    immediateAlertService {systemTask, notificationManager},
    heartRateService {systemTask, heartRateController},
    motionService{systemTask, motionController},
#ifdef CUEBAND_SERVICE_UART_ENABLED
    uartService {
      systemTask, 
      bleController, 
      settingsController, 
      batteryController, 
      dateTimeController, 
      motorController, 
      motionController,
      heartRateController
#ifdef CUEBAND_ACTIVITY_ENABLED
      , activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
      , cueController
#endif
    },
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
    activityService {
      systemTask,
      bleController,
      settingsController,
      activityController
    },
#endif
    serviceDiscovery({&currentTimeClient, &alertNotificationClient}) {
}

void nimble_on_reset(int reason) {
  NRF_LOG_INFO("Resetting state; reason=%d\n", reason);
#ifdef CUEBAND_DEBUG_ADV
  debugAdvResetTime = debugAdvSequence++;
  debugAdvResetCount++;
  debugAdvResetLastReason = reason;
#endif
}

void nimble_on_sync(void) {
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    ASSERT(rc == 0);

#ifdef CUEBAND_DEBUG_ADV
    debugAdvSyncTime = debugAdvSequence++;
    debugAdvSyncCount++;
#endif
    nptr->StartAdvertising();
}

int GAPEventCallback(struct ble_gap_event* event, void* arg) {
  auto nimbleController = static_cast<NimbleController*>(arg);
  return nimbleController->OnGAPEvent(event);
}

void NimbleController::Init() {
  while (!ble_hs_synced()) {
  }

  nptr = this;
  ble_hs_cfg.reset_cb = nimble_on_reset;
  ble_hs_cfg.sync_cb = nimble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  deviceInformationService.Init();
  currentTimeClient.Init();
  currentTimeService.Init();
#ifndef CUEBAND_SERVICE_MUSIC_DISABLED
  musicService.Init();
#endif
#ifndef CUEBAND_SERVICE_NAV_DISABLED
  navService.Init();
#endif
  anService.Init();
  dfuService.Init();
  batteryInformationService.Init();
  immediateAlertService.Init();
#ifndef CUEBAND_SERVICE_HR_DISABLED
  heartRateService.Init();
#endif
  motionService.Init();

#ifdef CUEBAND_SERVICE_UART_ENABLED
  uartService.Init();
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
  activityService.Init();
#endif

  int rc;
  rc = ble_hs_util_ensure_addr(0);
  ASSERT(rc == 0);
  rc = ble_hs_id_infer_auto(0, &addrType);
  ASSERT(rc == 0);
  rc = ble_svc_gap_device_name_set(deviceName);
  ASSERT(rc == 0);
  rc = ble_svc_gap_device_appearance_set(0xC2);
  ASSERT(rc == 0);
  Pinetime::Controllers::Ble::BleAddress address;
  rc = ble_hs_id_copy_addr(addrType, address.data(), nullptr);
  ASSERT(rc == 0);
  bleController.AddressType((addrType == 0) ? Ble::AddressTypes::Public : Ble::AddressTypes::Random);
  bleController.Address(std::move(address));

#ifdef CUEBAND_DEVICE_NAME
  {
    // deviceName = "Prefix-######";
    strcpy(deviceName, CUEBAND_DEVICE_NAME);

    // addr_str = "A0B1C2D3E4F5"; 
    std::array<uint8_t, 6> addr = bleController.Address();        // using BleAddress = std::array<uint8_t, 6>;
    char addr_str[12 + 1];
    sprintf(addr_str, "%02X%02X%02X%02X%02X%02X", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    // Start at end of strings
    char *src = addr_str + strlen(addr_str);
    char *dst = deviceName + strlen(deviceName);

    // Copy over any trailing '#' characters with address digits
    while (src > addr_str && dst > deviceName && *(dst-1) == '#') {
      *(--dst) = *(--src);
    }

    // Reset the device name
    rc = ble_svc_gap_device_name_set(deviceName);
  }
#endif
#ifdef CUEBAND_SERIAL_ADDRESS
  {
    // addr_str = "a0b1c2d3e4f5"; 
    std::array<uint8_t, 6> addr = bleController.Address();        // using BleAddress = std::array<uint8_t, 6>;
    char addr_str[12 + 1];
    sprintf(addr_str, "%02x%02x%02x%02x%02x%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    deviceInformationService.SetAddress(addr_str);
  }
#endif

  rc = ble_gatts_start();
  ASSERT(rc == 0);

#ifdef CUEBAND_DEBUG_ADV
  debugAdvInitCount++;
  debugAdvInitTime = debugAdvSequence++;
#endif

  if (!ble_gap_adv_active() && !bleController.IsConnected())
    StartAdvertising();
}

void NimbleController::StartAdvertising() {
#ifdef CUEBAND_POLL_START_ADVERTISING
  // HACK: As this may be called from the NimBLE task, just set the flag...
  wantToStartAdvertising = true;
}

// HACK: ...this can be periodically polled from the main task.
void NimbleController::PollStartAdvertising() {
  // if (!wantToStartAdvertising && !ble_gap_adv_active() && !bleController.IsConnected()) wantToStartAdvertising = true;
  if (!wantToStartAdvertising) return;        // only process further if we want to start advertising
  if (bleController.IsConnected()) return;    // ignore while connected
  if (advertisingStartBackOff > 0) {          // back-off for N interations after failed attempt
    advertisingStartBackOff--;
    return;
  }
  // Going to attempt to start -- stop any existing advertising...
  if (ble_gap_adv_active()) {
    // return;
    ble_gap_adv_stop();
  }
  // ...now continue setting up advertising:
#endif
  int rc;

  /* set adv parameters */
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  /* advertising payload is split into advertising data and advertising
     response, because all data cannot fit into single packet; name of device
     is sent as response to scan request */
  struct ble_hs_adv_fields rsp_fields;

  /* fill all fields and parameters with zeros */
  memset(&adv_params, 0, sizeof(adv_params));
  memset(&fields, 0, sizeof(fields));
  memset(&rsp_fields, 0, sizeof(rsp_fields));

  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  /* fast advertise for 30 sec */
  if (fastAdvCount < 15) {
    adv_params.itvl_min = 32;
    adv_params.itvl_max = 47;
    fastAdvCount++;
  } else {
    adv_params.itvl_min = 1636;
    adv_params.itvl_max = 1651;
  }

  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
#ifdef CUEBAND_NO_ADV_RSP    // Optional test: do not split name into advertising response (name truncated, no UUID advertised)
  #warning "This build does not split the advertising response (name truncated, no UUID advertised)"
  fields.name_len = strlen(deviceName) > 8 ? 8 : strlen(deviceName);      // Truncate to 8 characters
  fields.name = (uint8_t *)deviceName;
  //fields.name += strlen(deviceName) > fields.name_len ? (strlen(deviceName) - fields.name_len) : 0;   // (Optional) Shift to the last 8 characters (rather than the first 8)
  fields.name_is_complete = 1;
#else
  fields.uuids128 = &dfuServiceUuid;
  fields.num_uuids128 = 1;
  fields.uuids128_is_complete = 1;
#endif
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  rsp_fields.name = (uint8_t*) deviceName;
  rsp_fields.name_len = strlen(deviceName);
  rsp_fields.name_is_complete = 1;

  rc = ble_gap_adv_set_fields(&fields);
  ASSERT(rc == 0);

#ifndef CUEBAND_NO_ADV_RSP    // Optional test: do not split name into advertising response
  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  ASSERT(rc == 0);
#endif

  rc = ble_gap_adv_start(addrType, NULL, 2000, &adv_params, GAPEventCallback, this);
  ASSERT(rc == 0);
#ifdef CUEBAND_DEBUG_ADV
  debugAdvCount++;
  debugAdvLastStartTime = debugAdvSequence++;
  debugAdvLastStartResult = rc;
#endif
#ifdef CUEBAND_POLL_START_ADVERTISING
  if (rc == 0) {  // Advertising started OK
    wantToStartAdvertising = false;
    advertisingStartBackOff = 0;
  } else {        // Otherwise, wait a few iterations before trying again
    advertisingStartBackOff = 10;
  }
#endif
}

int NimbleController::OnGAPEvent(ble_gap_event* event) {
  switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
      NRF_LOG_INFO("Advertising event : BLE_GAP_EVENT_ADV_COMPLETE");
      NRF_LOG_INFO("reason=%d; status=%d", event->adv_complete.reason, event->connect.status);
#ifdef CUEBAND_DEBUG_ADV
  debugAdvCompleteTime = debugAdvSequence++;
  debugAdvCompleteCount++;
  debugAdvCompleteLastReason = event->adv_complete.reason;
#endif
      StartAdvertising();
      break;

    case BLE_GAP_EVENT_CONNECT:
      NRF_LOG_INFO("Advertising event : BLE_GAP_EVENT_CONNECT");

      /* A new connection was established or a connection attempt failed. */
      NRF_LOG_INFO("connection %s; status=%d ", event->connect.status == 0 ? "established" : "failed", event->connect.status);
#ifdef CUEBAND_DEBUG_ADV
      debugAdvConnectTime = debugAdvSequence++;
      debugAdvConnectCount++;
      debugAdvConnectLastStatus = event->connect.status;
#endif

      if (event->connect.status != 0) {
        /* Connection failed; resume advertising. */
        currentTimeClient.Reset();
        alertNotificationClient.Reset();
        connectionHandle = BLE_HS_CONN_HANDLE_NONE;
        bleController.Disconnect();
        fastAdvCount = 0;
        StartAdvertising();
      } else {
        connectionHandle = event->connect.conn_handle;
        bleController.Connect();
        systemTask.PushMessage(Pinetime::System::Messages::BleConnected);
        // Service discovery is deferred via systemtask
      }
      break;

    case BLE_GAP_EVENT_DISCONNECT:
      NRF_LOG_INFO("Advertising event : BLE_GAP_EVENT_DISCONNECT");
      NRF_LOG_INFO("disconnect reason=%d", event->disconnect.reason);
#ifdef CUEBAND_DEBUG_ADV
      debugAdvDisconnectTime = debugAdvSequence++;
      debugAdvDisconnectCount++;
      debugAdvDisconnectLastReason = event->disconnect.reason;
#endif

      /* Connection terminated; resume advertising. */
      currentTimeClient.Reset();
      alertNotificationClient.Reset();
#ifdef CUEBAND_SERVICE_UART_ENABLED
      uartService.Disconnect();
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      activityService.Disconnect();
#endif
      connectionHandle = BLE_HS_CONN_HANDLE_NONE;
      bleController.Disconnect();
      fastAdvCount = 0;
      StartAdvertising();
      break;

    case BLE_GAP_EVENT_CONN_UPDATE:
      NRF_LOG_INFO("Advertising event : BLE_GAP_EVENT_CONN_UPDATE");
      /* The central has updated the connection parameters. */
      NRF_LOG_INFO("update status=%d ", event->conn_update.status);
      break;

    case BLE_GAP_EVENT_ENC_CHANGE:
      /* Encryption has been enabled or disabled for this connection. */
      NRF_LOG_INFO("encryption change event; status=%d ", event->enc_change.status);
      break;

    case BLE_GAP_EVENT_SUBSCRIBE:
      NRF_LOG_INFO("subscribe event; conn_handle=%d attr_handle=%d "
                   "reason=%d prevn=%d curn=%d previ=%d curi=???\n",
                   event->subscribe.conn_handle,
                   event->subscribe.attr_handle,
                   event->subscribe.reason,
                   event->subscribe.prev_notify,
                   event->subscribe.cur_notify,
                   event->subscribe.prev_indicate);

      if(event->subscribe.reason == BLE_GAP_SUBSCRIBE_REASON_TERM) {
        heartRateService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      }
      else if(event->subscribe.prev_notify == 0 && event->subscribe.cur_notify == 1) {
        heartRateService.SubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.SubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      }
      else if(event->subscribe.prev_notify == 1 && event->subscribe.cur_notify == 0) {
        heartRateService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      }
      break;

    case BLE_GAP_EVENT_MTU:
      NRF_LOG_INFO("mtu update event; conn_handle=%d cid=%d mtu=%d\n",
        event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
        bleController.SetMtu(event->mtu.value); // GetMtu()
#endif
      break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
      /* We already have a bond with the peer, but it is attempting to
       * establish a new secure link.  This app sacrifices security for
       * convenience: just throw away the old bond and accept the new link.
       */

      /* Delete the old bond. */
      struct ble_gap_conn_desc desc;
      ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      ble_store_util_delete_peer(&desc.peer_id_addr);

      /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
       * continue with the pairing operation.
       */
    }
      return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_NOTIFY_RX: {
      /* Peer sent us a notification or indication. */
      size_t notifSize = OS_MBUF_PKTLEN(event->notify_rx.om);

      NRF_LOG_INFO("received %s; conn_handle=%d attr_handle=%d "
                   "attr_len=%d",
                   event->notify_rx.indication ? "indication" : "notification",
                   event->notify_rx.conn_handle,
                   event->notify_rx.attr_handle,
                   notifSize);

      alertNotificationClient.OnNotification(event);
    } break;
      /* Attribute data is contained in event->notify_rx.attr_data. */

#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
    case BLE_GAP_EVENT_NOTIFY_TX: {
      // Transmission
      // event->notify_tx.attr_handle; // attribute handle
      // event->notify_tx.conn_handle; // connection handle
      // event->notify_tx.indication;  // 0=notification(unacknowledged), 1=indication(acknowledged)
      // event->notify_tx.status;      // 0=successful, BLE_HS_EDONE=indication ACK, BLE_HS_ETIMEOUT=indication ACK not received, other=error
#ifdef CUEBAND_SERVICE_UART_ENABLED
      uartService.TxNotification(event);
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      activityService.TxNotification(event);
#endif
      return 0;
    }
#endif

    default:
      //      NRF_LOG_INFO("Advertising event : %d", event->type);
      break;
  }
  return 0;
}

void NimbleController::StartDiscovery() {
  if (connectionHandle != BLE_HS_CONN_HANDLE_NONE) {
    serviceDiscovery.StartDiscovery(connectionHandle);
  }
}

uint16_t NimbleController::connHandle() {
  return connectionHandle;
}

void NimbleController::NotifyBatteryLevel(uint8_t level) {
  if (connectionHandle != BLE_HS_CONN_HANDLE_NONE) {
    batteryInformationService.NotifyBatteryLevel(connectionHandle, level);
  }
}

#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
// For usable payload, must subtract 1-byte opcode and 2-byte handle
size_t NimbleController::GetMtu() {   // default 23 bytes, -3 => 20 bytes usable; this config (?) up to 256 bytes, -3 => 253 bytes usable
  if (connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return 0; // 23;
  } else {
    return ble_att_mtu(connectionHandle);
  }
}
#endif

bool Pinetime::Controllers::NimbleController::IsSending() {
#ifdef CUEBAND_SERVICE_UART_ENABLED
  if (uartService.IsSending()) return true;
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
  if (activityService.IsSending()) return true;
#endif
  return false;
}

#ifdef CUEBAND_STREAM_ENABLED
bool Pinetime::Controllers::NimbleController::IsStreaming() {
  return uartService.IsStreaming();
}
// Handle streaming
bool Pinetime::Controllers::NimbleController::Stream() {
#ifdef CUEBAND_ACTIVITY_ENABLED
  activityService.Idle();
#endif
  bool isStreaming = uartService.Stream();
  uartService.Idle();
  return isStreaming;
}
#endif
