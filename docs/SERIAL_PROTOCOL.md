# Serial Protocol (Draft)

Transport: USB serial, newline-delimited JSON (one JSON per line). Default baud: 115200.

Rules:
- Every message must be valid JSON and end with `\n`.
- `timestamp_ms` is an integer (milliseconds since boot).
- Commands always receive a `command_ack` response.

Message types (ESP32 -> Pi):
- `telemetry`
- `command_ack`
- `log_event` (optional)

Command format (Pi -> ESP32):
```json
{"id":"<string>","cmd":"<COMMAND>","args":{}}
```

Ack format (ESP32 -> Pi):
```json
{"type":"command_ack","timestamp_ms":123,"id":"<same id>","ok":true,"err":null,"data":{}}
```
