import json


def test_telemetry_shape():
  line = (
    '{"type":"telemetry","timestamp_ms":1234,"sel":1,'
    '"vrx":[{"id":1,"freq_hz":5740000000,"rssi_raw":219},'
    '{"id":2,"freq_hz":5800000000,"rssi_raw":140},'
    '{"id":3,"freq_hz":5860000000,"rssi_raw":98}],'
    '"video":{"selected":1},'
    '"led":{"r":0,"y":1,"g":0},'
    '"sys":{"uptime_ms":1234,"heap":123456}}'
  )
  data = json.loads(line)
  assert data["type"] == "telemetry"
  assert isinstance(data["timestamp_ms"], int)
  assert isinstance(data["vrx"], list) and len(data["vrx"]) == 3
  for item in data["vrx"]:
    assert "id" in item
    assert "freq_hz" in item
    assert "rssi_raw" in item
  assert data["video"]["selected"] in (1, 2, 3)
  assert set(data["led"].keys()) == {"r", "y", "g"}
  assert "uptime_ms" in data["sys"]


def test_ack_shape():
  line = '{"type":"command_ack","timestamp_ms":1234,"id":"1","ok":true,"err":null,"data":{"cmd":"SET_LEDS"}}'
  data = json.loads(line)
  assert data["type"] == "command_ack"
  assert isinstance(data["timestamp_ms"], int)
  assert data["id"] == "1"
  assert isinstance(data["ok"], bool)
  assert "err" in data
