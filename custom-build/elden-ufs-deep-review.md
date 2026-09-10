# Elden UFS deep review — 2026-09-13

## 9442 / shared-memory provider integration

Hardware reaches pre-BDS stop with eight BlockIO handles, but driver status remains
EFI_NOT_FOUND from RPMB allocation. Handle enumeration is not a media-read test
and does not prove failed-driver protocol lifetime is safe for use.

Original ShmBridgeDxeLA installs GUID 9c1eb71f-dd6c-4ed5-9f6a-5cc0ca789f16:
GUID at RVA 0x7068, interface at 0x70a8, InstallMultipleProtocolInterfaces at
0x1528. Interface revision is 0x10002, allocation function 0x1558 and free 0x1568.
UFSDxe uses interface+8 with the allocation parameter pointer in x1, matching
0x1558 which loads its size from [x1], selects 0x1000 alignment and branches to
0x2a20. Entry initialization at 0x14e4 returns success, then installs interface.
The module was absent from Elden DXE.inc; the new FILE DRIVER includes the
unmodified imported PE and explicit TRUE DEPEX (the imported INF lacks DEPEX).
It is dispatched with normal drivers before the separately gated UFS trial.

No TzDxe transplant, SCM bypass, key programming or RPMB write test is added.
The previous EFI_NOT_FOUND return guard remains if installation fails. Further
listener/security dependencies may become visible; complete RPMB operation is
not yet verified. LINK-PROFILE keeps reads unchanged while reducing successful
output to power-mode and topology summaries; error dumps remain.

## 9440: real submission progresses; RPMB listener dependency failure

Multiple SUBMIT_DOORBELL END and device-stage zero returns are visible. This
supports progress beyond the old first-submit failure, not full storage proof.
The final assertion is RpmbListener.c:160, EFI_NOT_FOUND. Pinned UFSDxe RVA
0x137b8 calls LocateProtocol for GUID 9c1eb71f-dd6c-4ed5-9f6a-5cc0ca789f16
(bytes at 0x20368). Its failure path asserts, then falls through to dereference
the output. GUID matches also occur in ShmBridgeDxeLA, ScmDxeCompat and TzDxeLA;
occurrence alone does not prove which installed provider is required.

Quiet-return v1 redirects the failure-only block at 0x137c0 to existing epilogue
0x1382c, preserving x0's real error. Success branch 0x137bc and allocator body
are unchanged. Caller 0x48c4 already checks nonzero and records the error.
This is safe failure propagation, NOT an implementation of the missing service.
No RPMB key provisioning, RPMB write or secure-service bypass is introduced.

All per-request tracing hooks are omitted (original instruction bytes retained),
including descriptor stores, sync helpers, doorbell and barriers. Keep only four
stage wrappers and IOMMU configure summary. Startup's one-time topology logs
remain; original error printing remains. Static tests check actual error/success
branches, untouched real doorbell, and absence of repeated trace labels.

## 8650-test comparison and SMMU-inherit candidate

Reference fetched read-only at commit `4234f9221de6acfadea950a377f521aab740a2bf`:
https://github.com/GUF296/mu_aloha_platforms/tree/4234f9221de6acfadea950a377f521aab740a2bf
No reference files or submodules were copied over the working tree.

LanaiPkg/Device/qcom-8650 contains Lenovo Lapis panel data, sets EnableUfsIOC=1
and UfsSmmuConfigForOtherBootDev=1, and includes QcomPkg/PatchedBinaries/UFSDxe.inf.
Both configuration values already match Elden. Its PlatformBuild.py selects
Silicon/QC/SM8650, which is absent from this commit. Therefore package resolution
and a working Y700 image are NOT established by this checkout alone. In particular,
the public Sm8550 binary below must not be labeled a verified Y700 runtime binary.

The available Silicon/QC/Sm8550/QcomPkg/PatchedBinaries/UFSDxe.efi SHA256 is
`8a6eecab3ac7b6388b2b59a52fbc852f650614b97efc505058f21f7d2a1a22d6`.
Compared byte-for-byte with imported qcom-qrd8550 UFSDxe, only 11 bytes differ:
two instructions at 0x71d0/0x71d4 become mov w0,0;ret; and 0x17708 becomes
an unconditional branch to 0x1776c. The latter skips the domain attach call,
including its hardware stream reconfiguration, after domain configure.
The former helper bypass is not copied to Elden.

Elden equivalent control flow is 0x1b170 -> attach at 0x1b1c0 -> success at
0x1b1d4. The new candidate redirects only the successful CBZ w0 at 0x1b170
to 0x1b1d4. Nonzero configure results retain their original error path.
This is deliberately narrower than the reference unconditional branch.
Creating/configuring a fresh unattached domain is retained; attach is not called,
so this path does not replace the boot firmware's existing UFS stream mapping
with CB5. No foreign HALIOMMU binary, register address or memory map is imported.

Rationale: 9437 proves the current path chooses a fresh CB5, while the first
real DMA still fails. Preserving the inherited mapping is a candidate alternative
to replacing it, supported by the reference patch pattern, NOT a proven root cause.
Cold host reset may still affect DMA prerequisites, and the inherited mapping's
lifetime/accessibility remains unverified. Real DMA can still crash the device.

Package: elden-aloha-ufs-smmu-inherit-v1. Keep CT1, cold PHY/link setup, null-error
return and pre-BDS stop. Expected marker ATTACH_SKIPPED_KEEP_FIRMWARE; no attach
BEGIN on this path. Success requires real submit/initialization progress, not
merely seeing the marker. No new partition scan or media writes are introduced.
Tests cover the actual branch encoding, error-path preservation and pinned PE;
they do not establish runtime SMMU or storage correctness.

## Hardware follow-up: photo 9436

### Supplement 9437: attachment decoded

DOMAIN_20=0x0a9f00e0, DOMAIN_30=0xd5d1a218, DOMAIN_38=1.
ATTACH_NODE_00=0x15000000, ATTACH_NODE_08=0x0000000500000000,
ATTACH_NODE_10=0xd5d1a698, ATTACH_NODE_18=0.
The 64-bit NODE_08 sample packs two 32-bit fields: low word type=0,
high word context-bank index=5. This follows HALIOMMUDxe RVA 0x18c4
(STP w26,w8 at node+8) and 0x1aa4 (STR w25 at node+0xc).
Thus the software attachment selects SMMU 0x15000000, CB5, with a client
list and no following attachment node. It is not a null attachment.

The domain control matches the previously reversed mode-1 value with M=0.
Zero table pointers consequently remain compatible with the intended bypass
configuration. They must not be diagnosed as missing page tables on this evidence.
These are RAM structure values, not hardware register readback. SID matching,
effective S2CR/CBAR/SCTLR, stream ownership and post-reset persistence remain
unverified. HAL helper 0x2390 writes S2CR at base+0xc00+4*index;
0x23a4 writes SMR at base+0x800+4*index. SMR slot index must not be confused
with SID 0x60 or CB index 5. Helper 0x22f8 derives CB register base from ID1,
so an absolute CB5 address must not be guessed from the RAM node alone.

The CT1 candidate still hangs/reboots. The final visible marker is
CT1_TRIAL_REAL_SUBMIT; no SUBMIT_DOORBELL END is visible. CT correction alone
therefore does not resolve the observed failure. Do not repeat this candidate.
The earlier conclusion below is retained as the pre-test rationale, not proof.

The photo shows mask=1, MMIO base=0x01d80000, offset=0x4058,
TRL=0xd5d18000, TMRL=0xd5d19000, command buffer=0xd5d18400.
These agree with the intended doorbell address and basic alignment. The stock
queue programming at RVAs 0x11930..0x11974 uses the corresponding context
fields for the low/high base registers. This still does not prove DMA access.

Rechecked the logging wrapper: integer scratch registers x0..x18 and LR are
saved/restored, NZCV has its own slot, and the point hook branches back after
the original STR. No obvious integer-register corruption explains this photo.
The platform maps SMMU 0x15000000..0x151fffff as device memory; a simply absent
CPU mapping of this block is not supported by current source evidence.

The photo starts at ATTACH_NODE_18=0 and crops the preceding DOMAIN_* and
ATTACH_NODE_00/08/10 values. These are required to identify the selected context
bank and domain state. Neither the final marker nor the zero next-node pointer
identifies a SMMU fault. No new speculative register write is justified yet.

## Conclusion

The candidate corrects a concrete UTRD command-type compatibility mismatch.
It is not a confirmed explanation of the crashdump. Hardware validation is pending.
The previous gate prevented submission altogether; its stable return did not prove
DMA correctness. This candidate restores the real doorbell, so a crash remains possible.

## Evidence chain

- Photo 9412 reaches SUBMIT_DOORBELL BEGIN after descriptor construction, cache
  synchronization and the pre-submit barrier, then crashes. This localizes the
  failure to submission or its consequences, not necessarily the store itself.
- Photos 9433/9434 supersede the accidentally repeated 9415. IOMMU create,
  configure and attach return zero; cold PHY, link and peer stages return zero.
  Both requests are deliberately blocked before doorbell and return -5.
- HCI version reads 0x00000400. The MMIO base 0x01d80000 plus offset 0x4058
  resolves to HCI UTRLDBR 0x01d84058, consistent with the DT HCI base.
- The earlier gate-base value 0x80000000 was a diagnostic register-capture bug,
  corrected by sampling saved registers. It is not evidence of a bad real base.

## Command-type defect and fix

Pinned stock UFSDxe SHA256:
`dac620aca276e735361d8b747fe0218b6125eae004116391270f32a72e6ce750`.
RVA 0xf748/0xf75c supplies legacy device-management type 2. RVA 0x10c9c
shifts it into UTRD DW0 bits 31:28; RVA 0x10cbc stores the header.

Local Linux 7.1 `drivers/ufs/core/ufshcd.c:2797` sets
`UTP_CMD_TYPE_UFS_STORAGE`; `include/ufs/ufshci.h:434` defines it as 1.
The supplied BSP `drivers/ufs/core/ufshcd.c:2767` explicitly selects legacy
device-management only for HCI <=1.1 and storage type for newer controllers.
This is stronger evidence than assuming that every management UPIU needs CT=2.

The target-specific patch replaces instruction 0x53040d08 at RVA 0x10c9c with
0x52a20008 (`mov w8,#0x10000000`). Existing subsequent packing retains direction,
interrupt, crypto and key-index fields. UPIU transaction encoding is unchanged.
The pinned input hash and instruction assertion prevent applying this RVA to
another binary. A deterministic test checks the patch and lower-header-bit
preservation across modeled type/direction/interrupt/crypto/key combinations.
This test is not a hardware emulator.

Primary public cross-reference:
[Linux UFS core](https://github.com/torvalds/linux/blob/master/drivers/ufs/core/ufshcd.c).
The local checkout, not the moving public branch, defines the comparison here:
`/home/zhenlin/workspace/linux/linux-7.1`, HEAD
`45f2791cc4ff99801e0bb9d537394adb7aadc040` (HEAD is not a claim of a clean tree).
BSP root: `/home/zhenlin/workspace/qualcomm/sm8750-kernel-platform/kernel_platform/msm-kernel`.

## DMA, descriptor and SMMU review

Observed TRL 0xd5d18000, TMRL 0xd5d19000 and command descriptor 0xd5d18400
have suitable basic alignment. Matching CPU/sync addresses can be legitimate
with identity/bypass DMA; they do not establish accessibility to the controller.
The stock path runs synchronization helpers and a barrier before submission.
Their return alone does not validate cache attributes, full descriptor lengths,
ownership lifetime or the effective SMMU mapping. No claim of complete DMA proof
is made. Legacy queues are not invalid merely because the controller supports MCQ.

Pinned HALIOMMUDxe SHA256:
`5ecf601724cb65728b309b8ee9469c7d2160456efc97e0de2a4a5577dec25e7d`.
Reverse inspection of configure RVA 0x20f4 shows mode 1 stores control
0x0a9f00e0 at domain+0x20; bit M is clear. Attach RVA 0x1744 resolves IORT
mapping and programs context/stream state. Therefore zero table pointers are
not by themselves proof of a missing translation table. Extra logged x2/x3
values are not necessarily API arguments.

Linux `drivers/iommu/arm/arm-smmu/arm-smmu-qcom.c:475` describes firmware
bypass quirks; its reserved-context workaround is not justification for blindly
changing this firmware's S2CR. Return-zero attach is weaker than effective DMA
proof. SMMU/NoC fault details remain unavailable from these photographs.

## Power, clocks and reset

See [full DT audit](elden-ufs-topology.md), based on the supplied stock DTB and
`arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts:1097`, plus `sm8750.dtsi:5352`.
VCC=L17B ~2.5V; VCCQ=L1D 1.2V; PHY=L1J ~0.91V and L3G 1.2V.
BSP additionally describes reference/QREF/REFGEN supplies. Configured voltages
and enable properties are not measured rails.

The SoC lists AXI, aggregate, AHB, UniPro, reference and lane clocks, GDSC,
reset BCR, two interconnect paths (ufs-ddr/cpu-ufs), and SMMU SID 0x60.
SID matches the firmware configuration. Neither this match nor clock state=1
proves a live DMA path. Mainline qcom reset code uses an assertion delay and a
post-deassert settling interval. Successful cold PHY/link/peer completion lowers
the priority of speculative rail/reset changes but does not eliminate them.
No arbitrary PMIC, shared clock or reset changes are introduced.

## 9448 successful transaction and read-only follow-up

Device log confirms boot_b write/flush/read Success with match=1, followed by
restore/flush/read Success with match=1; final test=1 restored=1. This is one-block
runtime evidence, not power-loss persistence or endurance proof.
GPT inventory v1 sets ELDEN_BOOTB_WRITE_TEST=0, excluding the test transaction at
compile time. It retains CRC-checked primary/backup parsing and adds per-medium
partition counts, key partition ranges, and ESP GUID detection with a boot-sector
signature/FAT-label check. This does not mount a filesystem or load an EFI image.
PartitionDxe/Gpt.c can invoke PartitionRestoreGptTable on a damaged copy; no new
PartitionDxe connection is introduced during this strictly read-only stage.
The host harness also compiles the shipping disabled-write configuration and
asserts zero WriteBlocks calls for a valid GPT/boot_b fixture.

## 9447 rejection and stock backup layout correction

v1 rejected before writes; no stage was logged, so its exact runtime failure is
unknown. All supplied gpt_backup[0..5].bin reserve four blocks before the header.
The entry count is 32 except LUN4 (96); valid arrays occupy one or three blocks.
The v1 equality backup-entry-LBA == last-LBA - array-blocks therefore rejects
these layouts. v2 allows reserved padding while requiring the entire array to
remain after LastUsableLBA and before the backup header. CRCs, primary/backup
array comparison, nonoverlap, unique target and stock boot_b bounds remain.
The image GPT LBAs are flashing templates and differ from runtime media sizes;
do not transplant those header LBAs into the device. Runtime alternate-header
location still must match actual media LastBlock. New stage logging distinguishes
that rejection from the reserved-padding issue. Stock MBR CRCs across all six
images are 8B3C1444, matching the device's identical LBA0 CRCs.

## 9446 reads and authorized boot_b transaction

9446 shows six (not eight) BlockIO media, each LBA0 read returning Success and
CRC 8B3C1444. Four KiB of zeros instead hashes to C71C0011. Equal first blocks
alone do not establish correct addressing across LUNs.

The user explicitly authorized a boot_b write/read test. The new probe validates
both GPT headers and matching CRC-checked arrays, rejects overlaps and duplicate
boot_b targets, and cross-checks the stock XML range 321824..346399 at 4096 bytes.
Only the final block is modified. Two identical original reads precede writes;
the original block is held in RAM and restored even after a returned test error.
Each write is followed by FlushBlocks and an independent read/byte comparison.
Any hang/reset can prevent restoration. Slot A is a host-side prerequisite, not
a runtime assertion. No GPT writes or other partition writes are introduced.
Host tests execute the actual C function with mocked I/O and sanitizers; these
do not prove hardware DMA, flush persistence, or successful restoration.

## 9443 success and next read-only validation

9443 reports RPMB_LISTENER_ALLOC returning zero, UFSDxe Success, error count zero,
and eight BlockIO handles. The missing gEfiKernelProtocolGuid message is nonfatal
in this run. This does not prove RPMB secure services or media reads work.

UFS-READONLY v1 retains the successful driver configuration and adds one LBA0
ReadBlocks call per eligible fixed whole medium after driver success. It bounds
handle count, block size and alignment, allocates an aligned buffer, and reports
status/CRC32 without displaying contents or parsing partitions. It stops on the
first read error, without retry, WriteBlocks, FlushBlocks or Reset. Synchronous
driver calls have no independent watchdog in this probe and may still hang.
Build/regression checks are not device-read proof. Earlier hypotheses below are
historical and superseded where the newer transfer/driver evidence applies.

## Ranked hypotheses and decision (historical CT1 candidate)

1. **Confirmed compatibility mismatch, uncertain crash causality:** legacy CT=2
   on HCI4. Fix it; illegal CT might otherwise produce a normal error rather than
   crashdump, so do not treat this as the sole cause.
2. **Still plausible:** effective SMMU/NoC/DMA access fault at first request.
   Requires fault evidence or successful transfer to resolve; attach success
   and aligned addresses are insufficient.
3. **Still plausible:** remaining descriptor/cache/firmware integration issue.
   Current inspection does not prove every byte or DMA attribute correct.
4. **Lower priority:** rail/clock/reset/link prerequisites. Cold link/peer success
   argues against indiscriminate changes; no electrical measurements exist.

## Candidate and validation boundary

Candidate: `elden-aloha-ufs-ct1-fix-v1`. Real DMA is restored, unlike gated builds.
Keep original failure handling and the intentional pre-BDS stop. No new partition
scan or media-write operation is added; stock initialization is not read-only.
Recovery: `elden-aloha-ufs-doorbell-gate-v2` (previous non-rebooting gate).
Flash only the packaged boot.img to boot_a using the established workflow.

Success evidence must include returning from submission and subsequent device
initialization/BlockIO outcome. A completed build is not boot proof. If submission
still crashes, this falsifies the claim that CT correction alone fixes the fault;
do not repeat the same image hoping for a different result.
