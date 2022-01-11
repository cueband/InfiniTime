# Cue.band InfiniTime Firmware

This is a work-in-progress fork of [InfiniTime](https://github.com/JF002/InfiniTime) for the [Cue Band project](https://cue.band/).

Test firmware: 

  * [cueband/InfiniTime/releases](https://github.com/cueband/InfiniTime/releases)


## Overview

"Cue.band" firmware for the PineTime device.

This repository is intended to track the main [InfiniTime repository](https://github.com/JF002/InfiniTime). 
To this end, the additional services are switched via a single master configuration file ([`cueband.h`](src/cueband.h)) 
-- so that the code demonstrates a clear separation between the base firmware and additional services, and that variations 
between the two can be compiled and tested at any time.

All stored/transmitted values are little-endian.

The primary additional features:

*	Vibration cues at timed intervals.
*	Weekly scheduling of these intervals.
*	On-device cue management (e.g. silencing of cues).
*	Device wear detection (e.g. only cue while worn).
*	Device activity log (e.g. when cueing occurs).
*	Synchronization (scheduling and logs) between the device and a user's phone.

Further functionality:

*	Additional security protections.
* Interface simplifications (feature removal to standard watch functionality).
* BLE UART alternative for all communciation.
* ? Shipping mode

In the following description, `(*)` denotes a feature not yet fully implemented.


## Standard BLE Services

Standard BLE services that will be used:

* Service: Device Information `0000180a-0000-1000-8000-00805f9b34fb`
  * Characteristic: Manufacturer Name String `"PINE64"` `00002a29-0000-1000-8000-00805f9b34fb` (R)
  * Characteristic: Model Number String `"PineTime"` `00002a24-0000-1000-8000-00805f9b34fb` (R)
  * Characteristic: Serial Number String `"0"` `00002a25-0000-1000-8000-00805f9b34fb` (R)
  * Characteristic: Firmware Revision String `"1.2.0"` `00002a26-0000-1000-8000-00805f9b34fb` (R)
  * Characteristic: Hardware Revision String `"1.0.0"` `00002a27-0000-1000-8000-00805f9b34fb` (R)
  * Characteristic: Software Revision String `"InfiniTime"` `00002a28-0000-1000-8000-00805f9b34fb` (R)

* Service: Battery Service `0000180f-0000-1000-8000-00805f9b34fb`
  * Characteristic: Battery Level `00002a19-0000-1000-8000-00805f9b34fb` (R/NOTIFY)

* Service: Current Time Service `00001805-0000-1000-8000-00805f9b34fb`
  * Characteristic: Current Time `00002a2b-0000-1000-8000-00805f9b34fb` (R/W)
  
* Service: Device Firmware Update Service `00001530-1212-efde-1523-785feabcd123`
  * Characteristic: `00001532-1212-efde-1523-785feabcd123` (W/NORESPONSE)
  * Characteristic: `00001531-1212-efde-1523-785feabcd123` (W/CCCD/NOTIFY)
  * Characteristic: `00001534-1212-efde-1523-785feabcd123` (R)


Other standard services (not likely to be used):

* Service: Alert Notification Service `00001811-0000-1000-8000-00805f9b34fb`
  * Characteristic: New Alert `00002a46-0000-1000-8000-00805f9b34fb` (W)
  * Characteristic: (Custom?) Alert Notification Event `00020001-78fc-48fe-8e23-433b3a1942d0` (CCCD/NOTIFY)

* Service: Immediate Alert `00001802-0000-1000-8000-00805f9b34fb`
  * Characteristic: Alert level `00002a06-0000-1000-8000-00805f9b34fb` (W/NORESPONSE)

<!--
* Service: Heart Rate `0000180d-0000-1000-8000-00805f9b34fb`
  * Characteristic: Heart Rate Measurement `00002a37-0000-1000-8000-00805f9b34fb` (R/CCCD/NOTIFY)
-->


## Additional Feature: Cueing

Cueing...

**TODO:** Full description.

Brief notes:

* Control points
* Each applies to one or more days, and a specific time of day, specifying the prompting interval and type
* Prompts given as scheduled
* Local muting/snoozing of prompts
* Schedule updated over Bluetooth


### Cueing watch app interface

Cueing watch app interface... snooze/sleep/pause prompts... configure...

**TODO:** Full description.



### (Proposed) Prompting Cue Schedule Configuration BLE Service

The BLE service can be used to:

* query the current stored schedule id
* query the current stored schedule (one control point at a time, incrementing the `read_index` after each read)
* write a new *scratch* schedule in parts (one control point at a time)
* store the *scratch* schedule as active one (specifying a unique schedule ID so that this can be later queried to check that the schedule is the current one)


#### Service: Schedule

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Schedule Service                                 |
| UUID                          | *(TBD)*                                          |


#### Characteristic: Schedule Status

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Schedule Status Characteristic                   |
| UUID                          | *(TBD)*                                          |
| Read `status`                 | Query the current schedule status, and resets the `read_index` to `0`.   |
| Write *(no data)*             | Clears the scratch schedule with empty control points, and stores it as the active schedule with `schedule_id=0xffffffff` indicating no schedule. |
| Write `change_options`        | Changes the allowed device interface options as specified. |
| Write `set_impromptu`         | Configures the current *impromptu* settings . |
| Write `store_schedule`        | Store the scratch schedule as the active schedule with the specified ID. |

`status` is:

> ```c
> struct {
>     uint32_t active_schedule_id;    // @0
>     uint16_t max_control_points;    // @4
>     uint16_t current_control_point; // @6 Current active control point (0xffff=none)
>     uint8_t override;               // @8 Override setting (0x00=no override, 0x01=snooze, 0x02=impromptu)
>     uint8_t intensity;              // @9 Active cueing intensity
>     uint16_t interval;              // @10 Active cueing interval (seconds)
>     uint16_t duration;              // @12 Active configured cueing duration (seconds, saturates to 0xffff)
>     uint16_t remaining;             // @14 Active remaining cueing duration (seconds, saturates to 0xffff)
>     uint16_t options;               // @16 Device interface options
>     uint16_t reserved;              // @18 (reserved)
> } // @20
> ```

`change_options` is:

> ```c
>  struct {
>      uint8_t command_type;           // @0 = 0x01 for "change >  options"
>      uint8_t reserved[3];            // @1 (reserved/padding, >  write as 0x00)
>      uint16_t options;               // @4 Device interface options
>  } // @6
>  ```
>  
>  Where `options` is combined of the following bit flags OR'd together:
>  
>  * `0b00000001` - Allow user to enable/disable cueing functionality (set: user enable/disable in cue settings menu; >  clear: do not show cue settings menu)
>  * `0b00000010` - Allow cueing functionality -- will follow a programmed schedule
>  * `0b00000100` - If cueing allowed, will show status on screen
>  * `0b00001000` - If cueing allowed, will allow tap to open cue details
>  * `0b00010000` - Cue details allows customized vibration level
>  * `0b00100000` - Cue details allows snooze
>  * `0b01000000` - Cue details allows impromtu cueing
>  * `0b10000000` - (reserved)

`set_impromtu` is:

> ```c
> struct {
>     uint8_t command_type;           // @0 = 0x02 for "set impromptu"
>     uint8_t reserved[3];            // @1 (reserved/padding, write as 0x00)
>     uint8_t override;               // @4 Override setting (0x00=no override, 0x01=snooze, 0x02=impromptu)
>     uint8_t intensity;              // @5 (`override != 0x00`) Override cueing intensity
>     uint16_t interval;              // @6 (`override != 0x00`) Override cueing interval (seconds)
>     uint32_t duration;              // @8 (`override != 0x00`) Override cueing duration (seconds)
> } // @12
> ```

`store_schedule` is:

> ```c
> struct {
>     uint8_t command_type;           // @0 = 0x03 for "store schedule
>     uint8_t reserved[3];            // @1 (reserved/padding, write as 0x00)
>     uint32_t schedule_id;           // @4 Schedule ID to use to store the current scratch schedule information
> } // @8
> ```


#### Characteristic: Schedule Control Point

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Schedule Control Point Characteristic            |
| UUID                          | *(TBD)*                                          |
| Read `control_point`          | Reads the stored control point at the `read_index`, and increments the `read_index`.  |
| Write *(no data)*             | Clears the current scratch schedule, fills with all-empty control points.  |
| Write `control_point`         | Set the scratch control point values at the specified index.  |


Where `control_point`:

> ```c
> struct {
>     uint16_t index;         // @0 Write: control point index to set; Read: the `read_index` being read
>     uint8_t  intensity;     // @2 Prompt intensity (0=off; non-zero values=prompt)
>     uint8_t  days;          // @3 Least-significant 7-bits: a bitmap of the days the control point is active for. (b0=Sun, b1=Mon, ..., b6=Sat)
>     uint16_t minute;        // @4 Least-significant 11-bits: minute of the day the control point begins. (0-1439)
>     uint16_t interval;      // @6 Number of seconds for the prompting interval
> } // @8
> ```


## Additional Feature: Device Activity Log

The device activity log measures device state, user interactions, and user activity levels (for wear-time, and so that a user does not feel the need to substitute another device to measure activity levels).

### Sampling and recording

**TODO:** Full description.

Brief notes:

* Storage file format as a block structure (`activity_log` format, see below: *Device Activity Log Block Format*).
* ~~Circular buffer~~ -- did not work as LittleFS cannot be used for random writes within a large file, flushing appears to cost of an order of the remainder of the file [LittleFS Issue #27](https://github.com/littlefs-project/littlefs/issues/27).  Instead *N* files are kept, append-only, and the oldest is removed and replaced as required.
* Logical block id
* Active block (RAM)
* Accelerometer in FIFO mode
* Data passed to Activity service for filtering, calculations, and summarization


### Device Activity Log Service BLE Service

User subscribes to notifications on the device's *TX* channel to receive response data, and sends packets to the device's *RX* channel for commands -- these commands initiate a response from the device.

#### Service: Activity

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Activity Service                                 |
| UUID                          | `0e1d0000-9d33-4e5e-aead-e062834bd8bb`           |

#### Characteristic: Activity Status

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Activity Status Characteristic                   |
| UUID                          | `0e1d0001-9d33-4e5e-aead-e062834bd8bb`           |
| Read `status`                 | Query current activity log status.               |
| Write `uint8_t[6]`            | `"Erase!"`: resets the activity log.             |
|                               | `"Validate!"`: remotely validates the firmware (risky)  |
|                               | `"Reset!"`: remotely resets the device (risky?)  |

Where `status` is:

> ```c
> struct {
>     uint32_t earliestBlockId;           // @0  Earliest available logical block ID
>     uint32_t activeBlockId;             // @4  Last available logical block ID (the active block -- partially written)
>     uint16_t blockSize = 256;           // @8  Size (bytes) of each block
>     uint16_t epochInterval = 60;        // @10 Epoch duration (seconds)
>     uint16_t maxSamplesPerBlock = 28;   // @12 Maximum number of epoch samples in each block
>     uint8_t  flags;                     // @14 Status flags (b0 = firmware validated)
> } // @14
> ```

#### Characteristic: Activity Block

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Activity Block ID Characteristic                 |
| UUID                          | `0e1d0002-9d33-4e5e-aead-e062834bd8bb            |
| Write `request`               | Begin transmission of `response` for requested block ID. |

Where `request` is:

> ```c
> struct {
>     uint32_t logicalBlockId;            // @0
> } // @4
> ```

#### Characteristic: Activity Block Data

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | Activity Block Data Characteristic               |
| UUID                          | `0e1d0003-9d33-4e5e-aead-e062834bd8bb`           |
| Notification `uint8_t[<=20]`  | Subscribe to notifications to stream `response`. |

Where `response` is:

> ```c
> struct {
>     uint16_t payload_length;                // @0
>     uint8_t payload_body[payload_length];   // @2
> } // @(payload_length)
> ```

Where `payload_length` is likely to be `256`, and `payload_body` should be interpreted as `activity_log` (see below: *Device Activity Log Block Format*).


### Device Activity Log Block Format

The device activity log blocks are of the form `activity_log`:

> ```c
> const size_t BLOCK_SIZE = 256;
> const size_t SAMPLE_CAPACITY = ((BLOCK_SIZE-30-2)/8);  // =28
> 
> struct {
>     // @0 Header (30 bytes)
>     uint16_t   block_type;              // @0  ASCII 'A' and 'D' as little-endian (= 0x4441)
>     uint16_t   block_length;            // @2  Bytes following the type/length (BLOCK_SIZE-4=252)
>     uint16_t   format;                  // @4  0x00 = current format (8-bytes per sample)
>     uint32_t   block_id;                // @6  Logical block identifier
>     uint8_t[6] device_id;               // @10 Device ID (address)
>     uint32_t   timestamp;               // @16 Seconds since epoch for the first sample
>     uint8_t    count;                   // @20 Number of valid samples (up to 28 samples when 8-bytes each in a 256-byte block)
>     uint8_t    epoch_interval;          // @21 Epoch interval (seconds, = 60)
>     uint32_t   prompt_configuration;    // @22 Active prompt configuration ID (may remove: this is just as a diagnostic as it can change during epoch)
>     uint8_t    battery;                 // @26 Battery (0xff=unknown; top-bit=charging, lower 7-bits: percentage)
>     uint8_t    accelerometer;           // @27 Accelerometer (bottom 2 bits sensor type; next 2 bits reserved for future use; next 2 bits reserved for rate information; top 2 bits reserved for scaling information).
>     int8_t     temperature;             // @28 Temperature (degrees C, signed 8-bit value, 0x80=unknown)
>     uint8_t    firmware;                // @29 Firmware version
> 
>     // @30 Body (BLOCK_SIZE-30-2=224 bytes)
>     activity_sample sample[SAMPLE_CAPACITY];// @30 Samples (8-bytes each; (BLOCK_SIZE-30-2)/8 = 28 count) at epoch interval from start time
> 
>     // @(BLOCK_SIZE-2=254) Checksum (2 bytes)
>     uint16_t   checksum;                // @(BLOCK-SIZE-2=254)
> } // @(BLOCK_SIZE)
> ```

Where `format` is one of:

> |  Value | Description                                                |
> |:------:|:-----------------------------------------------------------|
> | 0x0000 | 30 Hz resampled data, no high-pass filter, no SVMMO.       |
> | 0x0001 | 30 Hz resampled data, no high-pass filter, SVMMO present.  |
> | 0x0002 | 40 Hz resampled data, high-pass filter SVMMO and unfiltered SVMMO. |

Each sample is of the form `activity_sample`:

> ```c
> struct {
>     uint16_t events;                    // @0 Event flags (see below)
>     uint16_t prompts_steps;             // @2 Lower 10-bits: step count; next 3-bits: muted prompts count (0-7 saturates); top 3-bits: prompt count (0-7 saturates).
>     uint16_t mean_filtered_svmmo;       // @4 Mean of the *filter(abs(SVM-1))* values for the entire epoch, using a high-pass filter at 0.5 Hz (0xffff = invalid, e.g. too few samples; 0xfffe = saturated/clipped value)
>     uint16_t mean_svmmo;                // @6 Mean of the *abs(SVM-1)* values for the entire epoch (0xffff = invalid, e.g. too few samples; 0xfffe = saturated/clipped value)
> } // @8
> ```

<!-- Note: Offset `@4` was previously (format `0x0000`-`0x0001`): *Mean of the SVM values for the entire epoch*. -->


The `events` flags are bitwise flags and defined as follows:

> ```c
> const uint16_t ACTIVITY_EVENT_POWER_CONNECTED     = 0x0001;  // @b0  Connected to power for at least part of the epoch
> const uint16_t ACTIVITY_EVENT_POWER_CHANGED       = 0x0002;  // @b1  Power connection status changed during the epoch
> const uint16_t ACTIVITY_EVENT_BLUETOOTH_CONNECTED = 0x0004;  // @b2  Connected to Bluetooth for at least part the epoch
> const uint16_t ACTIVITY_EVENT_BLUETOOTH_CHANGED   = 0x0008;  // @b3  Bluetooth connection status changed during the epoch
> const uint16_t ACTIVITY_EVENT_BLUETOOTH_COMMS     = 0x0010;  // @b4  Communication protocol activity
> const uint16_t ACTIVITY_EVENT_WATCH_AWAKE         = 0x0020;  // @b5  Watch was awoken at least once during the epoch
> const uint16_t ACTIVITY_EVENT_WATCH_INTERACTION   = 0x0040;  // @b6  Watch screen interaction (button or touch)
> const uint16_t ACTIVITY_EVENT_RESTART             = 0x0080;  // @b7  First epoch after device restart (or event logging restarted?)
> const uint16_t ACTIVITY_EVENT_NOT_WORN            = 0x0100;  // @b8  (TBD?) Activity: Device considered not worn
> const uint16_t ACTIVITY_EVENT_ASLEEP              = 0x0200;  // @b9  (TBD?) Activity: Wearer considered asleep
> const uint16_t ACTIVITY_EVENT_CUE_DISABLED        = 0x0400;  // @b10 (TBD?) All cueing disabled
> const uint16_t ACTIVITY_EVENT_CUE_CONFIGURATION   = 0x0800;  // @b11 Cue: new configuration written
> const uint16_t ACTIVITY_EVENT_CUE_OPENED          = 0x1000;  // @b12 (TBD?) Cue: user opened app
> const uint16_t ACTIVITY_EVENT_CUE_MANUAL          = 0x2000;  // @b13 Cue: temporary manual cueing in use
> const uint16_t ACTIVITY_EVENT_CUE_SNOOZE          = 0x4000;  // @b14 Cue: temporary manual snooze in use
> const uint16_t ACTIVITY_EVENT_RESERVED            = 0x8000;  // @b15 (Reserved)
> ```


## Additional Feature: UART

Exposing a simple "UART" communication as an alternative communications channel to access the additional functionality.


### UART BLE Service

User subscribes to notifications on the device's *TX* channel to receive response data, and sends packets to the device's *RX* channel for commands -- these commands initiate a response from the device.

#### Service: UART

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | UART Service                                     |
| UUID                          | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`           |

#### Characteristic: UART Rx

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | UART Rx Characteristic                           |
| UUID                          | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`           |
| Write `uint8_t[<=20]`         | Send data to device.                             |

#### Characteristic: UART Tx

| Name                          | Value                                            |
|-------------------------------|--------------------------------------------------|
| Name                          | UART Tx Characteristic                           |
| UUID                          | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`           |
| Notification `uint8_t[<=20]`  | Subscribe to notifications to receive response.  |


### UART Communcation Protocol

Generally a subset compatible with the [Open Movement](https://openmovement.dev) [AxLE device](https://github.com/digitalinteraction/OpenMovement-AxLE-Firmware) protocol (with some extensions for the [TwoCan](https://twocan.dev/) project).  Some of the outbound numeric parameters are (for backwards-compatibility) hex-encoded bytewise little-endian representations of the integers, producing the 4-bit nibble indexes:

  * `INT16HEX`: `1032`
  * `INT32HEX`: `10325476`

Commands and responses are terminated with a final line-feed (`\n`), which may be immediately preceeded with a carriage-return (`\r`) that can be ignored.  

A response may be prefixed with:

  * `?`: Where an error has occurred.  A brief explanation may follow the `?`, or `?!` indicates a syntax error.
  * `!`: If UART-based security is used and the session has insufficient permission for this command.


#### UART Commands

* `#` - Device ID query
  > `AP:<application_type>,<cueband_version>`
  > `#:<12-hex-digit Bluetooth address>`

* `0` - Outputs (motor) off
  > OFF

* `1` - Vibrate motor (50 ms)
  > `MOT`

* `A` - Accelerometer single sample
  > `A:<Ax>,<Ay>,<Az>,<reg=00>,<chip=BMA421|BMA425|Unknown>,<ee_level=0>`

* `B` - Battery
  > `B:<battery_percentage>%`

* `E` - Erase
  > `Erase all`

* `I <rate=50> <range=8>` - Stream sensor data
  > `OP:<mode=00>, <rate>, <range>`
  >
  > `<stream_packet>`
  >
  > ...

  Where `rate` is in Hz, and `range` in ±*g*.

  Each `stream_packet` response line is base-16 (hex-encoded) and, once decoded to binary, of the format:

  ```c
  struct {
      uint32_t timestamp;
      uint16_t battery;
      int16_t temperature;
      accel_sample[25] accelerometer;
  }
  ```

  The receiver should cope with a flexible length for the `accelerometer` array determined from:  floor((*packet_length* - 8) / 6)

  The `timestamp` is in units of 1/32768 seconds and, for backwards-compatibility, you should consider only the lower 24-bits.

  The `battery` value has its lower 7-bits representing an estimated percentage (or 127 if not known), and the upper 9-bits representing the voltage in units of 0.01 V (or 0 if not known).

  The `temperature` value is of the scale 0.25°C, and `0xffff` if unknown (therefore, -0.25°C must be represented as 0°C).

  The `accel_sample` is a 6-byte structure of:

  ```c
  struct {
      int16_t accel_x;  // @0
      int16_t accel_y;  // @2
      int16_t accel_z;  // @4
  } // @6
  ```

  ...and the accelerometer units are: 1 *g* = 4096.

  Streaming ends when any other packet is sent to the device.

* `J <interval=0> <maximum_runtime=4294967295> <motor_pulse_width=50>` - Set temporary queueing interval (seconds)
  > `J:<interval>,<maximum_runtime>,<motor_pulse_width>`

* `M` - Motor on (100 ms)
  > `MOT`

* `N` - Epoch interval (read-only)
  > `N:60`

* `T` - Query time
  > `T:<seconds_since_epoch_year_2000>`

* `T$<YY>/<MM>/<DD>,<hh>:<mm>:<ss>` - Set time. Also accepts variations such as: `T$<YYYY>-<MM>-<DD> <hh>:<mm>` or `T$<YY>-<MM>-<DD> <hh>:<mm>:<ss>`.
  > `T:<seconds_since_epoch_year_2000>`

* `Q` - Query status (mostly AxLE-compatible)
  > `T:<seconds_since_epoch_year_2000>`
  > `B:<active_block_id>`
  > `N:<epoch_index>`
  > `E:<block_timestamp>`
  > `C:<block_count_available>`
  > `I:<read_block_id>`

* `R<id>` - Read block id (Base-16 hex encoded) -- can be interpreted as `activity_log` (see above: *Device Activity Log Block Format*).

* `R+<id>` - Read block id (Base-64 encoded) -- can be interpreted as `activity_log` (see above: *Device Activity Log Block Format*).

* `X!` - Remotely reset device (risky?)
  > (resets device)

* `XV?` - Query firmware validation
  > `XV:<firmware_validated>`
  ...where `firmware_validated` is `0` if not validated, or `1` if validated.

* `XV!` - Remotely validate firmware (risky)
  > `XV:1`

* `XW0` - Remotely sleep device
  > `XW:0`

* `XW1` - Remotely wake device
  > `XW:1`


<!--

Control point commands used in TC project:

* `KQ` - Prompt query
  > `KQ:<version>,<window-size>,<minimum-interval>,<image>,<max-controls>`

* `KC VVVV` - Prompt clear (if not on specified version `VVVV`, or version=0 for always clear)
  > `KC:<version>`

* `KA II DD TTTT VVVV` - Prompt add control point (`I`=index, `D`=day mask, `T`=time of day minutes, `V`=threshold value)
  > `KA:?` - format
  > `KA:+` - added
  > `KA:!` - failed to add

* `KR II` - Prompt read (`I`=index)
  > `KR:<index>,<day-mask>,<time-minutes>,<threshold-value>`
  > `KR:!` - failed

* `KS VVVV WW II GG` - Prompt save control points (V=version, W=window size, I=minimum interval, G=image)
  > `KS:<version>,<window-size>,<minimum-interval>,<image>`

-->


<!--

## Task list


* [ ] ...(move task list here)...

-->
