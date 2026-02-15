#!/usr/bin/env python3
"""ESP32 serial tester with automatic port detection.

Detection algorithm:
1) List /dev/serial/by-id/*
2) Resolve real device path
3) Read udev properties (VID, PID, manufacturer, product, serial)
4) Score each port:
   +100 if VID:PID == 1a86:55d3
   +60  if by-id name contains "USB_Single_Serial"
   +20  if tty is ACM (ttyACM*)
   -10  if VID:PID == 1a86:7523 (CH340)
5) Pick highest score
6) Validate by probing for telemetry JSON
"""

from __future__ import annotations

import argparse
import os
import subprocess
import time
from dataclasses import dataclass
from typing import List, Optional, Tuple

SERIAL_BY_ID = "/dev/serial/by-id"
TELEMETRY_SIGNATURES = (b'"type":"telemetry"', b'"proto":1')


@dataclass
class PortInfo:
  by_id: str
  real: str
  vid: str
  pid: str
  serial: str
  product: str
  vendor: str
  score: int


def run_cmd(cmd: List[str]) -> Tuple[int, bytes, bytes]:
  p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  return p.returncode, p.stdout, p.stderr


def udev_props(dev: str) -> dict:
  rc, out, _ = run_cmd(["udevadm", "info", "-q", "property", "-n", dev])
  props = {}
  if rc != 0:
    return props
  for line in out.splitlines():
    if b"=" not in line:
      continue
    k, v = line.split(b"=", 1)
    props[k.decode("utf-8", errors="ignore")] = v.decode("utf-8", errors="ignore")
  return props


def score_port(by_id_name: str, real_dev: str, vid: str, pid: str) -> int:
  score = 0
  vidpid = f"{vid}:{pid}" if vid and pid else ""
  if vidpid.lower() == "1a86:55d3":
    score += 100
  if "USB_Single_Serial" in by_id_name:
    score += 60
  if os.path.basename(real_dev).startswith("ttyACM"):
    score += 20
  if vidpid.lower() == "1a86:7523":
    score -= 10
  return score


def list_ports() -> List[PortInfo]:
  ports: List[PortInfo] = []
  if not os.path.isdir(SERIAL_BY_ID):
    return ports
  for name in sorted(os.listdir(SERIAL_BY_ID)):
    by_id = os.path.join(SERIAL_BY_ID, name)
    try:
      real = os.path.realpath(by_id)
    except OSError:
      continue
    props = udev_props(real)
    vid = props.get("ID_VENDOR_ID", "")
    pid = props.get("ID_MODEL_ID", "")
    serial = props.get("ID_SERIAL_SHORT", props.get("ID_SERIAL", ""))
    product = props.get("ID_MODEL", "")
    vendor = props.get("ID_VENDOR", "")
    score = score_port(name, real, vid, pid)
    ports.append(PortInfo(by_id, real, vid, pid, serial, product, vendor, score))
  ports.sort(key=lambda p: p.score, reverse=True)
  return ports


def stty_raw(dev: str) -> None:
  run_cmd(["stty", "-F", dev, "115200", "raw", "-echo"])


def probe_port(dev: str, seconds: float = 2.0) -> bool:
  stty_raw(dev)
  timeout_cmd = ["timeout", str(seconds), "cat", dev]
  rc, out, _ = run_cmd(timeout_cmd)
  if rc not in (0, 124):  # 124 is timeout exit code
    return False
  for sig in TELEMETRY_SIGNATURES:
    if sig in out:
      return True
  return False


def detect_port() -> Optional[PortInfo]:
  ports = list_ports()
  for p in ports:
    if probe_port(p.real):
      return p
  return ports[0] if ports else None


def send_json(dev: str, line: str) -> None:
  stty_raw(dev)
  if not line.endswith("\n"):
    line += "\n"
  with open(dev, "wb", buffering=0) as f:
    f.write(line.encode("utf-8"))


def main() -> int:
  ap = argparse.ArgumentParser(description="ESP32 serial tester with auto detection")
  ap.add_argument("--list", action="store_true", help="List detected serial ports")
  ap.add_argument("--port", default="", help="Override port path")
  ap.add_argument("--send", default="", help="Send raw JSON line")
  ap.add_argument("--video", type=int, default=0, help="Send VIDEO_SELECT ch")
  args = ap.parse_args()

  ports = list_ports()
  if args.list:
    for p in ports:
      print(f"{p.by_id} -> {p.real} vid:pid={p.vid}:{p.pid} score={p.score} serial={p.serial}")
    return 0

  port = args.port
  if not port:
    detected = detect_port()
    if not detected:
      print("No serial ports detected")
      return 2
    port = detected.real

  if args.video:
    payload = f'{{"id":"20","cmd":"VIDEO_SELECT","args":{{"ch":{args.video}}}}}'
    send_json(port, payload)
    time.sleep(1.0)
    return 0

  if args.send:
    send_json(port, args.send)
    time.sleep(0.2)
    return 0

  print(port)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
