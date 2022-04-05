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
    cueController.ClearScratch();
}

void Pinetime::Controllers::CueService::Disconnect() {
    ResetState();
}

int Pinetime::Controllers::CueService::OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt) {
    bool trusted = true;
#ifdef CUEBAND_TRUSTED_ACTIVITY
    trusted = bleController.IsTrusted();
#endif

    m_system.CommsCue();

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) { // Reading

        if (attr_handle == statusHandle) {  // STATUS: Read `status`
            uint8_t status[20];
            memset(status, 0, sizeof(status));
            
            // Reset read index
            readIndex = 0;

            // Status
            uint32_t active_schedule_id;
            uint16_t max_control_points;
            uint16_t current_control_point;
            uint32_t override_remaining;
            uint32_t intensity;
            uint32_t interval;
            uint32_t duration;
            uint8_t status_flags = 0x00;
            cueController.GetStatus(&active_schedule_id, &max_control_points, &current_control_point, &override_remaining, &intensity, &interval, &duration);
            if (override_remaining > 0xffff) override_remaining = 0xffff;
            if (intensity > 0xffff) intensity = 0xffff;
            if (interval > 0xffff) interval = 0xffff;
            if (duration > 0xffff) duration = 0xffff;
            if (cueController.IsInitialized()) status_flags |= 0x01;        // b0 = service initialized
            if (firmwareValidator.IsValidated()) status_flags |= 0x02;      // b1 = firmware validated
            if (bleController.IsTrusted()) status_flags |= 0x04;            // b2 = connection trusted

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

            // @8 (0=not overridden), remaining override duration (seconds, saturates to 0xffff)
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

            // @16 Options mask and value
            options_t base;
            options_t mask;
            options_t effectiveValue = cueController.GetOptionsMaskValue(&base, &mask, nullptr);
            status[16] = (uint8_t)(base);
            status[17] = (uint8_t)(mask);
            status[18] = (uint8_t)(effectiveValue);

            // @19 Status flags, 0x01=initialized
            status[19] = (uint8_t)(status_flags);

            int res = os_mbuf_append(ctxt->om, &status, sizeof(status));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        } else if (attr_handle == dataHandle && trusted) {   // DATA: Read `control_point`
            uint8_t control_point_data[8];
            memset(control_point_data, 0, sizeof(control_point_data));

            // Read control point at index
            ControlPoint controlPoint = cueController.GetStoredControlPoint((int)readIndex);

            // @0 Index being read
            control_point_data[0] = (uint8_t)(readIndex >> 0);
            control_point_data[1] = (uint8_t)(readIndex >> 8);

            // @2 Intensity
            control_point_data[2] = (uint8_t)(controlPoint.GetVolume());

            // @3 Days
            control_point_data[3] = (uint8_t)(controlPoint.GetWeekdays());

            // @4 Minute of day
            unsigned int minute = controlPoint.GetTimeOfDay() / 60;
            if (minute > 0xffff) minute = 0xffff;
            control_point_data[4] = (uint8_t)(minute >> 0);
            control_point_data[5] = (uint8_t)(minute >> 8);
            
            // @6 Interval
            unsigned int interval = controlPoint.GetInterval();
            if (interval > 0xffff) interval = 0xffff;
            control_point_data[6] = (uint8_t)(interval >> 0);
            control_point_data[7] = (uint8_t)(interval >> 8);
            
            // Increment read index
            readIndex++;

            int res = os_mbuf_append(ctxt->om, &control_point_data, sizeof(control_point_data));
            return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        }

    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && trusted) { // Writing
        size_t notifSize = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[notifSize];
        os_mbuf_copydata(ctxt->om, 0, notifSize, data);

        if (attr_handle == statusHandle) {

            if (notifSize == 0) {   // STATUS: Write *(no data)* - reset schedule
                cueController.Reset(false);

            } else {

                if (data[0] == 0x01) {  // STATUS: Write change_options
                    if (notifSize >= 8) {
                        // @4 Options mask and value
                        options_t mask = (options_t)(data[4] | (data[5] << 8));
                        options_t value = (options_t)(data[6] | (data[7] << 8));
                        cueController.SetOptionsMaskValue(mask, value);
                    }

                } else if (data[0] == 0x02) {  // STATUS: Write set_impromtu

                    if (notifSize >= 16) {
                        // @4 Interval
                        uint32_t interval = (uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
                        // @8 Duration
                        uint32_t duration = (uint32_t)data[8] | ((uint32_t)data[9] << 8) | ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
                        // @12 Intensity
                        uint32_t intensity = (uint32_t)data[12] | ((uint32_t)data[13] << 8) | ((uint32_t)data[14] << 16) | ((uint32_t)data[15] << 24);
                        cueController.SetInterval(interval, duration);
                        if (intensity < 0xffff) {
                            cueController.SetPromptStyle(intensity);
                        }
                    }

                } else if (data[0] == 0x03) {  // STATUS: Write store_schedule

                    if (notifSize >= 8) {
                        // @4 Schedule ID
                        uint32_t schedule_id = (uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
                        cueController.CommitScratch(schedule_id);
                    }

                } // otherwise, unhandled

            }

        } else if (attr_handle == dataHandle) {

            if (notifSize == 0) {   // DATA: Write *(no data)* - clear scratch
                cueController.ClearScratch();

            } else {        // DATA: Write control_point

                if (notifSize >= 8) {
                    // @0 Index
                    int index = data[0] | (data[1] << 8);
                    // @2 Intensity
                    unsigned int intensity = data[2];
                    // @3 Days
                    unsigned int days = data[3];
                    // @4 Minute
                    unsigned int minute = data[4] | (data[5] << 8);
                    // @6 Interval
                    unsigned int interval = data[6] | (data[7] << 8);
                    ControlPoint controlPoint = ControlPoint(true, days, interval, intensity, minute * 60);
                    cueController.SetScratchControlPoint(index, controlPoint);
                }

            }

        }

    }
    return 0;
}

#endif
