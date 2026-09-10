"""Offline experiment ONLY: bypass post-publication PMIC policy.

Not called by build-elden.sh. Resource initialization and ADC can still write
hardware. This is not a validated service-only PMIC driver or a flash artifact.
"""
from pathlib import Path
import argparse
import hashlib
import struct

SOURCE_SHA = '7203955b1a6dbda18186ca45fad129a7b0ad7c2f4f15b306c47f50c0dd3deb7c'


def candidate(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA:
        raise ValueError('unsupported PMIC image')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    section = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    for i in range(count):
        _, va, size, raw = struct.unpack_from('<IIII', data, section + i*40 + 8)
        if va <= 0x1678 and 0x167c <= va + size:
            offset = raw + 0x1678 - va
            break
    else:
        raise ValueError('branch not backed by raw section')
    # CBZ W8: OR of both publication statuses. Only the all-zero path changes.
    # W21 still holds zero from the first publication; failure paths untouched.
    old = 0x34000008 | (((0x16d8 - 0x1678)//4) << 5)
    new = 0x34000008 | (((0x16f8 - 0x1678)//4) << 5)
    if struct.unpack_from('<I', data, offset)[0] != old:
        raise ValueError('unexpected success branch')
    result = bytearray(data)
    struct.pack_into('<I', result, offset, new)
    return bytes(result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / 'Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/PmicDxe/PmicDxeLa.efi'
    result = candidate(source.read_bytes())
    # Never overwrite the original, a previous candidate, or a build artifact.
    with args.output.open('xb') as output:
        output.write(result)
    print('OFFLINE UNVALIDATED CANDIDATE:', args.output)
    print('SHA256:', hashlib.sha256(result).hexdigest())
