"""Pin PMIC initialization boundaries; do not execute or patch the image."""
from pathlib import Path
import hashlib
import struct
import runpy

root = Path(__file__).resolve().parents[1]
device = root / 'Platforms/PakalaPkg/Device/lenovo-elden'
data = (device / 'Binaries/QcomPkg/Drivers/PmicDxe/PmicDxeLa.efi').read_bytes()
assert hashlib.sha256(data).hexdigest() == '7203955b1a6dbda18186ca45fad129a7b0ad7c2f4f15b306c47f50c0dd3deb7c'
pe = struct.unpack_from('<I', data, 0x3c)[0]
sections = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]

def word(rva):
    for i in range(count):
        size, va, rawsize, raw = struct.unpack_from('<IIII', data, sections+i*40+8)
        if va <= rva and rva+4 <= va+rawsize:
            return struct.unpack_from('<I', data, raw+rva-va)[0]
    raise AssertionError(hex(rva))

def call(site, target):
    assert word(site) == 0x94000000 | (((target-site)//4) & 0x3ffffff), hex(site)

# Resource initialization precedes publication; publishing alone is not proof
# that the implementation's internal state is initialized.
call(0x164c, 0x13688)
call(0x1660, 0x1750)
call(0x1670, 0x133e4)
call(0x16dc, 0x13b00)
call(0x16e4, 0x1570)
call(0x16f0, 0x13450)
call(0x170c, 0x42c4)
for site in (0x1346c, 0x1347c, 0x1348c, 0x1349c,
             0x134ac, 0x134bc, 0x134cc, 0x134dc, 0x13514):
    call(site, 0x86b0)
fdf = (device / 'DXE.inc').read_text()
assert not any(line.startswith('INF ') and '/PmicDxe/' in line for line in fdf.splitlines())
print('PASS: pinned PMIC resource/install/policy/ADC boundaries; no execution or patch')

patch = runpy.run_path(str(root / 'custom-build/prepare-elden-pmic-candidate.py'))['candidate']
patched = patch(data)
assert len(patched) == len(data)
assert [i for i, (a, b) in enumerate(zip(data, patched)) if a != b] == [0x1679]
instruction = struct.unpack_from('<I', patched, 0x1678)[0]
assert instruction & 0xff00001f == 0x34000008  # CBZ W8, not unconditional
target = 0x1678 + ((instruction >> 5) & 0x7ffff) * 4
assert target == 0x16f8
for first, second in ((0, 0), (1, 0), (0, 1), (1, 1), (0x8000000e, 0)):
    # W8 = W0 | W21. No publication failure is converted to success.
    assert ((first | second) == 0) == (first == 0 and second == 0)
try:
    patch(bytes(len(data)))
except ValueError:
    pass
else:
    raise AssertionError('unrecognized image accepted')
assert 'prepare-elden-pmic-candidate.py' not in (root / 'custom-build/build-elden.sh').read_text()
print('PASS: offline candidate changes only success destination; not wired into build')
