"""Pinned, guarded SE5 FIFO recovery using the original firmware loader."""
from pathlib import Path
import hashlib
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/I2CDxe/I2CDxe.efi'

def branch(pc, target, link=False):
    delta = target - pc
    assert delta % 4 == 0 and -(1 << 27) <= delta < (1 << 27)
    return (0x94000000 if link else 0x14000000) | ((delta // 4) & 0x3ffffff)

def patched():
    original = SOURCE.read_bytes()
    assert hashlib.sha256(original).hexdigest() == 'f56f072d194ad13f0f38e5bda6fa599a602c3dac46c30353c3830db80af353e8'
    data = bytearray(original)
    # Hash-pinned .text maps RVA=raw offset, through 0xbfff. This tail is zero.
    cave, limit = 0xb800, 0xc000
    assert data[cave:limit] == bytes(limit-cave)
    # Enter only the existing rejection path. W8 still holds the read at 59f0.
    assert struct.unpack_from('<I', data, 0x59f0)[0] == 0xb9406508
    assert struct.unpack_from('<I', data, 0x5a28)[0] == branch(0x5a28, 0x59c8)
    payload = original[0xc1a8:0xc1a8+0x6d1]
    assert hashlib.sha256(payload).hexdigest() == '311a91dce7f1c6c38f52f885f3f1b7da06b8da09507acb1e2a47a171f59a13de'
    symbols = dict(stock_clock_on=0x5c14, stock_clock_off=0x5cfc,
                   stock_success=0x59f8,
                   stock_failure=0x59c8, stock_loader_body=0x6eb4)
    with tempfile.TemporaryDirectory(prefix='elden-i2c-') as directory:
        obj, elf, binary = [Path(directory)/name for name in ('hook.o', 'hook.elf', 'hook.bin')]
        subprocess.run(['clang', '--target=aarch64-none-elf', '-c',
                        str(ROOT/'custom-build/elden-i2c-fifo-recover.S'), '-o', str(obj)], check=True)
        subprocess.run(['ld.lld', '--image-base=0', '-Ttext=0xb800', '-e', '_start',
                        *[f'--defsym={key}={value:#x}' for key,value in symbols.items()],
                        str(obj), '-o', str(elf)], check=True)
        subprocess.run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', str(elf), str(binary)], check=True)
        hook = binary.read_bytes()
    assert len(hook) <= limit-cave
    data[cave:cave+len(hook)] = hook
    struct.pack_into('<I', data, 0x5a18, branch(0x5a18, cave))
    assert len(data) == len(original)
    assert all(a == b or 0x5a18 <= i < 0x5a1c or cave <= i < limit
               for i, (a, b) in enumerate(zip(original, data)))
    return bytes(data)

if __name__ == '__main__':
    destination = ROOT / 'Build/EldenDiagnostic/I2CDxe.efi'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(patched())
    print('PASS: pinned SE5 FIFO recovery; idle/protocol/DMA/GSI guards retained')
