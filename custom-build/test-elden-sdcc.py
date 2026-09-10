"""Static checks for the removable-only SDCC PE; not a hardware boot test."""
import importlib.util
import struct
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('sdcc', ROOT / 'custom-build/prepare-elden-sdcc.py')
sdcc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdcc)
original = sdcc.SOURCE.read_bytes()
image = sdcc.patched()
assert image == sdcc.patched()
assert len(image) == len(original)
assert sdcc.SOURCE.with_suffix('.depex').read_bytes() == (
    b'\x02' + uuid.UUID('241afae6-885f-4f6c-a7ea-c28eab79c3e5').bytes_le +
    b'\x02' + uuid.UUID('26baccb3-6f42-11d4-bce7-0080c73c8881').bytes_le + b'\x03\x08')

replacements = {
    0xbe10: 0x39005a9f, 0xe85c: 0x321123f5, 0xe874: 0x72a81ff5,
    0x3874: 0x14000105, 0x181c: 0xd2800100,
    0x1820: 0xf2f00000, 0x1824: 0xd65f03c0,
}
allowed = set()
for offset, word in replacements.items():
    assert struct.unpack_from('<I', image, offset)[0] == word
    allowed.update(range(offset, offset + 4))
changed = {i for i, (a, b) in enumerate(zip(original, image)) if a != b}
assert changed and changed <= allowed
assert b'ELDEN SD STAGE' not in image
assert b'ELDEN SD INIT raw=' not in image

# The per-transfer PIO flag must still reach all three original consumers.
for offset, word in ((0xbe60, 0x39405a88), (0xc348, 0x39405a69),
                     (0xbf24, 0x39405a88), (0xc35c, 0x390073a9)):
    assert struct.unpack_from('<I', image, offset)[0] == word
assert image[0xc374:0xc410] == original[0xc374:0xc410]
assert image[0xbf3c:0xbf40] == original[0xbf3c:0xbf40]
assert image[0x4d24:0x4ee8] == original[0x4d24:0x4ee8]
assert struct.unpack_from('<6Q', original, 0x1a390) == (
    0x20001, 0x1a360, 0x162c, 0x1634, 0x181c, 0x19b8)

main = (ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenStorageDxe/EldenStorageDxe.c').read_text()
connect = (ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenConnect.c').read_text()
probe = main.split('EldenInitializeSd (VOID)', 1)[1].split('// Compare inherited', 1)[0]
assert 'WriteBlocks(' not in probe and 'gBS->ConnectController(' not in probe
assert 'ReadBlocks' not in probe
assert 'Io->Media->MediaPresent' in connect and 'EldenConnectSdFilesystems(Handles[I])' in connect
assert '!Io->Media->RemovableMedia' in connect and 'Io->Media->LogicalPartition' in connect
assert 'EldenPrepareUi();' in connect and 'EldenInitializeSd();' in main
assert 'EldenStartSd();' in probe
assert probe.index('EldenStartSd();') < probe.index('gBS->CreateEventEx(')
assert 'EldenConnectDevices();' in (ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenBootManager.c').read_text()

fsprobe = connect.split('EldenConnectSdFilesystems (EFI_HANDLE Parent)', 1)[1].split(
    'EldenPrepareUi (VOID)', 1)[0]
assert 'CompareMem(Path, Candidate, Prefix)' in fsprobe
assert 'IsDevicePathValid(Candidate, 4096)' in fsprobe
assert 'gBS->ConnectController(Parent, NULL, NULL, FALSE)' in fsprobe
assert 'gBS->ConnectController(List[I], NULL, NULL, FALSE)' in fsprobe
assert 'gBS->FreePool(List)' in fsprobe
for forbidden in ('ReadBlocks', 'WriteBlocks', 'FlushBlocks', '->Open(', 'gBS->LoadImage(', 'gBS->StartImage('):
    assert forbidden not in fsprobe

ui = connect.split('EldenPrepareUi (VOID)', 1)[1].split('STATIC', 1)[0]
assert 'PcdGetPtr(PcdMsGopOverrideProtocolGuid)' in ui
assert 'ConnectController(Displays[I], NULL, NULL, FALSE)' in ui
assert 'UninstallProtocolInterface' not in connect
assert 'InstallProtocolInterface' not in connect
fdf = (ROOT / 'Platforms/AndromedaPkg/Frontpage.fdf.inc').read_text()
assert 'INF MsGraphicsPkg/GopOverrideDxe/GopOverrideDxe.inf' in fdf
assert '"$(TARGET_DEVICE)" != "lenovo-elden"' not in fdf

bm = (ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenBootManager.c').read_text()
choice = (ROOT / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenBootChoice.c').read_text()
assert 'return EldenChooseEfiBoot(Option)' in bm
assert 'EFI_FILE_MODE_READ' in choice and 'File->Read(File, &Size, Magic)' in choice
assert 'UsbDisk,\n  SdDisk,\n  UfsDisk' in choice
assert 'A->Lun < B->Lun' in choice and 'A->Partition < B->Partition' in choice
assert choice.count('SetTimer(Timer, TimerRelative, 3 * 10000000ULL)') == 2
assert 'return Confirmed ? Selected : 0' in choice
print('PASS: pinned SDCC fixes, read-only PIO, no tracing, EFI boot selection')
