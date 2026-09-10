"""Apply the hash-pinned Elden UFS fixes without instrumenting the stock PE."""
from pathlib import Path
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/UFSDxe/UFSDxe.efi'
SHA256 = 'dac620aca276e735361d8b747fe0218b6125eae004116391270f32a72e6ce750'


def patched():
    data = bytearray(SOURCE.read_bytes())
    assert hashlib.sha256(data).hexdigest() == SHA256
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    sections = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]

    def offset(rva):
        for i in range(count):
            section = sections + i * 40
            size, va, rawsize, raw = struct.unpack_from('<IIII', data, section + 8)
            if va <= rva and rva + 4 <= va + rawsize:
                return raw + rva - va
        raise AssertionError(f'RVA {rva:#x} not backed by section')

    # A NULL handle must return EFI_DEVICE_ERROR through the existing epilogue.
    loc = offset(0x42d4)
    assert data[loc:loc + 4] == bytes.fromhex('a10000f0')
    data[loc:loc + 4] = struct.pack('<I',
                                    0x14000000 | (((0x42b8 - 0x42d4) // 4) & 0x03ffffff))

    # Do not inherit the old UFS link state from the boot firmware.
    loc = offset(0x19894)
    assert data[loc:loc + 4] == bytes.fromhex('69060039')
    data[loc:loc + 4] = bytes.fromhex('7f060039')

    # HCI 4.0 uses UFS_STORAGE command type, including NOP and QUERY.
    loc = offset(0x10c9c)
    assert struct.unpack_from('<I', data, loc)[0] == 0x53040d08
    struct.pack_into('<I', data, loc, 0x52a20008)

    # Keep the firmware's active stream mapping; preserve configure errors.
    loc = offset(0x1b170)
    assert struct.unpack_from('<I', data, loc)[0] == 0x340001a0
    struct.pack_into('<I', data, loc,
                     0x34000000 | (((0x1b1d4 - 0x1b170) // 4) << 5))

    # Propagate a failed listener allocator lookup rather than dereference NULL.
    loc = offset(0x137c0)
    assert struct.unpack_from('<I', data, loc)[0] == 0x90000041
    struct.pack_into('<I', data, loc,
                     0x14000000 | ((0x1382c - 0x137c0) // 4))
    return bytes(data)


if __name__ == '__main__':
    out = ROOT / 'Build/EldenDiagnostic/UFSDxe.efi'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(patched())
    print('Prepared Elden UFS driver:', hashlib.sha256(out.read_bytes()).hexdigest())
