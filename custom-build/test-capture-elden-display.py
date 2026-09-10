#!/usr/bin/env python3
"""Host-only regression checks; no adb or su is executed."""
import importlib.util
import subprocess
from pathlib import Path

path = Path(__file__).with_name("capture-elden-display.py")
spec = importlib.util.spec_from_file_location("capture", path)
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)

# Exercise exactly the remote shell command, locally, with a binary response.
command = capture.device_command("adb", None, False, ["printf", "\\000\\377\\001\\012"])
result = subprocess.run(["sh", "-c", command[-1]], capture_output=True, check=True)
assert capture.unpack_capture(result.stdout) == (b"\0\xff\x01\n", 0)
command = capture.device_command("adb", None, False, ["sh", "-c", "printf 'cat: Permission denied\\n'; exit 1"])
result = subprocess.run(["sh", "-c", command[-1]], capture_output=True)
assert capture.unpack_capture(result.stdout)[1] == 1
for bad in (b"", b"cat: Permission denied\n", b"hello\n__ELDEN_CAPTURE_RC__=0\nextra"):
    try:
        capture.unpack_capture(bad)
    except ValueError:
        pass
    else:
        raise AssertionError("invalid framing accepted")
for name in ("fb-bpp.txt", "fb-stride.txt", "fb-virtual-size.txt", "uefi-display-info.bin", "bootargs.bin"):
    assert capture.payload_error(name, b"cat: /sys/missing: No such file or directory\n")
assert capture.payload_error("uefi-display-info.bin", b"x" * 111)
assert capture.payload_error("fb-bpp.txt", b"not a number")
assert capture.payload_error("fb-virtual-size.txt", b"952")
assert capture.payload_error("fb-stride.txt", b"0\n")
assert not capture.payload_error("fb-bpp.txt", b"32\n")
assert not capture.payload_error("fb-virtual-size.txt", b"1904,3040\n")
assert not capture.payload_error("slot.txt", b"_a\n")
print("PASS: binary preservation, remote failure, framing and false-success regressions; no device accessed")
