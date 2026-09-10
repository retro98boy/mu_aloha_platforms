# Elden microSD v1: diagnostic, not yet hardware validated

## v2 power-failure diagnostics

Photo 9510: driver returns Success, enumeration is signalled, but voltage
setup fails for drive 1; total BlockIO remains 118 and removable=0.
Driver/event Success must not be interpreted as successful card discovery.

Pinned call at 0x97fc invokes voltage setup 0x585c. Failure 0x9804 now
preserves raw w0 in w3 and uses ADR at 0x9808 for the existing error string
(replaced in-place). It reports drive and raw return without changing control
flow or return status. At 0x59dc..0x59e8 a missing first-rail NPA client returns
4; configuration failure returns 6 at 0x58e4. No new power writes are added.
Later rail client groups have the same missing-handle guards.

After enumeration, read only the pinned PE's own global client pointers and
DT flag, with loaded-image size guard. VDD slots 0x1b9d8, VCCA 0x1b9f0,
VCCB 0x1b9b0 each contain mV/en/mode handles. These are not voltage readings.
No new NPA requests, SPMI writes or additional enumeration retries are made.
Test with card inserted; photograph SD POWER FAIL/raw and POWER snapshots.

Base checkpoint: parent 88a5648f, MU_BASECORE f12291cd9e.
Development branch: elden-microsd-bringup in both repositories.

Stock Android DT: sdhci@8804000, SDCC2, four-bit SD-only slot,
card detect GPIO55. Stock XBL configuration names this controller sdc1,
gcc_sdcc2_apps_clk/gcc_sdcc2_ahb_clk, TLMM pad 0x0f1db000.
Stock rails: ldob9 card VDD 2960mV, ldoi2 level-shifter VCCA 1200mV,
ldob8 VCCB 2960mV (1800mV low-voltage mode). Do not substitute guessed rails.
The imported driver consumes original firmware configuration; no new PMIC,
USB, VBUS, pinmux or voltage-table patches are introduced in this version.

SdccDxe SHA256 is pinned in prepare-elden-sdcc.py. Exact ARM64 disassembly:
- Entry 0x37fc performs IOMMU setup via 0x5570 before media initialization.
- Branch at 0x3874 skips the embedded drive-0/eMMC/RPMB initialization and
  jumps to stock removable-drive-1 setup at 0x3c88. x20 (boot services global
  page) is already initialized at 0x3840 and preserved by called functions.
- 0x15324 contains drive number 1, versus drive 0 at 0x15328.
- SD BlockIO template at 0x1a390: revision, media, Reset 0x162c,
  ReadBlocks 0x1634, WriteBlocks 0x181c, FlushBlocks 0x19b8.
  WriteBlocks now returns EFI_WRITE_PROTECTED without executing its body.
- 0x3cfc..0x3d28 registers removable enumeration callback 0x33a8 for
  event group b7972c36-8a4c-4a56-8b02-1159b52d4bfb.

SD driver is dispatched only after the explicit late gate, with stock Clock
and Timer architectural dependencies plus Env, HALIOMMU and NPA. Record actual driver
return separately from dispatcher return. Signal the SD enumeration group
once, inventory removable parent BlockIO, read only LBA0 if media present.
No SD filesystem mounting, partition repair, block writes, EFI or kernel
execution is requested. Existing UFS probe remains unchanged.

No hardware-status success is forged. Stock driver internals still perform
hardware initialization; DMA/IOMMU compatibility, power votes, detection and
card reads require device testing. This is not a guarantee against hardware
hangs or all stock-driver side effects. Use a spare backed-up card.

BDS is held before entry: neither boot_b GRUB nor FrontPage starts. Screen
ends with SD LOG HOLD unless the driver hangs earlier. Photograph from
ELDEN SD v1 BEGIN through the last line. No hot-plug testing yet.

Flash boot.img to boot_a, keep slot A active, leave boot_b untouched.
Test with a card inserted before power-on; compare with no card if needed.
If stuck, use the existing hardware long-power reset. No automatic reboot.
