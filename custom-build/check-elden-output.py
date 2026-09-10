#!/usr/bin/env python3
"""Verify the actual compiled DEBUG payload and excluded power-control modules."""
import gzip
import hashlib
import re
import struct
import uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
build = root / "Build/PakalaPkg"
fv = build / "DEBUG_CLANGPDB/FV"
fd = (fv / "SM8750_EFI.fd").read_bytes()
image = (build / "lenovo-elden.img").read_bytes()
shim = (root / "BootShim/BootShim.bin").read_bytes()

def require(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)

require(len(fd) == 0x400000, "FD must be exactly 4 MiB")
require(image[:8] == b"ANDROID!", "Android magic")
kernel_size, ramdisk_size, os_version, header_size = struct.unpack_from("<4I", image, 8)
require(struct.unpack_from("<I", image, 40)[0] == 4, "boot header v4")
require(header_size == 1584 and os_version == 0, "stock-style v4 header fields")
require(ramdisk_size == 0, "this engineering container has no boot ramdisk")
payload = gzip.decompress(image[4096:4096 + kernel_size])
require(payload == shim + fd, "gzip payload must equal the current BootShim + FD")
require(struct.unpack_from("<QQ", shim, 8) == (0xD7C00000, 0x400000), "shim FD target/size")
require(shim[56:60] == b"ARM\x64", "shim ARM64 header")
require(len(shim) % 16 == 0, "shim payload alignment")

guids = set(re.findall(r"[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}",
                      (fv / "FVMAIN.Fv.txt").read_text().upper()))
require("E1DE8750-2632-48B3-8610-56D6EA878ABD" in guids, "diagnostic application absent from FV")
require("9A00771F-36D4-4DD5-8916-C48ED9B16B86" in guids, "HALIOMMU trial driver absent")
require("87503222-9D17-4503-AB62-DFC783604301" in guids, "Elden GOP absent")
require("87503222-9D17-4503-AB62-DFC783604304" in guids, "Elden storage DXE driver absent")
for console_guid in ("51CCF399-4FDF-4E55-A45B-E123F84D456A",
                     "408EDCEC-CF6D-477C-A5A8-B4844E3DE281",
                     "CCCB0C28-4B24-11D5-9A5A-0090273FC14D"):
    require(console_guid in guids, "standard console driver absent: " + console_guid)
require("27D4595D-D3AA-4008-9B02-881B0AC25F09" in guids,
        "common GopOverrideDxe absent")
require("4B28E4C7-FF36-4E10-93CF-A82159E777C5" in guids, "PSCI reset driver absent")
require("2A7B4BEF-80CD-49E1-B473-374BA4D673FC" in guids, "SPMI driver absent")
device = root / "Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg"
require("4DB5DEA6-5302-4D1A-8A82-677A683B0D29" in guids, "Clock-only trial driver absent")
ufs_id = "0D35CD8E-97EA-4F9A-96AF-0F0D89F76567"
# Platform-owned images must be non-dispatchable FREEFORM FFS files. Inspect
# the emitted FFS header, not stale .depex sidecars from a previous build.
import runpy

def platform_image(ident, expected):
    require(ident in guids, "platform image absent: " + ident)
    directory = fv / "Ffs" / (ident + "FVMAIN")
    files = list(directory.glob("*.ffs"))
    require(len(files) == 1, "platform FFS ambiguous: " + ident)
    data = files[0].read_bytes()
    require(len(data) >= 24 and data[:16] == uuid.UUID(ident).bytes_le,
            "platform FFS header invalid: " + ident)
    require(data[18] == 2, "platform image must be FREEFORM: " + ident)
    header = 32 if data[19] & 1 else 24
    size = struct.unpack_from("<Q", data, 24)[0] if header == 32 else int.from_bytes(data[20:23], "little")
    require(size == len(data), "platform FFS size mismatch: " + ident)
    offset, images = header, []
    while offset < size:
        require(offset + 4 <= size, "truncated section: " + ident)
        length = int.from_bytes(data[offset:offset+3], "little")
        kind, section_header = data[offset+3], 4
        if length == 0xffffff:
            require(offset + 8 <= size, "truncated extended section: " + ident)
            length, section_header = struct.unpack_from("<I", data, offset+4)[0], 8
        require(length >= section_header and offset + length <= size, "bad section size: " + ident)
        require(kind != 0x13, "platform image must not have DXE DEPEX: " + ident)
        if kind == 0x10:
            images.append(data[offset+section_header:offset+length])
        offset = (offset + length + 3) & ~3
    require(images == [expected], "platform PE differs from pinned payload: " + ident)

platform_image(ufs_id, runpy.run_path(str(root / "custom-build/prepare-elden-ufs.py"))["patched"]())
platform_image("F10F76DB-42C1-533F-34A8-69BE24653110",
               runpy.run_path(str(root / "custom-build/prepare-elden-sdcc.py"))["patched"]())
platform_image("9A00771F-36D4-4DD5-8916-C48ED9B16B86",
               (device / "Drivers/HALIOMMUDxe/HALIOMMUDxe.efi").read_bytes())
clock_id = "4DB5DEA6-5302-4D1A-8A82-677A683B0D29"
clock_ffs = fv / "Ffs" / (clock_id + "FVMAIN")
clock_deps = ["AE37B942-457F-4C91-A196-D9669FD347A3",
              "B0760469-970C-487A-A4B5-28DB7B45CEF1",
              "1C34F691-D33D-4E14-B8F8-32C6C029E95B",
              "0389B776-625F-11EB-83BE-C741A913DE34",
              "3C50E0F7-7D40-4A86-9067-11A2372BFC3D",
              "79D6C870-725E-489E-A0A1-27E7A5D0CB35"]
expected_depex = b"".join(b"\x02" + uuid.UUID(g).bytes_le for g in clock_deps) + b"\x03" * (len(clock_deps)-1) + b"\x08"
require((clock_ffs / (clock_id + "SEC1.depex")).read_bytes() == expected_depex,
        "Clock dependency gate differs from six-protocol AND")
require("CB29F4D1-7F37-4692-A416-93E82E219711" in guids, "NPA trial driver absent")
npa_id = "CB29F4D1-7F37-4692-A416-93E82E219711"
npa_ffs = fv / "Ffs" / (npa_id + "FVMAIN")
npa_depex = b"".join(b"\x02" + uuid.UUID(g).bytes_le for g in
                     ("AE37B942-457F-4C91-A196-D9669FD347A3",
                      "93A824B1-F77C-4880-AD95-E7BC634F9051")) + b"\x03\x08"
require((npa_ffs / (npa_id + "SEC1.depex")).read_bytes() == npa_depex,
        "NPA must wait for DALSys and log service")
npa_pe = (npa_ffs / (npa_id + "SEC2.pe32")).read_bytes()
require(npa_pe[3] == 0x10 and npa_pe[4:] == (device / "Drivers/NpaDxe/NpaDxe.efi").read_bytes(),
        "NPA payload differs from imported PE")
require("0401B830-BFF9-44A2-A5F6-95734A33C017" in guids, "PwrUtils trial driver absent")
usb_power_id = "11FAED4C-B21F-4D88-8E48-C4C28A1E50DF"
for module in ("I2CDxe", "TLMMDxe"):
    directory = device / "Drivers" / module
    inf = (directory / (module + ".inf")).read_text()
    ident = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]+)", inf)[1].upper()
    require(ident in guids, module + " absent")
    base_name = re.search(r"BASE_NAME\s*=\s*(\w+)", inf)[1]
    files = fv / "Ffs" / (ident + ("FVMAIN" if module == "I2CDxe" else base_name))
    deps, pes = list(files.glob("*.dpx")), list(files.glob("*.pe32"))
    require(len(deps) == len(pes) == 1, module + " sections ambiguous")
    require(deps[0].read_bytes()[4:] == (directory / (module + ".depex")).read_bytes(),
            module + " original dependencies changed")
    expected = (runpy.run_path(str(root / "custom-build/prepare-elden-i2c.py"))["patched"]()
                if module == "I2CDxe" else (directory / (module + ".efi")).read_bytes())
    require(pes[0].read_bytes()[4:] == expected,
            module + " original binary changed")
usb_id = "CD823A4D-7DEC-4531-AE5D-4134FA4127B8"
usb_pes = list((fv / "Ffs" / (usb_id + "FVMAIN")).glob("*.pe32"))
require(len(usb_pes) == 1, "USB probe PE ambiguous")
require(usb_pes[0].read_bytes()[4:] == runpy.run_path(str(root / "custom-build/prepare-elden-usb-probe.py"))["patched"](), "USB probe PE differs")
usb_deps = (fv / "Ffs" / (usb_id + "FVMAIN") / (usb_id + "SEC1.depex")).read_bytes()
def and_guids(expression):
    found, depth, pos = set(), 0, 0
    while pos < len(expression):
        op = expression[pos]
        pos += 1
        if op == 2:
            require(pos + 16 <= len(expression), "truncated USB dependency")
            found.add(expression[pos:pos+16])
            pos += 16
            depth += 1
        elif op == 3:
            require(depth >= 2, "invalid USB dependency stack")
            depth -= 1
        else:
            require(op == 8 and pos == len(expression) and depth == 1,
                    "USB dependencies must be AND-only")
    require(expression[-1] == 8, "unterminated USB dependencies")
    return found
storage_id = "87503222-9D17-4503-AB62-DFC783604304"
storage_ffs = fv / "Ffs" / (storage_id + "EldenStorageDxe")
require((storage_ffs / (storage_id + ".ffs")).read_bytes()[18] == 7,
        "storage initializer must be a dispatchable DXE driver")
storage_deps = list(storage_ffs.glob("*.dpx"))
require(len(storage_deps) == 1, "storage DEPEX missing or ambiguous")
storage_required = and_guids(storage_deps[0].read_bytes()[4:])
for ident in ("241afae6-885f-4f6c-a7ea-c28eab79c3e5",
              "0389b776-625f-11eb-83be-c741a913de34",
              "ae37b942-457f-4c91-a196-d9669fd347a3",
              "79d6c870-725e-489e-a0a1-27e7a5d0cb35",
              "9e61de7c-9927-4f28-9f3d-320705d81801"):
    require(uuid.UUID(ident).bytes_le in storage_required,
            "storage prerequisite missing: " + ident)
required_usb = and_guids((device / "Drivers/UsbConfigDxe/UsbConfigDxe.depex").read_bytes())
required_usb.update(uuid.UUID(g).bytes_le for g in (
    "54b6d3b4-5d33-4f91-8600-6c41d5deb19a",
    "241afae6-885f-4f6c-a7ea-c28eab79c3e5",
    "b27ae8b1-3e10-4d07-ab5c-eb9a6dc6fa8f"))
require(usb_deps == b"\x07\x08", "USB isolation requires FALSE/END DEPEX")
for module in ("UsbInitDxe", "XhciPciEmulationDxe", "XhciDxe", "UsbBusDxe", "UsbMassStorageDxe"):
    inf = (device / "Drivers" / module / (module + ".inf")).read_text()
    ident = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]+)", inf)[1].upper()
    require(ident not in guids, module + " must be absent during USB isolation")
usb_power_ffs = fv / "Ffs" / (usb_power_id + "FVMAIN")
usb_power_depex = (usb_power_ffs / (usb_power_id + "SEC1.depex")).read_bytes()
# Evaluate the emitted postfix expression for every prerequisite combination.
# This validates the FV output, not just the source FDF spelling/order.
power_guids = [uuid.UUID(g).bytes_le for g in (
    "ae6ae96e-483f-42ae-9cc1-9fac1b584728",
    "61740cc4-916f-4471-b0dc-00c6f43a7fc6",
    "4684800a-2755-4edc-b443-7f8ceb3239d3")]
for mask in range(8):
    stack, pos = [], 0
    while pos < len(usb_power_depex):
        op = usb_power_depex[pos]
        pos += 1
        if op == 2:
            g = usb_power_depex[pos:pos+16]
            require(g in power_guids, "unexpected USB power prerequisite")
            pos += 16
            stack.append(bool(mask & (1 << power_guids.index(g))))
        elif op == 3:
            require(len(stack) >= 2, "invalid USB power AND")
            right, left = stack.pop(), stack.pop()
            stack.append(left and right)
        elif op == 7:  # EFI_DEP_FALSE: explicit power-policy lock
            stack.append(False)
        else:
            require(op == 8 and pos == len(usb_power_depex), "invalid USB power DEPEX")
    require(stack == [False], "USB power entry must remain locked for every service combination")
for name in ("QcomScmiDxe", "MailboxDxe"):
    for inf in (device / "Drivers" / name).glob("*.inf"):
        match = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]+)", inf.read_text())
        require(match and match[1].upper() not in guids, "out-of-scope module included: " + name)
clock_pe_section = (clock_ffs / (clock_id + "SEC2.pe32")).read_bytes()
require(clock_pe_section[3] == 0x10 and
        clock_pe_section[4:] == (device / "Drivers/ClockDxe/ClockDxe.efi").read_bytes(),
        "Clock payload is not the unmodified imported PE")
for name in ("PmicDxe", "ButtonsDxe", "ResetRuntimeDxe", "QcomChargerDxe", "QcomWDogDxe"):
    for inf in (device / "Drivers" / name).glob("*.inf"):
        match = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]+)", inf.read_text())
        require(match and match[1].upper() not in guids, "power-control module included: " + str(inf))
for inf in (device / "Application/QcomChargerApp").glob("*.inf"):
    match = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]+)", inf.read_text())
    require(match and match[1].upper() not in guids, "charger application included")
libraries = (build / "DEBUG_CLANGPDB/AARCH64/PakalaPkg/PrePi/PeiUniCore/OUTPUT/static_library_files.lst").read_text()
require("EldenMemoryInitPeiLib.lib" in libraries, "SEC linked the wrong memory initializer")
require("/AndromedaPkg/MemoryInitPei/" not in libraries, "common memory initializer was linked")
app = (root / "Platforms/PakalaPkg/Application/EldenBringup/EldenBringup.c").read_text()
require("ReadKeyStroke" not in app and "SystemTable->ConIn" not in app, "diagnostic consumes physical keys")
for name, data in (("SM8750_EFI.fd", fd), ("lenovo-elden.img", image)):
    print(name, len(data), "bytes; SHA256", hashlib.sha256(data).hexdigest())
print("PASS: current FD/shim/boot container, diagnostic inclusion, power-module exclusions and SEC link")
print("Build/PakalaPkg/lenovo-elden.img is a raw engineering container, not the flash package.")
print("Use package-elden-flash.py and read the generated package's prerequisites before a device trial.")
