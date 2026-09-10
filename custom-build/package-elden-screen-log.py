#!/usr/bin/env python3
"""Package current Aloha screen-log build with the verified 1.1.11.263 boot backup."""
import argparse
import gzip
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
spec = importlib.util.spec_from_file_location("package", HERE / "package-elden-flash.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, default=ROOT.parent / "porting-artifacts/elden-aloha-screen-log-v1")
    ap.add_argument("--stock", type=Path, default=ROOT.parent / "boot_a.img")
    args = ap.parse_args()
    output = args.output.resolve()
    p.require(not output.exists(), "output exists; choose a new directory")
    subprocess.run([sys.executable, str(HERE / "check-elden-output.py")], check=True)
    stock = args.stock.read_bytes()
    p.require(p.sha(stock) == "ec05dcf7ad951c3672cbd8bb20379aec45632faa13f7fe439330e950de0b7f4d", "wrong stock backup")
    old = p.avb(stock)
    p.check_hash(stock, old["hashes"]["boot"])
    p.require(p.boot_header(stock)[1] == 0, "unexpected stock ramdisk")
    compiled = (ROOT / "Build/PakalaPkg/lenovo-elden.img").read_bytes()
    size, ramdisk, _ = p.boot_header(compiled)
    p.require(ramdisk == 0, "unexpected payload ramdisk")
    kernel = compiled[4096:4096 + size]
    fd = (ROOT / "Build/PakalaPkg/DEBUG_CLANGPDB/FV/SM8750_EFI.fd").read_bytes()
    shim = (ROOT / "BootShim/BootShim.bin").read_bytes()
    p.require(gzip.decompress(kernel) == shim + fd, "payload mismatch")
    header = bytearray(stock[:4096])
    struct.pack_into("<I", header, 8, size)
    raw = bytes(header) + kernel + bytes(p.align(size) - size)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=output.name + ".partial-", dir=output.parent))
    image = stage / "boot.img"
    image.write_bytes(raw)
    avbtool = ROOT.parents[1] / "sm8750-kernel-platform/kernel_platform/prebuilts/kernel-build-tools/linux-x86/bin/avbtool"
    cmd = [str(avbtool), "add_hash_footer", "--image", str(image), "--partition_name", "boot", "--partition_size", str(len(stock)), "--algorithm", "NONE", "--hash_algorithm", "sha256", "--flags", "0", "--salt", old["hashes"]["boot"]["salt"], "--rollback_index", str(old["rollback"]), "--rollback_index_location", str(old["location"])]
    for key, value in sorted(old["props"].items()):
        cmd += ["--prop", key + ":" + value]
    subprocess.run(cmd, check=True)
    new = image.read_bytes()
    meta = p.avb(new)
    p.check_hash(new, meta["hashes"]["boot"])
    p.require(len(new) == len(stock) and new[:len(raw)] == raw and new[12:4096] == stock[12:4096], "packaging mismatch")
    p.require(meta["algorithm"] == 0 and meta["flags"] == 0 and meta["props"] == old["props"] and meta["rollback"] == old["rollback"], "AVB metadata mismatch")
    (stage / "stock").mkdir()
    (stage / "stock/boot.img").write_bytes(stock)
    for path in (image, stage / "stock/boot.img"):
        subprocess.run([str(avbtool), "verify_image", "--image", str(path)], check=True)
    shutil.copyfile(HERE / "elden-screen-log-README.md", stage / "README.md")
    (stage / "new-source/custom-build").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(HERE / "prepare-elden-ufs.py", stage / "new-source/custom-build/prepare-elden-ufs.py")
    for evidence in ("test-elden-ufs-ct1.py", "test-elden-ufs-inherit.py", "test-elden-gpt.py", "elden-ufs-deep-review.md", "elden-ufs-topology.md",
                     "test-elden-usb-depex.py", "test-elden-log-filter.py", "prepare-elden-i2c.py",
                     "elden-i2c-fifo-recover.S", "test-elden-i2c-recovery.py",
                     "elden-usb-config-probe.S", "prepare-elden-usb-probe.py", "test-elden-usb-probe.py", "elden-usb-reference-audit.md",
                     "inspect-elden-qupfw.py", "elden-qup-fifo-evidence.md",
                     "test-elden-pmic-boundary.py", "prepare-elden-pmic-candidate.py",
                     "elden-usb-topology.md", "elden-pmic-boundary.md",
                     "prepare-elden-sdcc.py", "test-elden-sdcc.py", "test-elden-sd-vote.py", "elden-microsd-v1.md", "elden-microsd-power.md"):
        shutil.copyfile(HERE / evidence, stage / "new-source/custom-build" / evidence)
    # Keep exact source diff alongside the tested image; checkpoint branch is untouched.
    # New device driver may still be untracked and therefore absent from git diff.
    shutil.copytree(ROOT / "Platforms/PakalaPkg/Device/lenovo-elden/EldenGop",
                    stage / "new-source/Platforms/PakalaPkg/Device/lenovo-elden/EldenGop")
    shutil.copyfile(HERE / "test-elden-keys.py", stage / "new-source/custom-build/test-elden-keys.py")
    for driver in ("EldenKeys", "EldenBootManager", "EldenFrontPage", "EldenStorageDxe", "Override"):
        shutil.copytree(ROOT / "Platforms/PakalaPkg/Device/lenovo-elden" / driver,
                        stage / "new-source/Platforms/PakalaPkg/Device/lenovo-elden" / driver)
    (stage / "source.patch").write_bytes(subprocess.check_output(["git", "diff", "HEAD", "--"], cwd=ROOT))
    # Elden changes are platform-owned; record the upstream dependency revision
    # rather than exporting a patch against MU_BASECORE.
    (stage / "mu-basecore-head.txt").write_bytes(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT / "MU_BASECORE"))
    manifest = dict(stock_version="1.1.11.263", stock_sha256=p.sha(stock), boot_sha256=p.sha(new), fd_sha256=p.sha(fd), shim_sha256=p.sha(shim), avb=meta, hardware_validated=False, power_key_changed=False, source_head=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip())
    (stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (stage / "SHA256SUMS").write_text("".join(p.sha(f.read_bytes()) + "  " + str(f.relative_to(stage)) + "\n" for f in sorted(stage.rglob("*")) if f.is_file()))
    stage.rename(output)
    print("PASS:", output, "boot SHA256:", p.sha(new))


if __name__ == "__main__":
    main()
