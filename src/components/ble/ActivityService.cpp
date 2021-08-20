// Activity Sync Service
// Dan Jackson, 2021

#include "cueband.h"

#ifdef CUEBAND_ACTIVITY_ENABLED

#include "ActivityService.h"

#include "systemtask/SystemTask.h"

#define MAX_PACKET 20

int ActivityCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
  auto ActivityService = static_cast<Pinetime::Controllers::ActivityService*>(arg);
  return ActivityService->OnCommand(conn_handle, attr_handle, ctxt);
}

Pinetime::Controllers::ActivityService::ActivityService(Pinetime::System::SystemTask& system,
        Controllers::Settings& settingsController,
        Pinetime::Controllers::ActivityController& activityController
    ) : m_system(system),
    settingsController {settingsController},
    activityController {activityController}
    {

    characteristicDefinition[0] = {
        .uuid = (ble_uuid_t*) (&activityStatusCharUuid), 
        .access_cb = ActivityCallback, 
        .arg = this, 
        .flags = BLE_GATT_CHR_F_READ,
        .val_handle = &statusHandle
    };
    characteristicDefinition[1] = {
        .uuid = (ble_uuid_t*) (&activityBlockIdCharUuid), 
        .access_cb = ActivityCallback, 
        .arg = this, 
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ
    };
    characteristicDefinition[2] = {
        .uuid = (ble_uuid_t*) (&activityBlockDataCharUuid),
        .access_cb = ActivityCallback,
        .arg = this,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &transmitHandle
    };
    characteristicDefinition[3] = {0};

    serviceDefinition[0] = {
        .type = BLE_GATT_SVC_TYPE_PRIMARY, 
        .uuid = (ble_uuid_t*) &activityUuid, 
        .characteristics = characteristicDefinition
    };
    serviceDefinition[1] = {0};
}

void Pinetime::Controllers::ActivityService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

void Pinetime::Controllers::ActivityService::Disconnect() {
  // Free resources
  readLogicalBlockIndex = ACTIVITY_BLOCK_INVALID;
  blockLength = 0;
  blockOffset = 0;
  free(blockBuffer);
  blockBuffer = nullptr;
  packetTransmitting = false;
}

bool Pinetime::Controllers::ActivityService::IsSending() {
    return blockBuffer != nullptr && blockLength != 0 && blockOffset < blockLength;
}

void Pinetime::Controllers::ActivityService::Idle() {
    SendNextPacket();
    if (readPending) {
        StartRead();
    }
}

void Pinetime::Controllers::ActivityService::SendNextPacket() {
// HACK: TxNotification not working?
packetTransmitting = false;
    if (IsSending() && !packetTransmitting) {
        size_t len = blockLength - blockOffset;
        if (len > MAX_PACKET) len = MAX_PACKET;
        auto* om = ble_hs_mbuf_from_flat(blockBuffer + blockOffset, len);
        if (ble_gattc_notify_custom(tx_conn_handle, transmitHandle, om) == 0) {
            packetTransmitting = true;
            blockOffset += len;
        }
    }
}

void Pinetime::Controllers::ActivityService::TxNotification(ble_gap_event* event) {
  // Transmission
  // event->notify_tx.attr_handle; // attribute handle
  // event->notify_tx.conn_handle; // connection handle
  // event->notify_tx.indication;  // 0=notification, 1=indication
  // event->notify_tx.status;      // 0=successful, BLE_HS_EDONE=indication ACK, BLE_HS_ETIMEOUT=indication ACK not received, other=error
  if (event->notify_tx.attr_handle == transmitHandle) {
      packetTransmitting = false;
      //tx_conn_handle = event->notify_tx.conn_handle;
      //SendNextPacket();
  }
}

void Pinetime::Controllers::ActivityService::StartRead() {
    if (!readPending) { return; }
    readPending = false;

    uint16_t len = ACTIVITY_BLOCK_SIZE;
    uint16_t prefix = sizeof(len);  // 2

    if (readLogicalBlockIndex == ACTIVITY_BLOCK_INVALID) { return; }

    if (blockBuffer == nullptr) {
        blockBuffer = (uint8_t *)malloc(prefix + len);
    }

    if (blockBuffer == nullptr || IsSending()) {
        len = 0;        // Error: memory failure or busy
    } else {
        if (!activityController.ReadLogicalBlock(readLogicalBlockIndex, blockBuffer + prefix)) {
            len = 0;    // read failure
// HACK: Temporary dummy data for out-of-range blocks
#if defined(CUEBAND_DEBUG_DUMMY_MISSING_BLOCKS)
len = ACTIVITY_BLOCK_SIZE;
for (int i = 0; i < len; i++) {
blockBuffer[i + prefix] = (uint8_t)i;
}
#endif
        }
    }

    if (len == 0) {
        // Send empty length
        auto* omLen = ble_hs_mbuf_from_flat(&len, sizeof(len));
        ble_gattc_notify_custom(tx_conn_handle, transmitHandle, omLen);
    } else {
        memcpy(blockBuffer, &len, sizeof(len));
        blockOffset = 0;
        blockLength = prefix + len;
//                SendNextPacket();

        readLogicalBlockIndex++;
    }

}

int Pinetime::Controllers::ActivityService::OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt) {

#ifdef CUEBAND_ACTIVITY_ENABLED
    m_system.CommsActivity();
#endif

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) { // Reading

        if (attr_handle == statusHandle) {
            uint8_t status[14];

            uint32_t earliestBlockId = activityController.EarliestLogicalBlock();
            status[0] = (uint8_t)(earliestBlockId >> 0);
            status[1] = (uint8_t)(earliestBlockId >> 8);
            status[2] = (uint8_t)(earliestBlockId >> 16);
            status[3] = (uint8_t)(earliestBlockId >> 24);

            uint32_t activeBlockId = activityController.ActiveLogicalBlock();
            status[4] = (uint8_t)(activeBlockId >> 0);
            status[5] = (uint8_t)(activeBlockId >> 8);
            status[6] = (uint8_t)(activeBlockId >> 16);
            status[7] = (uint8_t)(activeBlockId >> 24);

            uint16_t blockSize = activityController.BlockSize(); 
            status[8] = (uint8_t)(blockSize >> 0);
            status[9] = (uint8_t)(blockSize >> 8);

            uint16_t epochInterval = activityController.EpochInterval(); 
            status[10] = (uint8_t)(epochInterval >> 0);
            status[11] = (uint8_t)(epochInterval >> 8);

            uint16_t maxSamplesPerBlock = activityController.MaxSamplesPerBlock(); 
            status[12] = (uint8_t)(maxSamplesPerBlock >> 0);
            status[13] = (uint8_t)(maxSamplesPerBlock >> 8);

            int res = os_mbuf_append(ctxt->om, &status, sizeof(status));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        } else if (ble_uuid_cmp(ctxt->chr->uuid, (ble_uuid_t*) &activityBlockIdCharUuid) == 0) {
            int res = os_mbuf_append(ctxt->om, &readLogicalBlockIndex, sizeof(readLogicalBlockIndex));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    } if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) { // Writing
        size_t notifSize = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[notifSize];
        os_mbuf_copydata(ctxt->om, 0, notifSize, data);

        // Writing to the block id
        if (ble_uuid_cmp(ctxt->chr->uuid, (ble_uuid_t*) &activityBlockIdCharUuid) == 0) {
            readLogicalBlockIndex = activityController.ActiveLogicalBlock();
            if (notifSize >= 4) { 
                readLogicalBlockIndex = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            }
            // Trigger the response
            tx_conn_handle = conn_handle;
            readPending = true;
            // StartRead() is called in idle
        }

    }
    return 0;
}

#endif
