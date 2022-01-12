// Cue Schedule Configuration Service
// Dan Jackson, 2021-2022

#include "cueband.h"

#ifdef CUEBAND_CUE_ENABLED

#include "CueService.h"

#include "systemtask/SystemTask.h"

int CueCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
  auto CueService = static_cast<Pinetime::Controllers::CueService*>(arg);
  return CueService->OnCommand(conn_handle, attr_handle, ctxt);
}

Pinetime::Controllers::CueService::CueService(Pinetime::System::SystemTask& system,
        Controllers::Ble& bleController,
        Controllers::Settings& settingsController,
        Pinetime::Controllers::CueController& cueController
    ) : m_system(system),
    bleController {bleController},
    settingsController {settingsController},
    cueController {cueController}
    {

    characteristicDefinition[0] = {
        .uuid = (ble_uuid_t*) (&cueStatusCharUuid), 
        .access_cb = CueCallback, 
        .arg = this, 
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .val_handle = &statusHandle
    };
    characteristicDefinition[1] = {
        .uuid = (ble_uuid_t*) (&cueDataCharUuid), 
        .access_cb = CueCallback, 
        .arg = this, 
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .val_handle = &dataHandle
    };
    characteristicDefinition[2] = {0};

    serviceDefinition[0] = {
        .type = BLE_GATT_SVC_TYPE_PRIMARY, 
        .uuid = (ble_uuid_t*) &cueUuid, 
        .characteristics = characteristicDefinition
    };
    serviceDefinition[1] = {0};

    ResetState();
}

void Pinetime::Controllers::CueService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

void Pinetime::Controllers::CueService::ResetState() {
    // Reset state
    readIndex = 0;
}

void Pinetime::Controllers::CueService::Disconnect() {
    ResetState();
}

int Pinetime::Controllers::CueService::OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt) {

    m_system.CommsCue();

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) { // Reading

        if (attr_handle == statusHandle) {
            uint8_t status[15];
            
            // Reset read index
            readIndex = 0;

            // Status
            uint32_t active_schedule_id;
            uint16_t max_control_points;
            uint16_t current_control_point;
            uint16_t override_remaining;
            uint16_t intensity;
            uint16_t interval;
            uint16_t duration;
            cueController->GetStatus(&active_schedule_id, &max_control_points, &current_control_point, &override_remaining, &intensity, &interval, &duration);

            // @0 Active cue schedule ID
            status[0] = (uint8_t)(active_schedule_id >> 0);
            status[1] = (uint8_t)(active_schedule_id >> 8);
            status[2] = (uint8_t)(active_schedule_id >> 16);
            status[3] = (uint8_t)(active_schedule_id >> 24);

            // @4 Maximum number of control points supported
            status[4] = (uint8_t)(max_control_points >> 0);
            status[5] = (uint8_t)(max_control_points >> 8);

            // @6 Current active control point (0xffff=none)
            status[6] = (uint8_t)(current_control_point >> 0);
            status[7] = (uint8_t)(current_control_point >> 8);

            // @6 @8 (0=not overridden), remaining override duration (seconds, saturates to 0xffff)
            status[8] = (uint8_t)(override_remaining >> 0);
            status[9] = (uint8_t)(override_remaining >> 8);

            // @10 Effective cueing intensity
            status[10] = (uint8_t)(intensity >> 0);
            status[11] = (uint8_t)(intensity >> 8);

            // @12 Effective cueing interval (seconds)
            status[12] = (uint8_t)(interval >> 0);
            status[13] = (uint8_t)(interval >> 8);

            // @14 Effective remaining cueing duration (seconds, saturates to 0xffff)
            status[14] = (uint8_t)(duration >> 0);
            status[15] = (uint8_t)(duration >> 8);

            // @0 Active cue schedule ID
            uint32_t options = cueController->GetOptions();
            status[16] = (uint8_t)(options >> 0);
            status[17] = (uint8_t)(options >> 8);
            status[18] = (uint8_t)(options >> 16);
            status[19] = (uint8_t)(options >> 24);

            int res = os_mbuf_append(ctxt->om, &status, sizeof(status));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        } else if (attr_handle == dataHandle) {
            uint8_t data[20];
            memset(data, 0, sizeof(data));

            // @0 ???
            uint32_t value = 0;
            data[0] = (uint8_t)(value >> 0);
            data[1] = (uint8_t)(value >> 8);
            data[2] = (uint8_t)(value >> 16);
            data[3] = (uint8_t)(value >> 24);

            int res = os_mbuf_append(ctxt->om, &data, sizeof(data));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        }

    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) { // Writing
        size_t notifSize = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[notifSize];
        os_mbuf_copydata(ctxt->om, 0, notifSize, data);

        if (attr_handle == statusHandle) {

            if (notifSize >= 4) { 
                // readIndex = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            }

        } else if (attr_handle == dataHandle) {

            // TODO

        }

    }
    return 0;
}

#endif
