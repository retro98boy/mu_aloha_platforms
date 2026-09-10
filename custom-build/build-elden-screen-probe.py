#!/usr/bin/env python3
"""Host-only isolated screen probe; never accesses a device or overwrites output."""
import argparse
import ctypes
import gzip
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
spec = importlib.util.spec_from_file_location("package", HERE / "package-elden-flash.py")
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, default=ROOT / "porting-artifacts/elden-screen-probe-v1")
    args = ap.parse_args()
    output = args.output.resolve()
    pkg.require(not output.exists(), "output exists; choose a fresh directory")
    stock = (ROOT / "boot_a.img").read_bytes()
    pkg.require(pkg.sha(stock) == "ec05dcf7ad951c3672cbd8bb20379aec45632faa13f7fe439330e950de0b7f4d", "unexpected original boot backup")
    old = pkg.avb(stock)
    pkg.check_hash(stock, old["hashes"]["boot"])
    _, ramdisk, _ = pkg.boot_header(stock)
    pkg.require(ramdisk == 0 and len(stock) == 100663296, "unexpected boot layout")
    dtb = (ROOT / "ZUXOS_1.1.11.263.stock.dtb").read_bytes()
    pkg.require(pkg.sha(dtb) == "7e899538ca0c73c66d313a9079c1e68ac6efe6586f168a74cf2447ba01666390", "unexpected reference FDT")
    avbtool = ROOT.parent / "sm8750-kernel-platform/kernel_platform/prebuilts/kernel-build-tools/linux-x86/bin/avbtool"
    pkg.require(avbtool.is_file(), "host avbtool missing")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=output.name + ".partial-", dir=output.parent))
    build = stage / "build"
    build.mkdir()
    source = HERE / "elden-screen-probe"

    # Execute the same parser and rectangle writer on host-owned memory first.
    run("clang", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror", source / "probe.c", "-o", build / "test.so")
    lib = ctypes.CDLL(str(build / "test.so"))
    lib.probe_fdt_matches.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.probe_fdt_matches.restype = ctypes.c_int
    def matches(data, available=None):
        buf = ctypes.create_string_buffer(data)
        return lib.probe_fdt_matches(buf, len(data) if available is None else available)
    pkg.require(matches(dtb) == 1, "reference live FDT rejected")
    pkg.require(matches(dtb, len(dtb) - 1) == 0, "truncation accepted")
    for before, after in [(bytes.fromhex("fc800000"), bytes.fromhex("fc900000")),
                          (bytes.fromhex("02b00000"), bytes.fromhex("02c00000")),
                          (b"msm_drm.dsi_display0=", b"msm_drm.dsi_display9=")]:
        changed = dtb.replace(before, after)
        pkg.require(changed != dtb and matches(changed) == 0, "incorrect display configuration accepted")
    for length in (0, 4, 39, 40, 1024):
        pkg.require(matches(dtb[:length]) == 0, "short FDT accepted")
    frame = (ctypes.c_uint32 * (1904 * 3040))()
    lib.probe_draw.argtypes = [ctypes.c_void_p]
    lib.probe_draw(frame)
    for y in range(3040):
        for x in range(1904):
            expected = (0xffffffff if (x // 16 + y // 16) & 1 else 0xff000000) if 64 <= x < 320 and 64 <= y < 128 else 0
            pkg.require(frame[y * 1904 + x] == expected, "write outside checker or wrong color")
    print("PASS: real FDT, negative FDT cases, exact framebuffer write bounds", flush=True)

    flags = ["--target=aarch64-none-elf", "-ffreestanding", "-fno-builtin", "-mgeneral-regs-only", "-fno-stack-protector", "-fpie", "-Os", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Wall", "-Wextra", "-Werror"]
    run("clang", *flags, "-c", source / "probe.c", "-o", build / "probe.o")
    run("clang", "--target=aarch64-none-elf", "-c", source / "entry.S", "-o", build / "entry.o")
    run("ld.lld", "--no-undefined", "-T", source / "probe.ld", build / "entry.o", build / "probe.o", "-o", build / "probe.elf")
    run("llvm-objcopy", "-O", "binary", build / "probe.elf", build / "Image")
    payload = (build / "Image").read_bytes()
    pkg.require(payload[56:60] == b"ARM\x64" and struct.unpack_from("<QQ", payload, 8) == (0x80000, len(payload)), "ARM64 header mismatch")
    pkg.require(len(payload) < 0x10000, "unexpectedly large probe")
    (build / "disassembly.txt").write_bytes(subprocess.check_output(["llvm-objdump", "-d", str(build / "probe.elf")]))
    kernel = gzip.compress(payload, mtime=0)
    header = bytearray(stock[:4096])
    struct.pack_into("<I", header, 8, len(kernel))
    raw = bytes(header) + kernel + bytes(pkg.align(len(kernel)) - len(kernel))
    image = stage / "boot.img"
    image.write_bytes(raw)
    command = [avbtool, "add_hash_footer", "--image", image, "--partition_name", "boot", "--partition_size", len(stock), "--algorithm", "NONE", "--hash_algorithm", "sha256", "--flags", "0", "--salt", old["hashes"]["boot"]["salt"], "--rollback_index", old["rollback"], "--rollback_index_location", old["location"]]
    for key, value in sorted(old["props"].items()):
        command += ["--prop", key + ":" + value]
    run(*command)
    new = image.read_bytes()
    meta = pkg.avb(new)
    pkg.check_hash(new, meta["hashes"]["boot"])
    pkg.require(len(new) == len(stock) and new[:len(raw)] == raw, "packaging changed payload or size")
    pkg.require(new[12:4096] == stock[12:4096], "stock header changed")
    pkg.require(meta["algorithm"] == 0 and meta["flags"] == 0 and meta["props"] == old["props"] and meta["rollback"] == old["rollback"], "AVB metadata mismatch")
    run(avbtool, "verify_image", "--image", image)
    (stage / "stock").mkdir()
    (stage / "stock/boot.img").write_bytes(stock)
    pkg.require(pkg.sha((stage / "stock/boot.img").read_bytes()) == pkg.sha(stock), "restore copy mismatch")
    run(avbtool, "verify_image", "--image", stage / "stock/boot.img")
    shutil.copyfile(source / "README.md", stage / "README.md")
    manifest = dict(status="EXPERIMENT_NOT_HARDWARE_VALIDATED", stock_version="1.1.11.263", stock_sha256=pkg.sha(stock), dtb_sha256=pkg.sha(dtb), boot_sha256=pkg.sha(new), payload_size=len(payload), avb=meta, full_aloha=False, format_hypothesis="BGRA32 width=1904 height=3040 stride=7616 base=0xfc800000", writes="256x64 pixels at (64,64), then WFE forever", power_key_changed=False, device_accessed=False, sources={p.name: pkg.sha(p.read_bytes()) for p in source.iterdir() if p.is_file()})
    (stage / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    (stage / "SHA256SUMS").write_text("".join(pkg.sha(p.read_bytes()) + "  " + str(p.relative_to(stage)) + "\n" for p in sorted(stage.rglob("*")) if p.is_file()))
    stage.rename(output)
    print("PASS:", output)
    print("boot SHA256:", pkg.sha(new))


if __name__ == "__main__":
    main()
