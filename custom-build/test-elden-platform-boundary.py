#!/usr/bin/env python3
"""Check Elden policy lives in the platform, not a private DXE Core fork."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
device = root / 'Platforms/PakalaPkg/Device/lenovo-elden'
base = root / 'MU_BASECORE'
# The upstream revision used immediately before the Elden bring-up commits.
upstream = '762a65dad79e2934cddbefa467b7e2c7b5ee4bc0'
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=base, text=True).strip() == upstream
subprocess.run(['git', 'diff', '--exit-code', upstream, '--'], cwd=base, check=True)
assert not subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=base)

init = (device / 'EldenStorageDxe/EldenStorageDxe.c').read_text()
assert not re.search(r'\bCore[A-Z]\w*\s*\(', init), 'private core service call'
assert 'gDxeCore' not in init and '#include "DxeMain.h"' not in init
assert 'ELDEN_DXE_TRACE' not in (device / 'PcdsFixedAtBuild.dsc.inc').read_text()
assert '-DELDEN_FRAMEBUFFER_CONSOLE=1' in (device / 'PcdsFixedAtBuild.dsc.inc').read_text()
for path in ('Platforms/PakalaPkg/PrePi/PrePi.c',
             'Platforms/AndromedaPkg/Library/FrameBufferSerialPortLib/FrameBufferSerialPortLib.c'):
    text = (root / path).read_text()
    assert 'ELDEN_DXE_TRACE' not in text
    assert 'ELDEN_FRAMEBUFFER_CONSOLE' in text
assert 'EfiGetCurrentTpl() != TPL_APPLICATION' in init
boot = (device / 'EldenBootManager/EldenBootManager.c').read_text()
assert 'EldenConnectDevices' in boot
assert 'EldenStorageEntry' not in boot
assert 'gDS->Dispatch()' not in init
assert 'ConnectController(' not in init
assert 'EldenStorageEntry' in init
connect = (device / 'EldenBootManager/EldenConnect.c').read_text()
assert 'gBS->LoadImage(' not in connect and 'gBS->StartImage(' not in connect
assert 'LocateProtocol(&gEldenUfsReadyProtocolGuid' in connect
assert 'if (mEldenUfsReturned && !EFI_ERROR(mEldenUfsStartStatus))' in init
assert 'InstallProtocolInterface(&Ready, &gEldenUfsReadyProtocolGuid' in init
inf = (device / 'EldenStorageDxe/EldenStorageDxe.inf').read_text()
assert '[Depex]' in inf and 'gEfiClockProtocolGuid AND' in inf
assert 'gEldenHwioProtocolGuid' in inf
assert 'EldenStorageDxe/EldenStorageDxe.inf' in (device / 'DXE.inc').read_text()
assert 'EldenStorageDxe/EldenStorageDxe.inf' in (device / 'DXE.dsc.inc').read_text()
fdf = (device / 'DXE.inc').read_text()
for guid in ('F10F76DB-42C1-533F-34A8-69BE24653110',
             '0D35CD8E-97EA-4F9A-96AF-0F0D89F76567',
             '9A00771F-36D4-4DD5-8916-C48ED9B16B86'):
    assert fdf.count(guid) == 1
    assert f'FILE FREEFORM = {guid}' in fdf, 'platform-owned image must not auto-dispatch'
assert 'FALSE' in fdf.split('FILE DRIVER = CD823A4D-', 1)[1].split('SECTION PE32', 1)[0]

dsc = (root / 'Platforms/PakalaPkg/PakalaCommon.dsc.inc').read_text()
fv = (root / 'Platforms/PakalaPkg/Pakala.fdf').read_text()
assert 'MdeModulePkg/Core/Dxe/DxeMain.inf' in dsc and 'MdeModulePkg/Core/Dxe/DxeMain.inf' in fv
for component in ('DxeRngLib', 'FrameBufferBltLib', 'GraphicsConsoleDxe', 'PartitionDxe'):
    assert re.search(r'PakalaPkg/Device/lenovo-elden/Override/.*' + component + r'\.inf', dsc)
print('PASS: pristine upstream BaseCore, public-service platform init and Elden-only overrides')
