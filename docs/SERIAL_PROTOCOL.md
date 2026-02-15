# 📡 ESP32 Serial Protocol (Production Reference)

## Overview
Transport: USB serial, newline-delimited JSON (one JSON per line). Each line must end with `\n`.

Default baud: `115200` (configurable in `esp32_firmware/src/esp32.ino`).

Key rules:
- Valid JSON per line, terminated by `\n`.
- `timestamp_ms` is integer milliseconds since boot.
- `freq_hz` is always in Hertz.
- Every command returns a `command_ack`.

## Framing & Parsing
- Byte-by-byte serial parsing
- Overlong lines dropped, error counters incremented
- Keys are searched in the line; `args` object is accepted but not required

## Common Fields
- `type`: message type (`telemetry`, `command_ack`, `log_event`)
- `timestamp_ms`: integer milliseconds since boot

## ESP32 → Pi Messages

### Telemetry
```json
{
  "type": "telemetry",
  "timestamp_ms": 123456,
  "sel": 1,
  "vrx": [
    {"id": 1, "freq_hz": 5740000000, "rssi_raw": 219},
    {"id": 2, "freq_hz": 5800000000, "rssi_raw": 140},
    {"id": 3, "freq_hz": 5860000000, "rssi_raw": 98}
  ],
  "video": {"selected": 1},
  "led": {"r": 0, "y": 1, "g": 0},
  "sys": {"uptime_ms": 123456, "heap": 123456}
}
```

### Command Ack
```json
{
  "type": "command_ack",
  "timestamp_ms": 123456,
  "id": "<same id>",
  "ok": true,
  "err": null,
  "data": {"cmd": "<COMMAND>"}
}
```

## Pi → ESP32 Commands
All commands MUST include:
```json
{"id":"<string>","cmd":"<COMMAND>","args":{}}
```

### SET_VRX_FREQ
```json
{"id":"1","cmd":"SET_VRX_FREQ","args":{"vrx_id":1,"freq_hz":5740000000}}
```

### START_SCAN
```json
{"id":"2","cmd":"START_SCAN","args":{"dwell_ms":200,"step_hz":2000000,"start_hz":5645000000,"stop_hz":5865000000}}
```

### STOP_SCAN
```json
{"id":"3","cmd":"STOP_SCAN","args":{}}
```

### LOCK_STRONGEST
```json
{"id":"4","cmd":"LOCK_STRONGEST","args":{"vrx_id":1}}
```
If `vrx_id` is omitted, all VRXs are locked to their strongest.

### VIDEO_SELECT
```json
{"id":"5","cmd":"VIDEO_SELECT","args":{"ch":2}}
```

### SET_LEDS
```json
{"id":"6","cmd":"SET_LEDS","args":{"r":1,"y":0,"g":1}}
```

### GET_STATUS
```json
{"id":"7","cmd":"GET_STATUS","args":{}}
```

## Error Codes (`err` field)
- `missing_id`
- `missing_cmd`
- `missing_args`
- `args_oob`
- `unknown_cmd`
- `not_implemented`
- `vrx_id_oob`
- `freq_oob`
- `dwell_oob`
- `range_oob`
- `no_best`
- `ch_oob`
