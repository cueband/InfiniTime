// UART Service
// Dan Jackson, 2021

// NOTE: This is an incomplete "first step", currently with severe limitations:
// * inbound: only parses packets one-at-a-time rather than properly treating them as a stream
//   they should be buffered (up to some limit) and handed off to a higher level (e.g. line-at-a-time processing).
// * outbound stream should have a fixed maximum buffer, correctly send data that spans more 
//   than one transmit unit, and a check for whether the buffer can accept a new packet of a given size.

#include "cueband.h"

#ifdef CUEBAND_SERVICE_UART_ENABLED

#include "UartService.h"

#include "systemtask/SystemTask.h"
#include <hal/nrf_rtc.h>

// offset response from the year 2000 for compatibility
#define EPOCH_OFFSET 946684800

#define MAX_PACKET 20

uint8_t Pinetime::Controllers::UartService::streamBuffer[Pinetime::Controllers::UartService::sendCapacity];

// Simple function to write capitalised hex to a buffer from binary
// adds no spaces, adds a terminating null, returns chars written
// Endianess specified, for little endian, read starts at last ptr pos backwards
uint16_t WriteBinaryToHex(char* dest, void* source, uint16_t len, uint8_t littleEndian)
{
	uint16_t ret = (len*2);
	uint8_t* ptr = (uint8_t*)source;

	if(littleEndian) ptr += len-1; // Start at MSB

	char temp;
	for(;len>0;len--)
	{
		temp = '0' + (*ptr >> 4);
		if(temp>'9')temp += ('A' - '9' - 1);  
		*dest++ = temp;
		temp = '0' + (*ptr & 0xf);
		if(temp>'9')temp += ('A' - '9' - 1); 
		*dest++ = temp;

		if(littleEndian)ptr--;
		else ptr++;
	}
	*dest = '\0';
	return ret;
}

// Simple function to read an ascii string of hex chars from a buffer 
// For each hex pair, a byte is written to the out buffer
// Returns number read, earlys out on none hex char (caps not important)
uint16_t ReadHexToBinary(uint8_t* dest, const char* source, uint16_t maxLen)
{
	uint16_t read = 0;

	char hex1, hex2;
	for(;maxLen>0;maxLen-=2)
	{
		// First char
		if		(*source >= '0' && *source <= '9') hex1 = *source - '0';
		else if	(*source >= 'a' && *source <= 'f') hex1 = *source - 'a' + 0x0A;
		else if	(*source >= 'A' && *source <= 'F') hex1 = *source - 'A' + 0x0A;
		else break;

		source++;

		// Second char
		if		(*source >= '0' && *source <= '9') hex2 = *source - '0';
		else if	(*source >= 'a' && *source <= 'f') hex2 = *source - 'a' + 0x0A;
		else if	(*source >= 'A' && *source <= 'F') hex2 = *source - 'A' + 0x0A;
		else break;

		source++;

		// Regardless of endianess, pairs are assembled LSB on right
		*dest = (uint8_t)hex2 | (hex1<<4);	// hex1 is the msb

		// Increment count and dest
		read++;
		dest++;
	}

	return read;
}

// Encode a binary input as an ASCII Base64-encoded (RFC 3548) stream with NULL ending
// Output buffer must have capacity for (((length + 2) / 3) * 4) + 1 bytes
// Three bytes are represented by four characters
// - no output padding where the input is a multiple of 3
// Utility function: Encode a binary input as an ASCII Base64-encoded (RFC 3548) stream with NULL ending -- output buffer must have capacity for (((length + 2) / 3) * 4) + 1 bytes
#define BASE64_SIZE(_s) ((((_s) + 2) / 3) * 4) // this does not include the NULL terminator
size_t EncodeBase64(char *output, const void *input, size_t length)
{
	static const char base64lookup[64] = 
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
		'+', '/'
	};
	uint8_t *src = (uint8_t *)input;
    unsigned int value = 0;
    int bitcount = 0;
    size_t count = 0;
    for (size_t i = 0; i < length; i++)
    {
        // Add next byte into accumulator
        value = (value << 8) | src[i];
        bitcount += 8;
        
        // End of stream padding to next 6-bit boundary
        if (i + 1 >= length)
        {
            int boundary = ((bitcount + 5) / 6) * 6;
            value <<= (boundary - bitcount);
            bitcount = boundary;
        }

        // While we have 6-bit values to write
        while (bitcount >= 6)
        {
            // Get highest 6-bits and remove from accumulator
            bitcount -= 6;
            output[count++] = base64lookup[(value >> bitcount) & 0x3f];
        }
    }

    // Padding for correct Base64 encoding
    while ((count & 3) != 0) { output[count++] = '='; }

    // NULL ending (without incrementing count)
    output[count] = '\0';
    return count;
}



int UartCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
  auto uartService = static_cast<Pinetime::Controllers::UartService*>(arg);
  return uartService->OnCommand(conn_handle, attr_handle, ctxt);
}

Pinetime::Controllers::UartService::UartService(Pinetime::System::SystemTask& system,
        Controllers::Ble& bleController,
        Controllers::Settings& settingsController,
        Controllers::Battery& batteryController,
        Controllers::DateTime& dateTimeController,
        Pinetime::Controllers::MotorController& motorController,
        Pinetime::Controllers::MotionController& motionController
#ifdef CUEBAND_ACTIVITY_ENABLED
        , Pinetime::Controllers::ActivityController& activityController
#endif
#ifdef CUEBAND_CUE_ENABLED
        , Pinetime::Controllers::CueController& cueController
#endif
    ) : m_system(system),
    bleController {bleController},
    settingsController {settingsController},
    batteryController {batteryController},
    dateTimeController {dateTimeController},
    motorController {motorController},
    motionController {motionController} 
#ifdef CUEBAND_ACTIVITY_ENABLED
    , activityController {activityController}
#endif
#ifdef CUEBAND_CUE_ENABLED
    , cueController {cueController}
#endif
    {

    characteristicDefinition[0] = {
        .uuid = (ble_uuid_t*) (&uartRxCharUuid), 
        .access_cb = UartCallback, 
        .arg = this, 
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ
    };
    characteristicDefinition[1] = {
        .uuid = (ble_uuid_t*) (&uartTxCharUuid),
        .access_cb = UartCallback,
        .arg = this,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &transmitHandle
    };
    characteristicDefinition[2] = {0};

    serviceDefinition[0] = {
        .type = BLE_GATT_SVC_TYPE_PRIMARY, 
        .uuid = (ble_uuid_t*) &uartUuid, 
        .characteristics = characteristicDefinition
    };
    serviceDefinition[1] = {0};

#ifdef CUEBAND_STREAM_ENABLED
    streamFlag = false;
    streamSampleIndex = -1;
#endif
}

void Pinetime::Controllers::UartService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

void Pinetime::Controllers::UartService::Disconnect() {
    // Stop streaming
#ifdef CUEBAND_STREAM_ENABLED
    if (streamFlag) streamFlag = false;
#endif

    // Free resources
    sendBuffer = nullptr;
    blockLength = 0;
    blockOffset = 0;
    if (blockBuffer != nullptr) {
        free(blockBuffer);
        blockBuffer = nullptr;
    }
    packetTransmitting = false;
}

bool Pinetime::Controllers::UartService::IsSending() {
    return sendBuffer != nullptr && blockLength != 0;
}

void Pinetime::Controllers::UartService::Idle() {
    SendNextPacket();
}

void Pinetime::Controllers::UartService::SendNextPacket() {
// HACK: TxNotification not working?
packetTransmitting = false;
    if (IsSending() && !packetTransmitting) {
        if (blockOffset >= sendCapacity) blockOffset = 0;
        size_t len = sendCapacity - blockOffset;
        if (len > blockLength) len = blockLength;
        if (len > MAX_PACKET) len = MAX_PACKET;
        auto* om = ble_hs_mbuf_from_flat(sendBuffer + blockOffset, len);
        if (ble_gattc_notify_custom(tx_conn_handle, transmitHandle, om) == 0) {
            packetTransmitting = true;
            blockOffset += len;
            blockLength -= len;
            transmitErrorCount = 0;
        } else {
            if (transmitErrorCount++ > 10) {
                // TODO: Stop streaming (if streaming)?
                streamFlag = false;
                blockLength = 0;
            }
        }
    }
}

void Pinetime::Controllers::UartService::TxNotification(ble_gap_event* event) {
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

int Pinetime::Controllers::UartService::OnCommand(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt) {

#ifdef CUEBAND_ACTIVITY_ENABLED
    m_system.CommsActivity();
#endif

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        size_t notifSize = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[notifSize + 1];
        data[notifSize] = '\0';
        os_mbuf_copydata(ctxt->om, 0, notifSize, data);

        if (ble_uuid_cmp(ctxt->chr->uuid, (ble_uuid_t*) &uartRxCharUuid) == 0) {
            char resp[128];
            // Initially-empty response
            resp[0] = '\0';

#ifdef CUEBAND_STREAM_ENABLED
            // If receive any packet while streaming, stop streaming
            if (streamFlag) { // data[0] != 'I'
                streamFlag = false;
                // If mid-packet, terminate packet
                if (streamSampleIndex >= 0) {
                    StreamAppendString("\r\n");
                }
                streamSampleIndex = -1;
            }
#endif

            if (IsSending()) {
                StreamAppendString("?Busy\r\n");
                return 0;
            }

#ifdef CUEBAND_ACTIVITY_ENABLED
            activityController.Event(ACTIVITY_EVENT_BLUETOOTH_COMMS);
#endif

            if (data[0] == '#') {  // Device ID query
                std::array<uint8_t, 6> addr = bleController.Address();        // using BleAddress = std::array<uint8_t, 6>;
                sprintf(resp, "AP:%u,%s\r\n#:%02x%02x%02x%02x%02x%02x\r\n", CUEBAND_APPLICATION_TYPE, CUEBAND_VERSION, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

            } else if (data[0] == '0') {  // Motor/LEDs Off
                sprintf(resp, "OFF\r\n");
                motorController.RunForDuration(0);

            } else if (data[0] == '1') {  // Vibrate motor (original was duration 8 cycles at 8 Hz, mask pattern 0x00f5)
                sprintf(resp, "MOT\r\n");
                motorController.RunForDuration(50);   // milliseconds

            } else if (data[0] == 'A') {  // Accelerometer sample
                const char *chip = "?";
                if (motionController.DeviceType() == MotionController::DeviceTypes::BMA421) chip = "BMA421";
                else if (motionController.DeviceType() == MotionController::DeviceTypes::BMA425) chip = "BMA425";
                else if (motionController.DeviceType() == MotionController::DeviceTypes::Unknown) chip = "Unknown";
                int reg = 0x00;
                unsigned int eeLevel = 0;
                sprintf(resp, "A:%d,%d,%d,%02x,%s,%u\r\n", motionController.X(), motionController.Y(), motionController.Z(), reg, chip, eeLevel);

            } else if (data[0] == 'B') {  // Battery
                sprintf(resp, "B:%d%%\r\n", batteryController.PercentRemaining());

            } else if (data[0] == 'I') {  // Stream sensor data
#ifdef CUEBAND_STREAM_ENABLED
                // Parse 'I' command's space-separated rate/range
                char *p = (char *)data + 1;
                int rate = (int)strtol(p, &p, 0);
                int range = (int)strtol(p, &p, 0);

                // Streaming defaults
                if (rate == 0) rate = 50;
                if (range == 0) range = 8;

                // TODO: Try to honor the requested settings
#ifdef CUEBAND_STREAM_RESAMPLED
                rate = ACTIVITY_RATE / CUEBAND_STREAM_DECIMATE;                      // 30 Hz; 
#else
                rate = CUEBAND_BUFFER_EFFECTIVE_RATE / CUEBAND_STREAM_DECIMATE;      // 8 Hz; // 50 Hz; // 100 Hz; 
#endif
                range = CUEBAND_ORIGINAL_RANGE;           // +/- 2g; +/- 8g

                // Respond with settings in use
                int mode = 0;   // 0=accel, 2=debug info ('D' command)
                sprintf(resp, "OP:%02x, %d, %d\r\n", mode, rate, range);

                streamConnectionHandle = conn_handle;
                streamFlag = true;
                transmitErrorCount = 0;
                streamSampleIndex = -1; // header not yet sent
                streamStartTicks = xTaskGetTickCount();
#else
                sprintf(resp, "?Disabled\r\n");
#endif

            } else if (data[0] == 'J') { // Set current cueing interval: "J <interval> <maximumRuntime> <motorPulseWidth>"
#ifdef CUEBAND_CUE_ENABLED
                char *p = (char *)data + 1;
                uint32_t interval = (uint32_t)strtol(p, &p, 0);
                uint32_t maximumRuntime = (uint32_t)strtol(p, &p, 0);
                uint32_t motorPulseWidth = (uint32_t)strtol(p, &p, 0);
                if (maximumRuntime == 0) maximumRuntime = CueController::MAXIMUM_RUNTIME_INFINITE;
                if (motorPulseWidth == 0) motorPulseWidth = CueController::DEFAULT_MOTOR_PULSE_WIDTH;
                if (motorPulseWidth > 1000) motorPulseWidth = 1000;
                cueController.SetInterval(interval, maximumRuntime, motorPulseWidth);
                sprintf(resp, "J:%u,%d,%u\r\n", (uint16_t)interval, (int16_t)maximumRuntime, (uint16_t)motorPulseWidth); // maximumRuntime "-1" if infinite
#else
                sprintf(resp, "?Disabled\r\n");
#endif

            } else if (data[0] == 'M') {  // Vibrate motor (original was duration 16 cycles at 8 Hz, mask pattern 0x6565)
                sprintf(resp, "MOT\r\n");
                motorController.RunForDuration(100);   // milliseconds

            } else if (data[0] == 'N') {  // Epoch interval (read-only at the moment)

#ifdef CUEBAND_ACTIVITY_ENABLED
                sprintf(resp, "N:%lu\r\n", activityController.EpochInterval());
#else
                sprintf(resp, "?Disabled\r\n");
#endif

            } else if (data[0] == 'T') {  // Time
                bool err = false;
                if (data[1] == '$') { // Set time "T$YY/MM/DD,hh:mm:ss"
                                      //           0123456789012345678
                    if (data[4] == '/' && data[7] == '/' && data[10] == ',' && data[13] == ':' && data[16] == ':') {
                        uint16_t year = 2000 + 10 * (data[2] - '0') + (data[3] - '0');
                        uint8_t month = 10 * (data[5] - '0') + (data[6] - '0');
                        uint8_t day = 10 * (data[8] - '0') + (data[9] - '0');
                        uint8_t hour = 10 * (data[11] - '0') + (data[12] - '0');
                        uint8_t minute = 10 * (data[14] - '0') + (data[15] - '0');
                        uint8_t second = 10 * (data[17] - '0') + (data[18] - '0');

                        uint8_t dayOfWeek = 0;
                        uint32_t systickCounter = nrf_rtc_counter_get(portNRF_RTC_REG);

                        dateTimeController.SetTime(year, month, day, dayOfWeek, hour, minute, second, systickCounter);

                    } else {
                        sprintf(resp, "?!\r\n");
                        err = true;
                    }
                } else if (data[0] != '\0') {
                    sprintf(resp, "?!\r\n");
                    err = true;
                }

                if (!err) {
                    uint32_t now = std::chrono::duration_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime().time_since_epoch()).count();
                    if (now < EPOCH_OFFSET) {
                        sprintf(resp, "T:%ld\r\n", (long)now - EPOCH_OFFSET);  // Negative value for 1970-2000
                    } else {
                        sprintf(resp, "T:%lu\r\n", now - EPOCH_OFFSET);  // Positive value for rest of range 2000-2106
                    }
                }

            } else if (data[0] == 'Q') {  // Query

#ifdef CUEBAND_ACTIVITY_ENABLED
                uint32_t now = std::chrono::duration_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime().time_since_epoch()).count();
                char *r = resp;
                if (now < EPOCH_OFFSET) {
                    r += sprintf(r, "T:%ld\r\n", (long)now - EPOCH_OFFSET);  // Negative value for 1970-2000
                } else {
                    r += sprintf(r, "T:%lu\r\n", now - EPOCH_OFFSET);  // Positive value for rest of range 2000-2106
                }
                r += sprintf(r, "B:%lu\r\n", activityController.ActiveLogicalBlock());
                r += sprintf(r, "N:%lu\r\n", activityController.EpochIndex());
                
                uint32_t blockTimestamp = activityController.BlockTimestamp();
                if (blockTimestamp < EPOCH_OFFSET) {
                    r += sprintf(r, "E:%ld\r\n", (long)blockTimestamp - EPOCH_OFFSET);  // Negative value for 1970-2000
                } else {
                    r += sprintf(r, "E:%lu\r\n", blockTimestamp - EPOCH_OFFSET);  // Positive value for rest of range 2000-2106
                }
                r += sprintf(r, "C:%lu\r\n", activityController.ActiveLogicalBlock() - activityController.EarliestLogicalBlock());
                r += sprintf(r, "I:%lu\r\n", readLogicalBlockIndex);
#else
                sprintf(resp, "?Disabled\r\n");
#endif

            } else if (data[0] == 'R') {   // Read block
#ifdef CUEBAND_ACTIVITY_ENABLED
                bool useBase64 = false;
                char *p = (char *)data + 1;
                if (*p == '+') {
                    useBase64 = true;
                    p = (char *)data + 2;
                } else {
                    useBase64 = false;
                    p = (char *)data + 1;
                }
                char *e = p;
                long index = strtol(p, &e, 0);
                if (e != p) {
                    readLogicalBlockIndex = index;
                }

                if (blockBuffer == nullptr) {
                    blockBuffer = (uint8_t *)malloc(ACTIVITY_BLOCK_SIZE * 2 + 1);
                }

                if (blockBuffer == nullptr) {
                    sprintf(resp, "?Memory\r\n");
                } else {
                    // Read at end of buffer (hex/base64 conversion will write from the start)
                    uint8_t *data = blockBuffer + ACTIVITY_BLOCK_SIZE;
                    bool read = activityController.ReadLogicalBlock(readLogicalBlockIndex, data);
// HACK: Temporary dummy data for out-of-range blocks
#if defined(CUEBAND_DEBUG_DUMMY_MISSING_BLOCKS)
if (!read) {
    for (int i = 0; i < ACTIVITY_BLOCK_SIZE; i++) {
        data[i] = (uint8_t)i;
    }
    read = true;
}
#endif
                    if (!read) {
                        sprintf(resp, "?NotFound\r\n");
                    } else {
                        uint16_t len;

                        if (useBase64) {
                            len = BASE64_SIZE(ACTIVITY_BLOCK_SIZE);
                            EncodeBase64((char *)blockBuffer, data, ACTIVITY_BLOCK_SIZE);
                        } else {
                            len = ACTIVITY_BLOCK_SIZE * 2;
                            WriteBinaryToHex((char *)blockBuffer, data, ACTIVITY_BLOCK_SIZE, false);
                        }
                        blockBuffer[len++] = '\r';
                        blockBuffer[len++] = '\n';
                        blockBuffer[len++] = '\0';

                        // sendBuffer = blockBuffer;
                        // blockLength = len;
                        // blockOffset = 0;
                        tx_conn_handle = conn_handle;
// TODO: Write blockBuffer incrementally so such a large allocation is not required
                        StreamAppendString((const char *)blockBuffer);

//                        SendNextPacket();

                        // Increment readLogicalBlockIndex
                        readLogicalBlockIndex++;

                    }
                }
#else
                sprintf(resp, "?Disabled\r\n");
#endif


            } else { // Unhandled
                sprintf(resp, "?\r\n");
            }

            // Response
            if (strlen(resp) > 0) {
                tx_conn_handle = conn_handle;
                StreamAppendString(resp);
                SendNextPacket();
            }

        }
    }
    return 0;
}


#ifdef CUEBAND_STREAM_ENABLED
static size_t Base16Encode(const uint8_t *binary, size_t countBinary, uint8_t *output) {
    const uint8_t *sp = binary;
    uint8_t *dp = output;
    for (size_t i = countBinary; i > 0; i--) {
        uint8_t v = *sp++;
        char hi = ((v >> 4) & 0xf);
        *dp++ = (hi >= 10) ? (hi + 'a' - 10) : ('0' + hi);
        char lo = ((v >> 0) & 0xf);
        *dp++ = (lo >= 10) ? (lo + 'a' - 10) : ('0' + lo);
    }
    return dp - output;
}

// Handle streaming
bool Pinetime::Controllers::UartService::IsStreaming() {
    return streamFlag;
}

bool Pinetime::Controllers::UartService::StreamAppendString(const char *data) {
    return StreamAppend((const uint8_t *)data, strlen(data));
}

bool Pinetime::Controllers::UartService::StreamAppend(const uint8_t *data, size_t length) {
    if (sendBuffer != streamBuffer) {
        sendBuffer = streamBuffer;
        blockOffset = 0;
        blockLength = 0;
    }

    // Check overall remaining capacity
    size_t remaining = sendCapacity - blockLength;
    if (length > remaining) {
        return false;
    }

    // Append after end of data and before start of data or wrap point
    size_t end = (blockOffset + blockLength) % sendCapacity;        // end of data
    // End after start: free space before wrap-around point
    if (end >= blockOffset && blockLength < sendCapacity) {
        size_t available = sendCapacity - end;
        if (available > length) available = length;
        memcpy(streamBuffer + end, data, available);
        blockLength += available;
        data += available;
        length -= available;
        // Any remaining data wraps-around to start of buffer
        if (length > 0) {
            available = blockOffset;
            if (available > length) available = length;
            memcpy(streamBuffer + 0, data, available);
            blockLength += available;
            length -= available;
        }
    } else {
        // Free space in middle of buffer
        size_t available = blockOffset - end;
        if (available > length) available = length;
        memcpy(streamBuffer + end, data, available);
        blockLength += available;
        length -= available;
    }

    return true;
}

bool Pinetime::Controllers::UartService::StreamSamples(const int16_t *samples, size_t count) {

    for (unsigned int i = 0; i < count; i++) {
        // Send header before first smaple
        if (streamSampleIndex < 0) {
            // TODO: Use system timer instead of task scheduler's timer?
            auto currentTicks = xTaskGetTickCount();
            TickType_t deltaTicks = currentTicks - streamStartTicks;

            // Populate these
            uint32_t timestamp = (uint32_t)((uint64_t)deltaTicks * 32768 / configTICK_RATE_HZ);
            //timestamp = timestamp & 0x00ffffff; // least-significant 24-bits only, for compatibility
            //uint16_t batteryRaw = (uint16_t)((uint32_t)batteryController.Voltage() * 142 / 1000);
            uint16_t batteryRaw = ((batteryController.Voltage() / 10) << 7) | batteryController.PercentRemaining();
            uint16_t tempRaw = 0xffff; // 0 * 4;

            // Binary header
            uint8_t headerBin[8];
            headerBin[0] = (uint8_t)(timestamp >>  0);
            headerBin[1] = (uint8_t)(timestamp >>  8);
            headerBin[2] = (uint8_t)(timestamp >> 16);
            headerBin[3] = (uint8_t)(timestamp >> 24);
            headerBin[4] = (uint8_t)(batteryRaw >> 0);
            headerBin[5] = (uint8_t)(batteryRaw >> 8);
            headerBin[6] = (uint8_t)(tempRaw >> 0);
            headerBin[7] = (uint8_t)(tempRaw >> 8);

            // Hex header
            uint8_t headerHex[16];
            Base16Encode(headerBin, sizeof(headerBin), headerHex);

            // Send hex-encoded
            StreamAppend(headerHex, sizeof(headerHex));

            // Start on first sample
            streamSampleIndex = 0;
        }

        if (++streamRawSampleCount >= CUEBAND_STREAM_DECIMATE)
        {
            streamRawSampleCount = 0;

            // Send single sample
            int16_t accelX = samples[CUEBAND_AXES * i + 0];
            int16_t accelY = samples[CUEBAND_AXES * i + 1];
            int16_t accelZ = samples[CUEBAND_AXES * i + 2];

            // Streaming standard scale of 1g=4096
            accelX = (int16_t)((int32_t)accelX * 4096 / CUEBAND_BUFFER_16BIT_SCALE);
            accelY = (int16_t)((int32_t)accelY * 4096 / CUEBAND_BUFFER_16BIT_SCALE);
            accelZ = (int16_t)((int32_t)accelZ * 4096 / CUEBAND_BUFFER_16BIT_SCALE);

            // Binary sample (For streaming, 1g=4096)
            uint8_t sampleBin[6];
            sampleBin[0] = (uint8_t)(accelX >>  0);
            sampleBin[1] = (uint8_t)(accelX >>  8);
            sampleBin[2] = (uint8_t)(accelY >>  0);
            sampleBin[3] = (uint8_t)(accelY >>  8);
            sampleBin[4] = (uint8_t)(accelZ >>  0);
            sampleBin[5] = (uint8_t)(accelZ >>  8);

            // Hex sample
            uint8_t sampleHex[12 + 2];  // 2 * 3 * 2 + 2
            Base16Encode(sampleBin, sizeof(sampleBin), sampleHex);

            // Send hex-encoded
            uint16_t sampleLen = 12;    // 2 * 3 * 2

            streamSampleIndex++;
            if (streamSampleIndex >= 25) {
                // Append CRLF
                sampleHex[sampleLen++] = '\r';
                sampleHex[sampleLen++] = '\n';
                // Next packet will send new header
                streamSampleIndex = -1;
            }

            StreamAppend(sampleHex, sampleLen);
        }
    }

    return true;
}


// TODO: Rate-limit if many packets are not yet sent
bool Pinetime::Controllers::UartService::Stream() {
    // Streaming format: >16 header hex-pairs, multiple of 12 hex-pairs for the samples // 8+4+4+25*12+2=318, -2 CRLF = 316
    // @0 DWORD timestamp (1/32768ths second)
    // @4 WORD batteryRaw (= voltage * 1280 / 9)
    // @6 WORD tempRaw (= celsius * 4)
    // @8 [int16,int16,int16] sample (* 25 samples)
    // +CRLF

    // Not streaming if disconnected
    // TODO: Make this event-based
    if (streamFlag && !bleController.IsConnected()) {
        streamFlag = false;
    }

    if (!streamFlag) {
        return false;
    }

#if defined(CUEBAND_BUFFER_ENABLED)
    int16_t *accelValues = NULL;
    unsigned int lastCount = 0;
    unsigned int totalSamples = 0;
#ifdef CUEBAND_STREAM_RESAMPLED
    activityController.GetBufferData(&accelValues, &lastCount, &totalSamples);
#else
    motionController.GetBufferData(&accelValues, &lastCount, &totalSamples);
#endif

    // Only add samples if they're new
    if (totalSamples != lastTotalSamples) {
        StreamSamples(accelValues, lastCount);
        lastTotalSamples = totalSamples;
    }
#else
    // Send single sample
    int16_t accel[3];
    accel[0] = motionController.X();
    accel[1] = motionController.Y();
    accel[2] = motionController.Z();

    // Resecale to 16-bit
    accel[0] = (int16_t)((int32_t)accel[0] * CUEBAND_BUFFER_16BIT_SCALE / CUEBAND_ORIGINAL_SCALE);
    accel[1] = (int16_t)((int32_t)accel[1] * CUEBAND_BUFFER_16BIT_SCALE / CUEBAND_ORIGINAL_SCALE);
    accel[2] = (int16_t)((int32_t)accel[2] * CUEBAND_BUFFER_16BIT_SCALE / CUEBAND_ORIGINAL_SCALE);
    
    StreamSamples(accel, 1);
#endif

    SendNextPacket();
    return streamFlag;
}
#endif

// TODO: Proper notification
// std::string Pinetime::Controllers::UartService::getLine() {
//     return m_line;
// }

#endif
