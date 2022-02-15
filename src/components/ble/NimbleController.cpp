#include "cueband.h"
#include "UartService.h"

#include "components/ble/NimbleController.h"
#include <cstring>

#include <hal/nrf_rtc.h>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_id.h>
#include <host/util/util.h>
#include <controller/ble_ll.h>
#include <controller/ble_hw.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#undef max
#undef min
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/datetime/DateTimeController.h"
#include "components/fs/FS.h"
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
                                   Controllers::MotionController& motionController,
                                   Controllers::FS& fs
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
    fs {fs},
    dfuService {systemTask, bleController, spiNorFlash},

    currentTimeClient {dateTimeController},
    anService {systemTask, notificationManager},
    alertNotificationClient {systemTask, notificationManager},
    currentTimeService {dateTimeController},
    musicService {systemTask},
    weatherService {systemTask, dateTimeController},
    navService {systemTask},
    batteryInformationService {batteryController},
    immediateAlertService {systemTask, notificationManager},
    heartRateService {systemTask, heartRateController},
    motionService {systemTask, motionController},
    serviceDiscovery({&currentTimeClient, &alertNotificationClient}),
    fsService {systemTask, fs}
#ifdef CUEBAND_SERVICE_UART_ENABLED
    , uartService {
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
    }
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
    , activityService {
      systemTask,
      bleController,
      settingsController,
      activityController
    }
#endif
#ifdef CUEBAND_CUE_ENABLED
    , cueService {
      systemTask,
      bleController,
      settingsController,
      cueController
    }
#endif
    {
}

void nimble_on_reset(int reason) {
  NRF_LOG_INFO("Nimble lost sync, resetting state; reason=%d", reason);
#ifdef CUEBAND_DEBUG_ADV
  debugAdvResetTime = debugAdvSequence++;
  debugAdvResetCount++;
  debugAdvResetLastReason = reason;
#endif
}

void nimble_on_sync(void) {
  int rc;

  NRF_LOG_INFO("Nimble is synced");

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
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  deviceInformationService.Init();
  currentTimeClient.Init();
  currentTimeService.Init();
#ifndef CUEBAND_SERVICE_MUSIC_DISABLED
  musicService.Init();
#endif
#ifndef CUEBAND_SERVICE_WEATHER_DISABLED
  weatherService.Init();
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
  fsService.Init();

#ifdef CUEBAND_SERVICE_UART_ENABLED
  uartService.Init();
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
  activityService.Init();
#endif
#ifdef CUEBAND_CUE_ENABLED
  cueService.Init();
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

  bleController.Address(std::move(address));
  switch (addrType) {
    case BLE_OWN_ADDR_PUBLIC:
      bleController.AddressType(Ble::AddressTypes::Public);
      break;
    case BLE_OWN_ADDR_RANDOM:
      bleController.AddressType(Ble::AddressTypes::Random);
      break;
    case BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT:
      bleController.AddressType(Ble::AddressTypes::RPA_Public);
      break;
    case BLE_OWN_ADDR_RPA_RANDOM_DEFAULT:
      bleController.AddressType(Ble::AddressTypes::RPA_Random);
      break;
  }

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

  RestoreBond();

  StartAdvertising();
}

void NimbleController::StartAdvertising() {
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  struct ble_hs_adv_fields rsp_fields;

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

  rsp_fields.name = reinterpret_cast<const uint8_t*>(deviceName);
  rsp_fields.name_len = strlen(deviceName);
  rsp_fields.name_is_complete = 1;

  int rc;
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
}

int NimbleController::OnGAPEvent(ble_gap_event* event) {
  switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
      NRF_LOG_INFO("Advertising event : BLE_GAP_EVENT_ADV_COMPLETE");
      NRF_LOG_INFO("reason=%d; status=%0X", event->adv_complete.reason, event->connect.status);
#ifdef CUEBAND_DEBUG_ADV
      debugAdvCompleteTime = debugAdvSequence++;
      debugAdvCompleteCount++;
      debugAdvCompleteLastReason = event->adv_complete.reason;
#endif
      StartAdvertising();
      break;

    case BLE_GAP_EVENT_CONNECT:
      /* A new connection was established or a connection attempt failed. */
      NRF_LOG_INFO("Connect event : BLE_GAP_EVENT_CONNECT");
      NRF_LOG_INFO("connection %s; status=%0X ", event->connect.status == 0 ? "established" : "failed", event->connect.status);
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
      /* Connection terminated; resume advertising. */
      NRF_LOG_INFO("Disconnect event : BLE_GAP_EVENT_DISCONNECT");
      NRF_LOG_INFO("disconnect reason=%d", event->disconnect.reason);
#ifdef CUEBAND_DEBUG_ADV
      debugAdvDisconnectTime = debugAdvSequence++;
      debugAdvDisconnectCount++;
      debugAdvDisconnectLastReason = event->disconnect.reason;
#endif

      if (event->disconnect.conn.sec_state.bonded) {
        PersistBond(event->disconnect.conn);
      }

      currentTimeClient.Reset();
      alertNotificationClient.Reset();
#ifdef CUEBAND_SERVICE_UART_ENABLED
      uartService.Disconnect();
#endif
#ifdef CUEBAND_ACTIVITY_ENABLED
      activityService.Disconnect();
#endif
#ifdef CUEBAND_CUE_ENABLED
      cueService.Disconnect();
#endif
      connectionHandle = BLE_HS_CONN_HANDLE_NONE;
      bleController.Disconnect();
      fastAdvCount = 0;
      StartAdvertising();
      break;

    case BLE_GAP_EVENT_CONN_UPDATE:
      /* The central has updated the connection parameters. */
      NRF_LOG_INFO("Update event : BLE_GAP_EVENT_CONN_UPDATE");
      NRF_LOG_INFO("update status=%0X ", event->conn_update.status);
      break;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
      /* The central has requested updated connection parameters */
      NRF_LOG_INFO("Update event : BLE_GAP_EVENT_CONN_UPDATE_REQ");
      NRF_LOG_INFO("update request : itvl_min=%d itvl_max=%d latency=%d supervision=%d",
                   event->conn_update_req.peer_params->itvl_min,
                   event->conn_update_req.peer_params->itvl_max,
                   event->conn_update_req.peer_params->latency,
                   event->conn_update_req.peer_params->supervision_timeout);
      break;

    case BLE_GAP_EVENT_ENC_CHANGE:
      /* Encryption has been enabled or disabled for this connection. */
      NRF_LOG_INFO("Security event : BLE_GAP_EVENT_ENC_CHANGE");
      NRF_LOG_INFO("encryption change event; status=%0X ", event->enc_change.status);

      if (event->enc_change.status == 0) {
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (desc.sec_state.bonded) {
          PersistBond(desc);
#if defined(CUEBAND_TRUSTED_CONNECTION)
          // Bonded connections are automatically trusted
          bleController.SetTrusted();
#endif
        }

        NRF_LOG_INFO("new state: encrypted=%d authenticated=%d bonded=%d key_size=%d",
                     desc.sec_state.encrypted,
                     desc.sec_state.authenticated,
                     desc.sec_state.bonded,
                     desc.sec_state.key_size);
      }
      break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
      /* Authentication has been requested for this connection.
       *
       * BLE authentication is determined by the combination of I/O capabilities
       * on the central and peripheral. When the peripheral is display only and
       * the central has a keyboard and display then passkey auth is selected.
       * When both the central and peripheral have displays and support yes/no
       * buttons then numeric comparison is selected. We currently advertise
       * display capability only so we only handle the "display" action here.
       *
       * Standards insist that the rand() PRNG be deterministic.
       * Use the tinycrypt prng here since rand() is predictable.
       */
      NRF_LOG_INFO("Security event : BLE_GAP_EVENT_PASSKEY_ACTION");
      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        struct ble_sm_io pkey = {0};
        pkey.action = event->passkey.params.action;

        /*
         * Passkey is a 6 digits code (1'000'000 possibilities).
         * It is important every possible value has an equal probability
         * of getting generated. Simply applying a modulo creates a bias
         * since 2^32 is not a multiple of 1'000'000.
         * To prevent that, we can reject values greater than 999'999.
         *
         * Rejecting values would happen a lot since 2^32-1 is way greater
         * than 1'000'000. An optimisation is to use a multiple of 1'000'000.
         * The greatest multiple of 1'000'000 lesser than 2^32-1 is
         * 4'294'000'000.
         *
         * Great explanation at:
         * https://research.kudelskisecurity.com/2020/07/28/the-definitive-guide-to-modulo-bias-and-how-to-avoid-it/
         */
        uint32_t passkey_rand;
        do {
          passkey_rand = ble_ll_rand();
        } while (passkey_rand > 4293999999);
        pkey.passkey = passkey_rand % 1000000;

        bleController.SetPairingKey(pkey.passkey);
        systemTask.PushMessage(Pinetime::System::Messages::OnPairing);
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      }
      break;

    case BLE_GAP_EVENT_SUBSCRIBE:
      NRF_LOG_INFO("Subscribe event; conn_handle=%d attr_handle=%d "
                   "reason=%d prevn=%d curn=%d previ=%d curi=???\n",
                   event->subscribe.conn_handle,
                   event->subscribe.attr_handle,
                   event->subscribe.reason,
                   event->subscribe.prev_notify,
                   event->subscribe.cur_notify,
                   event->subscribe.prev_indicate);

      if (event->subscribe.reason == BLE_GAP_SUBSCRIBE_REASON_TERM) {
        heartRateService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      } else if (event->subscribe.prev_notify == 0 && event->subscribe.cur_notify == 1) {
        heartRateService.SubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.SubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      } else if (event->subscribe.prev_notify == 1 && event->subscribe.cur_notify == 0) {
        heartRateService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
        motionService.UnsubscribeNotification(event->subscribe.conn_handle, event->subscribe.attr_handle);
      }
      break;

    case BLE_GAP_EVENT_MTU:
      NRF_LOG_INFO("MTU Update event; conn_handle=%d cid=%d mtu=%d", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
#if defined(CUEBAND_SERVICE_UART_ENABLED) || defined(CUEBAND_ACTIVITY_ENABLED)
        bleController.SetMtu(event->mtu.value); // GetMtu()
#endif
      break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
      NRF_LOG_INFO("Pairing event : BLE_GAP_EVENT_REPEAT_PAIRING");
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
      /* Attribute data is contained in event->notify_rx.attr_data. */
      NRF_LOG_INFO("Notify event : BLE_GAP_EVENT_NOTIFY_RX");
      size_t notifSize = OS_MBUF_PKTLEN(event->notify_rx.om);

      NRF_LOG_INFO("received %s; conn_handle=%d attr_handle=%d "
                   "attr_len=%d",
                   event->notify_rx.indication ? "indication" : "notification",
                   event->notify_rx.conn_handle,
                   event->notify_rx.attr_handle,
                   notifSize);

      alertNotificationClient.OnNotification(event);
    } break;

    case BLE_GAP_EVENT_NOTIFY_TX:
      NRF_LOG_INFO("Notify event : BLE_GAP_EVENT_NOTIFY_TX");
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
      break;

    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
      NRF_LOG_INFO("Identity event : BLE_GAP_EVENT_IDENTITY_RESOLVED");
      break;

    default:
      NRF_LOG_INFO("UNHANDLED GAP event : %d", event->type);
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

void NimbleController::PersistBond(struct ble_gap_conn_desc& desc) {
  union ble_store_key key;
  union ble_store_value our_sec, peer_sec, peer_cccd_set[MYNEWT_VAL(BLE_STORE_MAX_CCCDS)] = {0};
  int rc;

  memset(&key, 0, sizeof key);
  memset(&our_sec, 0, sizeof our_sec);
  key.sec.peer_addr = desc.peer_id_addr;
  rc = ble_store_read_our_sec(&key.sec, &our_sec.sec);

  if (memcmp(&our_sec.sec, &bondId, sizeof bondId) == 0) {
    return;
  }

  memcpy(&bondId, &our_sec.sec, sizeof bondId);

  memset(&key, 0, sizeof key);
  memset(&peer_sec, 0, sizeof peer_sec);
  key.sec.peer_addr = desc.peer_id_addr;
  rc += ble_store_read_peer_sec(&key.sec, &peer_sec.sec);

  if (rc == 0) {
    memset(&key, 0, sizeof key);
    key.cccd.peer_addr = desc.peer_id_addr;
    int peer_count = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_CCCD, &peer_count);
    for (int i = 0; i < peer_count; i++) {
      key.cccd.idx = peer_count;
      ble_store_read_cccd(&key.cccd, &peer_cccd_set[i].cccd);
    }

    /* Wakeup Spi and SpiNorFlash before accessing the file system
     * This should be fixed in the FS driver
     */
    systemTask.PushMessage(Pinetime::System::Messages::GoToRunning);
    systemTask.PushMessage(Pinetime::System::Messages::DisableSleeping);
    vTaskDelay(10);

    lfs_file_t file_p;

    rc = fs.FileOpen(&file_p, "/bond.dat", LFS_O_WRONLY | LFS_O_CREAT);
    if (rc == 0) {
      fs.FileWrite(&file_p, reinterpret_cast<uint8_t*>(&our_sec.sec), sizeof our_sec);
      fs.FileWrite(&file_p, reinterpret_cast<uint8_t*>(&peer_sec.sec), sizeof peer_sec);
      fs.FileWrite(&file_p, reinterpret_cast<const uint8_t*>(&peer_count), 1);
      for (int i = 0; i < peer_count; i++) {
        fs.FileWrite(&file_p, reinterpret_cast<uint8_t*>(&peer_cccd_set[i].cccd), sizeof(struct ble_store_value_cccd));
      }
      fs.FileClose(&file_p);
    }
    systemTask.PushMessage(Pinetime::System::Messages::EnableSleeping);
  }
}

void NimbleController::RestoreBond() {
  lfs_file_t file_p;
  union ble_store_value sec, cccd;
  uint8_t peer_count = 0;

  if (fs.FileOpen(&file_p, "/bond.dat", LFS_O_RDONLY) == 0) {
    memset(&sec, 0, sizeof sec);
    fs.FileRead(&file_p, reinterpret_cast<uint8_t*>(&sec.sec), sizeof sec);
    ble_store_write_our_sec(&sec.sec);

    memset(&sec, 0, sizeof sec);
    fs.FileRead(&file_p, reinterpret_cast<uint8_t*>(&sec.sec), sizeof sec);
    ble_store_write_peer_sec(&sec.sec);

    fs.FileRead(&file_p, &peer_count, 1);
    for (int i = 0; i < peer_count; i++) {
      fs.FileRead(&file_p, reinterpret_cast<uint8_t*>(&cccd.cccd), sizeof(struct ble_store_value_cccd));
      ble_store_write_cccd(&cccd.cccd);
    }

    fs.FileClose(&file_p);
    fs.FileDelete("/bond.dat");
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
