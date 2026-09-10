"""Pin stock USB entry evidence and the shipping USB isolation policy."""
from pathlib import Path
import hashlib
import re
import struct
import uuid

root = Path(__file__).resolve().parents[1]
device = root / 'Platforms/PakalaPkg/Device/lenovo-elden'
binary = device / 'Binaries/QcomPkg/Drivers/UsbConfigDxe'
data = (binary / 'UsbConfigDxe.efi').read_bytes()
assert hashlib.sha256(data).hexdigest() == '11d81930099a11fe7de8fddb8c2548a70acb2d3fb2c43b0ff94390ed3b8a4325'
# This image's relevant RVAs equal raw offsets. Check section mapping first.
pe = struct.unpack_from('<I', data, 0x3c)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]
sections = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
for offset in (0x3fc0, 0x3fd4, 0x131f8):
    assert any(va <= offset < va + size and raw == va
               for i in range(count)
               for size, va, rawsize, raw in [struct.unpack_from('<IIII', data, sections + i*40 + 8)])
assert struct.unpack_from('<I', data, 0x3fc0)[0] == 0xd63f0100  # blr x8
assert struct.unpack_from('<I', data, 0x3fd4)[0] == 0xf9400508  # ldr x8,[x8,#8]
required = {str(uuid.UUID(bytes_le=data[0x131f8:0x13208]))}
required.update(('241afae6-885f-4f6c-a7ea-c28eab79c3e5',
                 'b27ae8b1-3e10-4d07-ab5c-eb9a6dc6fa8f'))
depex = (binary / 'UsbConfigDxe.depex').read_bytes()
i = 0
while i < len(depex):
    op = depex[i]
    i += 1
    if op == 2:
        required.add(str(uuid.UUID(bytes_le=depex[i:i+16])))
        i += 16
    else:
        assert op in (3, 8)  # AND, END; no OR/NOT hidden in this expression
fdf = (device / 'DXE.inc').read_text()
block = fdf.split('FILE DRIVER = CD823A4D-', 1)[1].split('SECTION PE32', 1)[0]
names = re.findall(r'g\w+Guid', block)
declarations = (root / 'Platforms/PakalaPkg/PakalaPkg.dec').read_text() + (root / 'MU_BASECORE/MdePkg/MdePkg.dec').read_text()
declarations += (root / 'Platforms/AndromedaPkg/AndromedaPkg.dec').read_text()
actual = set()
for name in names:
    value = re.search(r'\b' + name + r'\s*=\s*\{([^\n]+)', declarations).group(1)
    numbers = [int(n, 16) for n in re.findall(r'0x[0-9a-fA-F]+', value)]
    actual.add(str(uuid.UUID(bytes_le=struct.pack('<IHH8B', *numbers))))
# UsbConfig is deliberately disabled in the shipping Elden profile. The old
# bring-up test expected its dependencies to be enabled, contrary to DXE.inc.
assert re.search(r'SECTION DXE_DEPEX_EXP\s*=\s*\{\s*FALSE\s*\}', block)
assert not actual, 'UsbConfig must not become eligible through protocol discovery'
assert 'MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf' in fdf
assert not re.search(r'^INF .*UsbPwrCtrl', fdf, re.M)
print('PASS: stock USB evidence and disabled UsbConfig entry; not hardware proof')
i2c = (device / 'Binaries/QcomPkg/Drivers/I2CDxe/I2CDxe.efi').read_bytes()
assert hashlib.sha256(i2c).hexdigest() == 'f56f072d194ad13f0f38e5bda6fa599a602c3dac46c30353c3830db80af353e8'
assert i2c[0xc058:0xc068] == data[0x13188:0x13198]
for module in ('TLMMDxe',):
    assert f'/Drivers/{module}/{module}.inf' in fdf
assert 'SECTION PE32 = Build/EldenDiagnostic/I2CDxe.efi' in fdf
power = (device / 'Binaries/QcomPkg/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.efi').read_bytes()
assert hashlib.sha256(power).hexdigest() == 'f168e7fcde1e5c2dc6a25428167c16b91d21d64ad9a3662fe1b15016951c2ef0'
block = fdf.split('FILE DRIVER = 11FAED4C-', 1)[1].split('SECTION PE32', 1)[0]
assert 'FALSE AND' in block, 'power services alone must not enable VBUS policy'
actual = set()
for name in re.findall(r'g\w+Guid', block):
    value = re.search(r'\b' + name + r'\s*=\s*\{([^\n]+)', declarations).group(1)
    numbers = [int(n, 16) for n in re.findall(r'0x[0-9a-fA-F]+', value)]
    actual.add(str(uuid.UUID(bytes_le=struct.pack('<IHH8B', *numbers))))
assert actual == {str(uuid.UUID(bytes_le=power[o:o+16])) for o in (0x80b8, 0x80c8, 0x80d8)}
def branch_target(data, offset):
    word = struct.unpack_from('<I', data, offset)[0]
    assert word >> 26 == 0b100101  # BL immediate
    immediate = word & 0x3ffffff
    if immediate & 0x2000000:
        immediate -= 0x4000000
    return offset + immediate * 4

# Entry -> initial port policy -> SetVbusEn. The latter identifies itself
# with the stock string at 0x727c. This is not a read-only provider entry.
label = b'UsbPwrCtrlLib_SetVbusEn\0'
assert label == power[0x727c:0x727c + len(label)]
assert branch_target(power, 0x4184) == 0x4dec
print('PASS: original USB power PE, explicit entry lock retained despite providers')
