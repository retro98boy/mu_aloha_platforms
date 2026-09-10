#!/usr/bin/env python3
"""Read-only positive/negative checks of the packaged Elden image."""
import gzip
import importlib.util
import struct
import sys
from pathlib import Path

script = Path(__file__).with_name("package-elden-flash.py")
spec = importlib.util.spec_from_file_location("elden_package", script)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
directory = Path(sys.argv[1]) if len(sys.argv) > 1 else package.ROOT.parent / "porting-artifacts/elden-aloha-v1-flash"
data = (directory / "boot.img").read_bytes()
stock = (directory / "stock/boot.img").read_bytes()
meta = package.avb(data)
package.check_hash(data, meta["hashes"]["boot"])
kernel, ramdisk, _ = package.boot_header(data)
package.require(ramdisk == 0, "boot ramdisk")
package.require(data[:8] == stock[:8] and data[12:4096] == stock[12:4096], "header preservation")
package.require(gzip.decompress(data[4096:4096 + kernel]) ==
                (package.ROOT / "BootShim/BootShim.bin").read_bytes() +
                (package.ROOT / "Build/PakalaPkg/DEBUG_CLANGPDB/FV/SM8750_EFI.fd").read_bytes(),
                "current payload")
for name, expected in package.STOCK_HASHES.items():
    package.require(package.sha((directory / "stock" / (name + ".img")).read_bytes()) == expected,
                    "stock copy: " + name)


def rejected(label, operation):
    try:
        operation()
    except ValueError:
        print("PASS: rejected", label)
    else:
        raise SystemExit("FAIL: accepted " + label)


bad = bytearray(data)
bad[4096] ^= 1
rejected("corrupt kernel digest", lambda: package.check_hash(bad, meta["hashes"]["boot"]))
bad = bytearray(data)
bad[-64:-60] = b"BAD!"
rejected("corrupt footer magic", lambda: package.avb(bad))
bad = bytearray(data)
struct.pack_into(">Q", bad, len(bad) - 44, len(bad))
rejected("out-of-bounds vbmeta", lambda: package.avb(bad))
bad = bytearray(data)
struct.pack_into("<I", bad, 40, 3)
rejected("wrong boot header version", lambda: package.boot_header(bad))
bad = bytearray(data)
struct.pack_into("<I", bad, 8, len(bad))
rejected("oversized kernel", lambda: package.boot_header(bad))
print("PASS: header, current FD/shim, stock copies and AVB self-hash; 5 negative checks")
print("No device accessed; no Lenovo signature or hardware boot claim.")
