#!/usr/bin/env python3
"""Validate staged Elden inputs; does not claim firmware bootability."""
import hashlib
import json
import re
from pathlib import Path


def check(device):
    layout = json.loads((device / "memory-layout.json").read_text())
    def region(name):
        r = layout[name]
        base, size = int(r["base"], 0), int(r["size"], 0)
        if size <= 0 or base % 4096 or size % 4096:
            raise ValueError(f"Invalid/alignment: {name}")
        return base, base + size
    original = region("original_dxe_heap")
    heap = region("dxe_container")
    free = region("dxe_allocatable")
    stack = region("uefi_stack")
    fd = region("uefi_fd")
    if not (original[0] == heap[0] == free[0] and
            free[1] == stack[0] and stack[1] == heap[1] == fd[0] and
            fd[1] == original[1]):
        raise ValueError("Heap/stack/FD partition mismatch")
    if fd[1] - fd[0] != 0x400000 or stack[1] - stack[0] != 0x40000:
        raise ValueError("Unexpected V1 FD/stack size")
    for name in ("kernel_window", "display_reserved", "cpu_vectors"):
        other = region(name)
        if max(original[0], other[0]) < min(original[1], other[1]):
            raise ValueError(f"Heap collides with {name}")
    if layout["kernel_compressed"] is not True:
        raise ValueError("BootShim requires the selected gzip path")
    source = (device / "Library/PlatformMemoryMapLib/PlatformMemoryMapLib.c").read_text()
    rows = re.findall(r'\{ "([^"]+)", (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+),', source)
    names = [name for name, _, _ in rows]
    if len(names) != len(set(names)):
        raise ValueError("Duplicate memory region name")
    regions = {name: (int(base, 0), int(base, 0) + int(size, 0)) for name, base, size in rows}
    for name, key in {"DXE Heap": "dxe_container", "DXE Free": "dxe_allocatable",
                      "UEFI Stack": "uefi_stack", "UEFI FD": "uefi_fd",
                      "CPU Vectors": "cpu_vectors", "Kernel": "kernel_window",
                      "Display Reserved": "display_reserved"}.items():
        if regions.get(name) != region(key):
            raise ValueError(f"C table and JSON disagree: {name}")
    pcd = (device / "MemoryPcds.dsc.inc").read_text()
    for name, value in {"PcdCpuVectorBaseAddress": region("cpu_vectors")[0],
                        "PcdPrePiStackBase": stack[0],
                        "PcdPrePiStackSize": stack[1] - stack[0]}.items():
        match = re.search(r'\.' + name + r'\|(0x[0-9A-Fa-f]+)', pcd)
        if not match or int(match[1], 0) != value:
            raise ValueError(f"PCD and JSON disagree: {name}")
    binaries = device / "Binaries"
    manifest = json.loads((device / "binary-sha256.json").read_text())
    actual = {p.relative_to(binaries).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in binaries.rglob("*") if p.is_file()}
    if actual != manifest:
        raise ValueError("Imported binary contents/file list changed")
    for name in ("DXE.inc", "DXE.dsc.inc", "APRIORI.inc"):
        lines = (binaries / name).read_text().splitlines()
        refs = [line.strip().removeprefix("INF ") for line in lines
                if line.strip().endswith(".inf")]
        if len(refs) != len(set(refs)):
            raise ValueError(f"Duplicate module reference: {name}")
        for ref in refs:
            if not (binaries / ref).is_file():
                raise ValueError(f"Missing INF: {ref}")
    print(f"PASS: V1 partition arithmetic, {len(actual)} input hashes and module lists")
    print("Not a DSC/FDF build or hardware boot validation.")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    check(root / "Platforms/PakalaPkg/Device/lenovo-elden")
