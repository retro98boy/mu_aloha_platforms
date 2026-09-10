#!/usr/bin/env python3
"""AArch64 syntax check + host HOB contract test; no firmware build or flashing."""
from pathlib import Path
import subprocess
import tempfile
import re

root = Path(__file__).resolve().parents[1]
device = root / "Platforms/PakalaPkg/Device/lenovo-elden"
# Reproduce DXE's unsigned resource-top minus PHIT-top calculation using
# the actual platform table, and pin the actual HobConstructor argument.
table = (device / "Library/PlatformMemoryMapLib/PlatformMemoryMapLib.c").read_text()
regions = {name: (int(base, 16), int(size, 16)) for name, base, size in
           re.findall(r'\{ "([^"]+)", (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+),', table)}
base, size = regions["DXE Free"]
assert regions["UFS HWIO Parent"] == (0x01d80000, 0x40000)
assert regions["GCC"] == (0x00100000, 0x001f5000)
assert regions["RAM High Bank 0"] == (0x880000000, 0x02F700000)
assert regions["RAM High Bank 1"] == (0x8B0000000, 0x005500000)
assert regions["RAM High Bank 2"] == (0x8C0000000, 0x140000000)
assert regions["RAM High Bank 3"] == (0xA00000000, 0x1ECD40000)
assert regions["DRAM Top Reserved"] == (0xBECD40000, 0x0132C0000)
assert regions["RAM High Bank 3"][0] + regions["RAM High Bank 3"][1] == regions["DRAM Top Reserved"][0]
assert regions["DRAM Top Reserved"][0] + regions["DRAM Top Reserved"][1] == 0xC00000000
for name in ("RAM High Bank 0", "RAM High Bank 1", "RAM High Bank 2", "RAM High Bank 3"):
    assert re.search(r'"' + re.escape(name) + r'".*AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN', table)
assert re.search(r'"DRAM Top Reserved".*AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN', table)
assert re.search(r'"GCC".*AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE', table)
assert "UFS HCI Probe" not in regions
assert regions["UFS HWIO Parent"][0] + regions["UFS HWIO Parent"][1] == regions["CRYPTO0_CRYPTO"][0]
assert re.search(r'"UFS HWIO Parent".*AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE', table)
heap_base, heap_size = regions["DXE Heap"]
stack_base, stack_size = regions["UEFI Stack"]
assert base == heap_base and base + size == stack_base
assert heap_base + heap_size == stack_base + stack_size
old_length = ((base + size) - (heap_base + heap_size)) % (1 << 64)
assert old_length == 0xfffffffffffc0000  # exact device failure
phit_top = base + size
assert (base + size) - phit_top == 0  # DXE must fall back to PHIT free range
sample_free_bottom, sample_free_top = 0xbd931680, 0xd6b62000  # photo
aligned_bottom = (sample_free_bottom + 4095) & ~4095
assert base <= aligned_bottom < sample_free_top <= phit_top
assert sample_free_top - aligned_bottom >= 0x10000
prepi = (root / "Platforms/PakalaPkg/PrePi/PrePi.c").read_text()
assert re.search(r'HobConstructor\s*\(\s*\(VOID \*\)UefiMemoryBase,\s*DxeFree.Length,', prepi)
assert "MemoryPeim (UefiMemoryBase, UefiMemorySize)" in prepi
print("PASS: PHIT/resource contract, old underflow reproduced, corrected DXE fallback arithmetic")
sources = [device / "Library" / name / (name + ".c") for name in
           ("PlatformMemoryMapLib", "PlatformConfigurationMapLib", "MemoryInitPeiLib")]
includes = [root / p for p in ("MU_BASECORE/MdePkg/Include", "MU_BASECORE/ArmPkg/Include",
                               "MU_BASECORE/UefiCpuPkg/Include", "Platforms/AndromedaPkg/Include")]
common = ["clang", "-fshort-wchar", "-Werror"]
for inc in includes:
    common += ["-I", str(inc)]
subprocess.run(common + ["--target=aarch64-linux-gnu", "-ffreestanding", "-fsyntax-only",
                        "-I", str(root / "MU_BASECORE/MdePkg/Include/AArch64")]
               + [str(p) for p in sources], check=True)
with tempfile.TemporaryDirectory(prefix="elden-memory-test-") as tmp:
    binary = str(Path(tmp) / "test")
    subprocess.run(common + ["-DMDEPKG_NDEBUG", "-I", str(root / "MU_BASECORE/MdePkg/Include/X64")]
                   + [str(p) for p in sources]
                   + [str(root / "custom-build/tests/elden-memory-test.c"), "-o", binary], check=True)
    subprocess.run([binary], check=True)
print("PASS: AArch64 syntax and host tests; real MMU/HOB consumer/boot still untested")
