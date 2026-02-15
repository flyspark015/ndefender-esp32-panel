# ESP32 Serial Protocol (Production Reference)

## Overview

Transport: USB serial, newline-delimited JSON (one JSON object per line). Each line must be valid JSON and end with `\n`.

Default baud: `115200` (configurable in `esp32_firmware/include/config.h`).

Key rules:
- Every message must be valid JSON and terminated by `\n`.
- `timestamp_ms` is an integer (milliseconds since boot).
- `freq_hz` is in Hertz (never MHz).
- Every command must receive a `command_ack` response.

## Framing & Parsing

- The ESP32 reads serial data byte-by-byte and splits on newline.
- Carriage returns (`\r`) are ignored.
- Overlong lines are dropped and counted as errors.
- Non-JSON lines are rejected with an error counter increment.

## Common Fields

- `type`: string identifying message type (`telemetry`, `command_ack`, `log_event`).
- `timestamp_ms`: integer timestamp in milliseconds since boot.

## ESP32 -> Pi Messages

### Telemetry

Schema:
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

Notes:
- `rssi_raw` is required. `rssi_dbm_est` is optional (future).
- `vrx[]` and `video` will be populated in later phases.

### Command Ack

Schema:
```json
{
  "type": "command_ack",
  "timestamp_ms": 123456,
  "id": "<same id>",
  "ok": true,
  "err": null,
  "data": {"optional": "object"}
}
```

### Log Event (optional)

Schema:
```json
{
  "type": "log_event",
  "timestamp_ms": 123456,
  "level": "info|warn|error",
  "msg": "string",
  "data": {"optional": "object"}
}
```

## Pi -> ESP32 Commands

All commands MUST include:
```json
{"id":"<string>","cmd":"<COMMAND>","args":{}}
```

### Required Commands

#### SET_VRX_FREQ
Args:
```json
{"vrx_id": 1, "freq_hz": 5740000000}
```

#### START_SCAN
Args:
```json
{"profile":"string","dwell_ms":80,"step_hz":2000000,"start_hz":5645000000,"stop_hz":5945000000}
```

#### STOP_SCAN
Args:
```json
{}
```

#### LOCK_STRONGEST
Args:
```json
{"vrx_id": 1}
```

#### VIDEO_SELECT
Args:
```json
{"ch": 1}
```

#### SET_LEDS
Args:
```json
{"r":1,"y":0,"g":1}
```

#### GET_STATUS
Args:
```json
{}
```

## Error Codes (Ack `err` field)

- `missing_id`
- `missing_cmd`
- `missing_args`
- `args_oob`
- `unknown_cmd`
- `not_implemented`

## Current Implementation Status

- Implemented: `GET_STATUS`, `SET_LEDS`, robust framing, `command_ack` for all commands.
- Not implemented yet (planned): `SET_VRX_FREQ`, `START_SCAN`, `STOP_SCAN`, `LOCK_STRONGEST`, `VIDEO_SELECT`.

## Examples

Command:
```json
{"id":"1","cmd":"SET_LEDS","args":{"r":1,"y":0,"g":1}}
```

Ack:
```json
{"type":"command_ack","timestamp_ms":1234,"id":"1","ok":true,"err":null,"data":{"cmd":"SET_LEDS"}}
```
