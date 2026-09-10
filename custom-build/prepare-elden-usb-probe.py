"""Elden redriver exclusion plus defensive read-only probe; no forced success.

The v8 snapshot targets I2C 0x1c, unlike Elden's USB2 repeater at 0x4f.
Select the stock disabled-feature path at A1B4, returning FALSE. This is
not a speed-limit, host-role, PHY, or power implementation.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/UsbConfigDxe/UsbConfigDxe.efi'

def patched():
    original = SOURCE.read_bytes()
    assert hashlib.sha256(original).hexdigest() == '11d81930099a11fe7de8fddb8c2548a70acb2d3fb2c43b0ff94390ed3b8a4325'
    cave, limit, site = 0x12eb0, 0x13000, 0xae7c
    assert original[cave:limit] == bytes(limit-cave)
    assert struct.unpack_from('<I', original, site)[0] == 0x97ffff37
    with tempfile.TemporaryDirectory(prefix='elden-usb-probe-') as directory:
        obj, elf, raw = [Path(directory)/n for n in ('probe.o','probe.elf','probe.bin')]
        subprocess.run(['clang','--target=aarch64-none-elf','-c',str(ROOT/'custom-build/elden-usb-config-probe.S'),'-o',str(obj)],check=True)
        subprocess.run(['ld.lld','--image-base=0','-Ttext=0x12eb0','-e','_start','--defsym=stock_failure=0xae94',str(obj),'-o',str(elf)],check=True)
        subprocess.run(['llvm-objcopy','-O','binary','--only-section=.text',str(elf),str(raw)],check=True)
        hook = raw.read_bytes()
    assert len(hook) <= limit-cave
    data = bytearray(original)
    # Only suppress the board redriver feature test. Preserve the stock
    # CBZ/return-FALSE path and all USB2 repeater, power and IOMMU code.
    feature_site = 0xa1b8
    assert struct.unpack_from('<I', original, feature_site)[0] == 0x39582108
    struct.pack_into('<I', data, feature_site, 0x2a1f03e8)  # mov w8, wzr
    data[cave:cave+len(hook)] = hook
    struct.pack_into('<I',data,site,0x14000000 | ((cave-site)//4))
    assert len(data)==len(original)
    assert all(a==b or feature_site<=i<feature_site+4 or site<=i<site+4 or cave<=i<limit for i,(a,b) in enumerate(zip(original,data)))
    return bytes(data)

if __name__ == '__main__':
    dest=ROOT/'Build/EldenDiagnostic/UsbConfigDxe.efi'
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_bytes(patched())
    print('PASS: Elden redriver uses stock disabled path; defensive I2C probe retained')
