import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.python_serial_tester import score_port


def test_score_usb_single_serial_acm():
  score = score_port(
    "usb-1a86_USB_Single_Serial_58FB009763-if00",
    "/dev/ttyACM0",
    "1a86",
    "55d3",
  )
  assert score == 180


def test_score_ch340_usb_serial():
  score = score_port(
    "usb-1a86_USB_Serial-if00-port0",
    "/dev/ttyUSB0",
    "1a86",
    "7523",
  )
  assert score == -10
