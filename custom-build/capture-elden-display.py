#!/usr/bin/env python3
"""Read display handoff evidence from stock Android; never flash/reboot/write it."""
import argparse
import json
import re
import shlex
import shutil
import struct
import subprocess
from pathlib import Path


def device_command(adb, serial, root, words):
    command = [adb] + (["-s", serial] if serial else []) + ["exec-out"]
    # Legacy exec-out can merge remote stderr into stdout and report host rc=0
    # even when cat failed. Frame the REMOTE status after the exact binary bytes.
    script = (shlex.join(words) + '; _elden_capture_rc=$?; '
              'printf "\\n__ELDEN_CAPTURE_RC__=%s\\n" "$_elden_capture_rc"; '
              'exit "$_elden_capture_rc"')
    return command + [shlex.join(["su" if root else "sh", "-c", script])]


def unpack_capture(data):
    match = re.search(rb"\n__ELDEN_CAPTURE_RC__=([0-9]{1,3})\n$", data)
    if match is None:
        raise ValueError("missing remote exit-status trailer")
    return data[:match.start()], int(match[1])


def payload_error(filename, data):
    """Reject diagnostic text saved by older exec-out even with host rc=0."""
    if not data:
        return "empty response"
    if data.lstrip().startswith((b"cat:", b"dmesg:", b"sh:", b"su:", b"/system/bin/sh:")):
        return "remote error text, not captured data"
    if filename == "live.dtb" and not valid_fdt(data):
        return "invalid/truncated FDT"
    if filename == "splash-reg.bin" and len(data) != 16:
        return "expected 4 big-endian cells"
    if filename == "uefi-display-info.bin" and len(data) != 124:
        return "expected efivar attributes (4 bytes) + UEFIDisplayInfo (120 bytes)"
    if filename in ("fb-bpp.txt", "fb-stride.txt") and not re.fullmatch(rb"[1-9][0-9]*\s*", data):
        return "expected a positive integer"
    if filename == "fb-virtual-size.txt" and not re.fullmatch(rb"[1-9][0-9]*,[1-9][0-9]*\s*", data):
        return "expected width,height"
    if filename == "slot.txt" and data.strip() not in (b"_a", b"_b"):
        return "unexpected slot suffix"
    return None


def valid_fdt(data):
    if len(data) < 40:
        return False
    magic, total = struct.unpack_from(">II", data)
    return magic == 0xD00DFEED and 40 <= total == len(data) <= 16 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", help="select one adb device")
    parser.add_argument("--root", action="store_true", help="use existing su for protected reads; does not install root")
    parser.add_argument("--output", type=Path, required=True, help="new host directory; existing directories are refused")
    parser.add_argument("--dry-run", action="store_true", help="print planned commands without accessing device or writing files")
    args = parser.parse_args()
    reads = [
        ("fingerprint.txt", False, ["getprop", "ro.build.fingerprint"]),
        ("device.txt", False, ["getprop", "ro.product.device"]),
        ("slot.txt", False, ["getprop", "ro.boot.slot_suffix"]),
        ("live.dtb", args.root, ["cat", "/sys/firmware/fdt"]),
        ("splash-reg.bin", args.root, ["cat", "/sys/firmware/devicetree/base/reserved-memory/splash_region/reg"]),
        ("bootargs.bin", args.root, ["cat", "/sys/firmware/devicetree/base/chosen/bootargs"]),
        ("fb-name.txt", args.root, ["cat", "/sys/class/graphics/fb0/name"]),
        ("fb-virtual-size.txt", args.root, ["cat", "/sys/class/graphics/fb0/virtual_size"]),
        ("fb-stride.txt", args.root, ["cat", "/sys/class/graphics/fb0/stride"]),
        ("fb-bpp.txt", args.root, ["cat", "/sys/class/graphics/fb0/bits_per_pixel"]),
        ("uefi-display-info.bin", args.root, ["cat", "/sys/firmware/efi/efivars/UEFIDisplayInfo-9042a9de-23dc-4a38-96fb-7aded080516a"]),
        ("display-dmesg.txt", args.root, ["dmesg"]),
    ]
    if args.dry_run:
        for _, root, words in reads:
            print(shlex.join(device_command("adb", args.serial, root, words)))
        return 0
    adb = shutil.which("adb")
    if not adb:
        parser.error("adb not found")
    if args.output.exists():
        parser.error("output already exists; choose a new directory")
    command = [adb] + (["-s", args.serial] if args.serial else []) + ["get-state"]
    result = subprocess.run(command, capture_output=True, timeout=20)
    if result.returncode or result.stdout.strip() != b"device":
        parser.error("stock Android must be booted and adb authorized; " + result.stderr.decode(errors="replace"))
    args.output.mkdir(parents=True, mode=0o700)
    report = {"root_requested": args.root, "device_writes": [], "reads": [],
              "warning": "Android fb0 describes current Android, not necessarily the bootloader framebuffer. No parameters approved for firmware writes."}
    for filename, root, words in reads:
        command = device_command(adb, args.serial, root, words)
        try:
            result = subprocess.run(command, capture_output=True, timeout=30)
            entry = dict(file=filename, command=command, returncode=result.returncode,
                         stderr=result.stderr.decode(errors="replace"))
            data = result.stdout
            try:
                data, remote_rc = unpack_capture(data)
                entry["remote_returncode"] = remote_rc
                if result.returncode != 0 or remote_rc != 0:
                    entry["error"] = "host or remote command failed"
                else:
                    error = payload_error(filename, data)
                    if error:
                        entry["error"] = error
            except ValueError as error:
                entry["error"] = str(error)
            if "error" not in entry and filename == "display-dmesg.txt":
                terms = (b"splash", b"framebuffer", b"mdss", b"dpu", b"sde", b"dsi", b"panel")
                lines = [line for line in data.splitlines() if any(term in line.lower() for term in terms)]
                if not lines:
                    entry["error"] = "dmesg read succeeded but no display messages matched"
                else:
                    data = b"\n".join(lines) + b"\n"
            if "error" in entry:
                entry["stdout_preview"] = data[:256].decode(errors="replace")
            else:
                (args.output / filename).write_bytes(data)
                entry["saved_bytes"] = len(data)
                if filename == "splash-reg.bin":
                    address, size = struct.unpack(">QQ", data)
                    entry["address"] = hex(address)
                    entry["size"] = hex(size)
            report["reads"].append(entry)
            print(("OK: " if "saved_bytes" in entry else "UNAVAILABLE: ") + filename, flush=True)
        except subprocess.TimeoutExpired:
            report["reads"].append(dict(file=filename, command=command, error="timeout"))
            print("UNAVAILABLE (timeout): " + filename, flush=True)
    dtc = shutil.which("dtc")
    live = args.output / "live.dtb"
    if dtc and live.exists():
        result = subprocess.run([dtc, "-I", "dtb", "-O", "dts", str(live)], capture_output=True, timeout=30)
        (args.output / "dtc-stderr.txt").write_bytes(result.stderr)
        if result.returncode == 0:
            (args.output / "live.dts").write_bytes(result.stdout)
    (args.output / "report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print("Host output:", args.output.resolve())
    print("No device files, partitions, power settings or boot state were modified.")
    if not live.exists():
        print("Live FDT unavailable. Share report.json; do not assume missing values are zero.")
        return 2
    print("Capture complete, not framebuffer validation. Bootargs/logs may contain device identifiers; do not post publicly without reviewing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
