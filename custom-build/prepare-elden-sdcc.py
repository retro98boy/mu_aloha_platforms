"""Prepare the hash-pinned, removable-only SDCC driver without trace hooks."""
from pathlib import Path
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/SdccDxe/SdccDxe.efi'
SHA256 = '3ada699bb95848b606cc6de338b237d8b69439512338326b96906017a07da8bc'


def patched():
    data = bytearray(SOURCE.read_bytes())
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError('SdccDxe source changed; re-audit before patching')

    replacements = {
        # Use PIO per transfer; retain the original capability and wait paths.
        0xbe10: (0x39005a89, 0x39005a9f),
        # Do not request 1.8 V during either ACMD41 path.
        0xe85c: (0x321127f5, 0x321123f5),
        0xe874: (0x72a83ff5, 0x72a81ff5),
        # Skip embedded MMC/RPMB, initialize the removable card only.
        0x3874: (0x94000a31, 0x14000000 | ((0x3c88 - 0x3874) // 4)),
        # Expose a read-only BlockIo interface.
        0x181c: (0xa9ba7bfd, 0xd2800100),
        0x1820: (0xa9016ffc, 0xf2f00000),
        0x1824: (0xa90267fa, 0xd65f03c0),
    }
    for offset, (old, new) in replacements.items():
        if struct.unpack_from('<I', data, offset)[0] != old:
            raise ValueError(f'instruction mismatch at {offset:x}')
        struct.pack_into('<I', data, offset, new)
    return bytes(data)


if __name__ == '__main__':
    target = ROOT / 'Build/EldenDiagnostic/SdccDxe.efi'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(patched())
    print('Prepared removable-only SDCC driver')
