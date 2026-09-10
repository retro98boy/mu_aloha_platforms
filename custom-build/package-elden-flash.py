#!/usr/bin/env python3
"""Package the compiled Elden payload for an unlocked, stock-matched boot trial.

Never invokes fastboot or modifies source firmware. Only the new boot partition
image is changed; stock companion images and rollback inputs are copied verbatim.
"""
import argparse
import gzip
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STOCK_HASHES = {
    "boot": "b428a31dadc5db3f91620c1f4114f5ee7702e34e0789ed9bf84a878a49898f20",
    "vendor_boot": "326d5c328103ac6d1284773a11f09c8524582e9c264512734d5ba9079e4b9534",
    "init_boot": "e8206b81d5523d52c96f8f2eb11dfade3e80edcc4b76b796a7f74d6f45c25033",
    "dtbo": "45f517902d2271abe9c8bf6cab2cfa43a3126d89684fc7f956b823f03e0929a7",
    "vbmeta": "f4cafcba36b00aa29f05ed65d6aed20ec4f6d4121bb529e7312e2ad2ca529097",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def align(value):
    return (value + 4095) & ~4095


def avb(data, footer=True):
    """Read AVB bounds and the property/hash descriptors used by this package."""
    original = len(data)
    if footer:
        magic, major, minor, original, offset, size = struct.unpack(
            ">4sIIQQQ28x", data[-64:])
        require((magic, major, minor) == (b"AVBf", 1, 0), "bad AVB footer")
        require(0 < original <= offset and offset + size <= len(data) - 64,
                "AVB footer bounds")
        data = data[offset:offset + size]
    require(data[:4] == b"AVB0", "missing AVB header")
    auth, aux = struct.unpack_from(">QQ", data, 12)
    descriptor_offset, descriptor_size, rollback, flags, location = struct.unpack_from(
        ">QQQII", data, 96)
    require(256 + auth + aux <= len(data), "AVB block bounds")
    require(descriptor_offset + descriptor_size <= aux, "AVB descriptor bounds")
    pos = 256 + auth + descriptor_offset
    end = pos + descriptor_size
    hashes, props = {}, {}
    while pos < end:
        require(pos + 16 <= end, "truncated AVB descriptor")
        tag, count = struct.unpack_from(">QQ", data, pos)
        require(count % 8 == 0 and pos + 16 + count <= end, "AVB descriptor length")
        body = data[pos + 16:pos + 16 + count]
        if tag == 0:
            key_len, val_len = struct.unpack_from(">QQ", body)
            require(16 + key_len + 1 + val_len + 1 <= len(body), "AVB property bounds")
            key = body[16:16 + key_len].decode()
            props[key] = body[17 + key_len:17 + key_len + val_len].decode()
        elif tag == 2:
            image_size, algorithm, plen, slen, dlen, hflags = struct.unpack_from(
                ">Q32sIIII", body)
            require(116 + plen + slen + dlen <= len(body), "AVB hash bounds")
            name = body[116:116 + plen].decode()
            salt = body[116 + plen:116 + plen + slen]
            digest = body[116 + plen + slen:116 + plen + slen + dlen]
            require(name not in hashes, "duplicate AVB hash descriptor")
            hashes[name] = dict(size=image_size, algorithm=algorithm.rstrip(b"\0").decode(),
                                flags=hflags, salt=salt.hex(), digest=digest.hex())
        pos += 16 + count
    return dict(original=original, rollback=rollback, flags=flags, location=location,
                algorithm=struct.unpack_from(">I", data, 28)[0], hashes=hashes, props=props)


def check_hash(data, descriptor):
    require(descriptor["algorithm"] == "sha256" and descriptor["flags"] == 0,
            "unexpected hash algorithm/A-B flags")
    require(0 < descriptor["size"] <= len(data), "hash image bounds")
    actual = sha(bytes.fromhex(descriptor["salt"]) + data[:descriptor["size"]])
    require(actual == descriptor["digest"], "AVB image hash mismatch")


def boot_header(data):
    require(data[:8] == b"ANDROID!", "Android boot magic")
    kernel, ramdisk, os_version, header_size = struct.unpack_from("<4I", data, 8)
    require(struct.unpack_from("<I", data, 40)[0] == 4 and header_size == 1584,
            "expected boot header v4")
    require(struct.unpack_from("<I", data, 1580)[0] == 0, "unexpected v4 signature_size")
    require(4096 + align(kernel) + align(ramdisk) <= len(data), "boot payload bounds")
    return kernel, ramdisk, os_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-dir", type=Path, default=ROOT.parent / "TB322_ZUXOS_1.5.10.259_Tool/image")
    parser.add_argument("--avbtool", type=Path, default=ROOT.parents[1] / "sm8750-kernel-platform/kernel_platform/prebuilts/kernel-build-tools/linux-x86/bin/avbtool")
    parser.add_argument("--output", type=Path, default=ROOT.parent / "porting-artifacts/elden-aloha-v1-flash")
    args = parser.parse_args()
    output = args.output.resolve()
    require(not output.exists(), "output already exists; use a new --output directory")
    require(args.avbtool.is_file(), "avbtool missing; pass --avbtool PATH (host packaging)")
    subprocess.run([sys.executable, str(ROOT / "custom-build/check-elden-inputs.py")], check=True)
    subprocess.run([sys.executable, str(ROOT / "custom-build/check-elden-output.py")], check=True)

    stock = {}
    for name, expected in STOCK_HASHES.items():
        data = (args.stock_dir / (name + ".img")).read_bytes()
        require(sha(data) == expected, "unsupported/modified stock image: " + name)
        stock[name] = data
    sizes = {}
    for entry in ET.parse(args.stock_dir / "rawprogram4.xml").getroot().iter("program"):
        name = entry.get("label")
        if name in {n + "_" + s for n in STOCK_HASHES for s in ("a", "b")}:
            sizes[name] = int(entry.get("SECTOR_SIZE_IN_BYTES")) * int(entry.get("num_partition_sectors"))
    for name in STOCK_HASHES:
        require(sizes.get(name + "_a") == sizes.get(name + "_b") and
                sizes.get(name + "_a", 0) >= len(stock[name]),
                "stock size exceeds/disagrees with A/B partition table: " + name)
        if name != "vbmeta":
            require(sizes[name + "_a"] == len(stock[name]), "footer image must fill partition: " + name)

    old = avb(stock["boot"])
    check_hash(stock["boot"], old["hashes"]["boot"])
    top = avb(stock["vbmeta"], footer=False)
    for name in ("vendor_boot", "init_boot", "dtbo"):
        footer = avb(stock[name])
        check_hash(stock[name], footer["hashes"][name])
        check_hash(stock[name], top["hashes"][name])
    old_kernel, old_ramdisk, _ = boot_header(stock["boot"])
    require(old_ramdisk == 0, "stock boot contains a ramdisk")
    init_kernel, init_ramdisk, _ = boot_header(stock["init_boot"])
    require(init_kernel == 0 and init_ramdisk > 0, "init_boot ramdisk missing")
    vendor = stock["vendor_boot"]
    require(vendor[:8] == b"VNDRBOOT" and struct.unpack_from("<II", vendor, 8) == (4, 4096),
            "vendor_boot version/page size")
    vendor_ramdisk = struct.unpack_from("<I", vendor, 24)[0]
    hsize, dtb_size, dtb_addr, table_size, table_count, entry_size, config_size = struct.unpack_from(
        "<IIQIIII", vendor, 2096)
    require(hsize == 2128 and dtb_size > 0 and vendor_ramdisk > 0,
            "vendor ramdisk/DTB missing")
    require(table_size == table_count * entry_size and entry_size == 108,
            "vendor ramdisk table invalid")
    require(align(hsize) + align(vendor_ramdisk) + align(dtb_size) + align(table_size)
            + align(config_size) <= avb(vendor)["original"], "vendor payload bounds")
    require(struct.unpack_from(">I", stock["dtbo"])[0] == 0xD7B7AB1E, "DTBO table magic")

    compiled = (ROOT / "Build/PakalaPkg/lenovo-elden.img").read_bytes()
    kernel_size, ramdisk_size, _ = boot_header(compiled)
    require(ramdisk_size == 0, "compiled boot ramdisk unexpected")
    kernel = compiled[4096:4096 + kernel_size]
    payload = gzip.decompress(kernel)
    shim = (ROOT / "BootShim/BootShim.bin").read_bytes()
    fd = (ROOT / "Build/PakalaPkg/DEBUG_CLANGPDB/FV/SM8750_EFI.fd").read_bytes()
    require(payload == shim + fd, "compiled payload differs from current FD/shim")
    # Conditional address check using the documented stock Kernel arena and ABL formula.
    kernel_start = 0xA8080000
    ramdisk_start = 0xB8000000 - align(align(init_ramdisk) + vendor_ramdisk) - 4096
    dtb_start = ramdisk_start - 0x200000 - 4096
    require(kernel_start + len(payload) < dtb_start, "payload overlaps candidate DTB/ramdisk")
    require(kernel_start + len(payload) < 0xD7BC0000, "payload overlaps new stack/FD")

    # Preserve the complete stock header page, changing only kernel_size. Do NOT
    # transplant old embedded GKI signatures or the old partition AVB footer.
    header = bytearray(stock["boot"][:4096])
    struct.pack_into("<I", header, 8, kernel_size)
    raw = bytes(header) + kernel + bytes(align(kernel_size) - kernel_size)
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=output.name + ".partial-", dir=output.parent))
    # avbtool resolves the descriptor's partition name to sibling boot.img.
    image = staging / "boot.img"
    image.write_bytes(raw)
    command = [str(args.avbtool.resolve()), "add_hash_footer", "--image", str(image),
               "--partition_name", "boot", "--partition_size", str(sizes["boot_a"]),
               "--algorithm", "NONE", "--hash_algorithm", "sha256", "--flags", "0",
               "--salt", old["hashes"]["boot"]["salt"],
               "--rollback_index", str(old["rollback"]),
               "--rollback_index_location", str(old["location"])]
    for key, value in sorted(old["props"].items()):
        command += ["--prop", key + ":" + value]
    subprocess.run(command, check=True)
    new = image.read_bytes()
    metadata = avb(new)
    require(len(new) == sizes["boot_a"], "output must fill boot partition for footer addressing")
    require(metadata["algorithm"] == 0 and metadata["flags"] == 0, "unsigned chained AVB flags")
    require(metadata["rollback"] == old["rollback"] and metadata["props"] == old["props"],
            "AVB template metadata changed")
    require(metadata["original"] == len(raw) and new[:len(raw)] == raw, "AVB altered payload")
    require(new[:8] == stock["boot"][:8] and new[12:4096] == stock["boot"][12:4096],
            "stock header changed outside kernel_size")
    check_hash(new, metadata["hashes"]["boot"])
    report = subprocess.check_output([str(args.avbtool.resolve()), "info_image", "--image", str(image)], text=True)
    (staging / "boot-avb-info.txt").write_text(report)
    # avbtool verifies unsigned metadata and the self hash, NOT Lenovo trust.
    verification = subprocess.run([str(args.avbtool.resolve()), "verify_image", "--image", str(image)],
                                  text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    (staging / "boot-avb-verify.txt").write_text(verification.stdout)
    (staging / "stock").mkdir()
    for name, data in stock.items():
        destination = staging / "stock" / (name + ".img")
        destination.write_bytes(data)
        require(sha(destination.read_bytes()) == STOCK_HASHES[name], "stock copy mismatch")
    shutil.copyfile(ROOT / "custom-build/elden-flash-README.md", staging / "README.md")
    manifest = dict(
        status="UNLOCKED_DEVICE_FIRST_BOOT_TRIAL_NOT_HARDWARE_VALIDATED",
        stock_version="TB322_ZUXOS_1.5.10.259", stock_sha256=STOCK_HASHES,
        stock_source=str(args.stock_dir.resolve()), partition_sizes=sizes,
        boot_sha256=sha(new), fd_sha256=sha(fd), shim_sha256=sha(shim),
        kernel_compressed_size=kernel_size, kernel_decompressed_size=len(payload),
        boot_header_version=4, boot_signature_size=0, avb=metadata,
        dropped_stock_post_kernel_bytes=old["original"] - (4096 + align(old_kernel)),
        candidate_addresses_not_runtime_measurements=dict(kernel=hex(kernel_start),
            dtb=hex(dtb_start), ramdisk=hex(ramdisk_start), fd="0xd7c00000"),
        screen_output_validated=False, power_key_handling_changed=False,
        device_partitions_written=[], top_level_vbmeta_modified=False,
        avbtool_sha256=sha(args.avbtool.read_bytes()),
    )
    (staging / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    files = sorted(p for p in staging.rglob("*") if p.is_file())
    (staging / "SHA256SUMS").write_text("".join(
        sha(p.read_bytes()) + "  " + p.relative_to(staging).as_posix() + "\n" for p in files))
    staging.rename(output)
    print("PASS: flash-trial package:", output)
    print("boot.img SHA256:", sha(new))
    print("No device accessed. Read README.md prerequisites BEFORE flashing.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit("FAIL: " + str(error)) from error
