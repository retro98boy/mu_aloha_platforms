# Elden microSD power, v6 command-status polling

v6 replaces masked-IRQ completion polling with command issued/complete bits
and TCS idle=1, as observed in photo 9519. It also requires an incomplete
sample before accepting completion: either immediately before trigger or
during polling. If hardware clears and completes too quickly to observe a
transition from an old complete value, the trial conservatively times out;
it does not claim success from stale state. No status-register writes added.
Host tests now simulate completion WITHOUT IRQ status, and retain stale-done,
partial completion and timeout coverage. Hardware validation still required.
Nine completed votes remain the test endpoint, not SD enumeration or BDS.

## Historical v5

v5 changes observation only, not the write sequence, interrupt mask, polling
bound or success criterion. Photo 9517 timed out on the first voltage request.
The v4 zero completion field was initialized software state, not a register
sample. v5 samples command status on timeout and prints the pre-request
command status/response plus post-failure IRQ, TCS control/status/enable/wait,
message/address/data and command status/response. A stale complete bit still
does NOT turn timeout into success (covered by the host regression).

Local Linux rpmh-rsc.c enables interrupts for active transfers and uses IRQ
status for completion. This does not establish masked-status semantics for
this trial. Do not enable an unhandled interrupt or reclaim the timed-out
TCS merely on the assumption that the transaction has completed.

## v4 change and limits

Photo 9516 qualified CmdDb addresses ldob9=0x41700, ldob8=0x41600,
ldoi2=0x42200; DRV2 version=0x30800, config=0x800c8104,
IRQ enable=1, status=0, all three active TCS control/enable=0, status=1.
v4 requires those resource addresses and hardware version/configuration.

v4 adds real RPMH writes on HLOS DRV2 TCS1, only while the active slots
remain idle and IRQ enable/status still match. The single-CPU pre-BDS trial
raises TPL_HIGH_LEVEL across each request to exclude other UEFI callbacks.
It never changes IRQ enables, TCS0, sleep or wake slots. This is NOT a
general resource-arbitration solution suitable for a normal boot sequence.

Each rail receives voltage, HPM mode=7, then enable=1. Requests outside the
exact three-rail allowlist (including every disable) are rejected. One
response-required command is sent per transfer. A new TCS1 completion bit
and issued/complete command status are required. Polling is bounded at
1,000,000 iterations, not a calibrated time duration. Completion-register
behavior with the interrupt masked is still a hardware test point.
On timeout the trial does not clear/reuse the possibly in-flight TCS and
issues no further votes. Reboot is required. No rollback/rail-off is attempted.

On completion it clears its trigger then mode, disables its command,
acknowledges only bit1 and restores the three command words. Existing
TCS0 interrupt settings remain untouched. The host test executes this
actual function against simulated MMIO and tests all nine allowed requests,
invalid/off requests, busy state, success, bad completion and timeout.

IMPORTANT: v4 intentionally does NOT reconnect the NPA adapter or retry SD
enumeration. The initial raw=4 remains expected. This image qualifies real
power requests first; a nine-vote completion is not an SD card read or a
physical voltage measurement. FrontPage/BDS/GRUB remain held. No disk writes.
Shared ldoi2 is never disabled. Inherited upstream BOB1/s7i supplies are not
reconfigured; the test does not establish their physical voltages.

## v3 evidence (historical)

This is NOT a completed power implementation. No new power writes are issued.
The existing SD driver still fails its NPA request until a backend is supplied.

## Board evidence

Local Linux 7.1 `arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts`:
`sdhc_2` uses l9b for vmmc and l8b for vqmmc. l9b permits
2960000..3008000 uV, l8b permits 1800000..3008000 uV.
The l2i regulator is fixed at 1200000 uV. The SD node itself does not
reference l2i; the three-rail association comes from XBL, not that node.

BSP `qcom/opensource/devicetree/qcom/sun-cdp.dtsi` maps SDCC2
to pm_humu_l9 / pm_humu_l8 and declares uses_level_shifter. The regulator
definitions in sun-regulators.dtsi map these to ldob9 / ldob8,
and pm_v8i_l2 to ldoi2. The reference board is not Elden;
its card-detect polarity must not be copied blindly.

Elden XBL configuration `porting-artifacts/xbl-config/fdt-000080dc.dts`,
sdcc/volt-regs explicitly sets:

| Rail | Purpose | Initial mV | Enable | Mode |
| --- | --- | ---: | ---: | ---: |
| ldob9 | SD VDD | 2960 | 1 | 7 |
| ldob8 | VCCB level shifter | 2960 | 1 | 7 |
| ldoi2 | VCCA level shifter | 1200 | 1 | 7 |

XBL also allows ldob8=1800 for voltage switching. Do not attempt that
until the SD voltage-switch protocol is implemented. BSP sun-cdp.dtsi
also uses pm_v8i_l2 for UFS vddp-ref-clk: do not power-cycle ldoi2.
Mainline supplies ldob8/9 from BOB1 and ldoi2 from s7i; upstream supply
state also matters. Command completion alone is not a voltage measurement.

## Other Aloha implementations

Samsung gts7fewifi DXE.inc includes patched SdccDxe plus full PmicDxeLa,
RpmhDxe and NpaDxe. Elden intentionally excludes the full PMIC entry;
its transitive policy/peripheral writes remain unaudited (see
elden-pmic-boundary.md). Enabling that entire entry is not a narrow SD fix.

CranePkg has source RPMH/CmdDB drivers, but its SM8450 target uses
0x17a20000. Its RpmhCrDxe entry also contains enabled ldoc3 power tests.
Do not include that driver unchanged on Elden. SM8750 DT instead selects
DRV2 at 0x16520000, TCS offset 0xd00, active slots 0..2.

## Why this test is needed

The v2 photograph proves null NPA handles, not absent physical voltage.
Before introducing direct RPMH transactions, v3 queries stock CmdDb for the
three exact rail addresses and reads the DRV2 version/configuration and
active TCS state. It neither borrows a TCS nor changes an interrupt bit.
Missing addresses, a non-VRM address, or unknown hardware layout fail closed.
No MMIO write, NPA resource fabrication or SPMI write is introduced.

CmdDb ABI is pinned by SHA256 and protocol-table tests: revision 0x10000,
method +0x10 at stock RVA0x14bc takes (This, Name, UINT32 *Address).
It returns EFI_SUCCESS with zero address on lookup failure; both are checked.
RSC offsets match local Linux drivers/soc/qcom/rpmh-rsc.c v3 layout.

Needed next: the v3 rail-address and RSC lines to qualify the backend.
Then implement real, completion-checked power votes with bounded waits and
retain the shared UFS rail; do not bypass the SD power error with success.
USB, BDS, FrontPage and GRUB stay paused. SD BlockIo remains write-protected.
# v7 fixed-voltage enumeration trial

Photo 9521 confirms all nine v6 requests completed with fresh command status
0x10101. This is not a voltage measurement or SD read proof.

After the nine requests complete, and only with DTflag=1, the diagnostic
installs a six-instruction fixed-voltage adapter at loaded SdccDxe RVA 0x58ec.
The immutable input PE remains hash-pinned and unchanged. Runtime installation
checks image size and every original instruction, then synchronizes data and
instruction caches. The original function prologue, DT initialization, stack
cookie and epilogue remain intact. Only drive=1, enable=1 succeeds; disable and
other drives return raw 2. No NPA handles are fabricated. This latched adapter
is valid only for this single-boot, log-hold trial with no rail-off operations,
no RPMH driver and no APs. It is not a normal-boot power-management solution.

The separate low-voltage routine at RVA 0x4d24 remains unchanged: it checks
all three client handles before scalar requests and returns raw 4 if absent.
Thus v7 does not implement 1.8V switching/UHS; such a request can fail card
initialization rather than silently claiming a voltage transition succeeded.

The removable event is signaled a second time only after adapter installation.
Its return is event status, not card status. Verification requires a removable
BlockIO media and successful LBA0 read. BlockIO WriteBlocks stays rejected;
no filesystem connection/mount, BDS, FrontPage or GRUB is started.
# v8 ACMD41 fixed-voltage negotiation

Photo 9522: nine votes and adapter installed, then the d2a0 caller prints
"Failed to initialize the card." No removable BlockIO appeared. This exact
string is at RVA 17bd7, used by 98c0 after d2a0 returns nonzero; it is not the
earlier host initialization b9d8 failure. d2a0 performs CMD11/voltage switching
for type 7, then CMD2/CID handling. The photo does not identify which failed.

Audit found e7dc sets ACMD41 arguments to 01ff8000 (CMD8 fallback) or
41ff8000 (CMD8 accepted), both containing S18R bit24. The local Linux
include/linux/mmc/sd.h defines this bit as the 1.8V switching request.
v8 clears only bit24 in both immediate constructions, retaining HCS and the
voltage window. Responses, return codes and low-voltage rejection are intact.
The high-capacity response decoding still selects type5/type7 from S18A; no
card type is forced and CMD11 is not blindly skipped.

The existing error string now prints the actual d2a0 raw return, preserving
w22 for the original error path. Raw34 is explicitly generated at d2f8 for
failed voltage switching; other failures need further tracing. After the
second event, software context fields (d83c(1)=image+1c940) are logged without
invoking the driver or doing additional hardware reads. No SD writes or
FrontPage/BDS startup is enabled. Build/static checks are not hardware proof.
# v9 bounded-scope stage/command tracing

Photo 9523 stops at powered enumerate BEGIN, then the user reports reboot.
No evidence identifies the reset cause or last command. Keep v8 negotiation,
power votes, write rejection and log hold unchanged. Do not disable watchdogs.

Trace five calls from sdcc_handle_open: b9d8 (host setup), d124 (identify),
d2a0 (voltage/CID), e158 (selection/status), d420 (subsequent configuration).
Each has a BEGIN and END raw result. Also wrap c44c command execution with
command number/argument before and raw result/first response word after.
Response is a raw buffer observation, not valid data when the command fails.
Logging adds latency and can affect timing; it is diagnostic, not a fix.

Wrappers occupy only 3878..3c87 within the already-skipped embedded MMC
initialization block. Existing entry branch at 3874 still jumps to 3c88.
Save x0..x7, caller FP/LR on an aligned 96-byte frame; restore arguments
before invoking the original callee, and preserve the full x0 result across
the final print. The c44c wrapper replays its displaced sub-sp instruction
before entering c450; original command logic and error paths remain intact.
No guessed callbacks, new relocations, filesystem access or hardware writes
are added. Static tests check relocation exclusion and interpret emitted
instructions with clobbering print/callee stubs for success/error returns.
Hardware execution remains to be tested.
# v10 command issue / completion wait / error reset trace

Photo 9525 confirms CMD8 response 1aa, ACMD41 eventually c0ff8000,
identification type5, d2a0 and e158 returning zero, CMD7/CMD16/CMD55
returning zero, then ACMD51 BEGIN without END. Reset cause is unknown.

c44c does not consume the data buffer: it issues the command through 88e4,
waits for command-complete/error through c374, and uses 8594 for an error
reset. c374 polls status with mask1|8000, includes a 100-us Stall, and has
an approximately five-second iteration budget. This does not prove Stall
or MMIO reads themselves return. 88e4 writes the 16-bit command register
at the selected controller base+0e. Error reset writes base+2f and polls.

v10 adds wrappers at c590 (88e4), c5a0 (c374), c5e4 (8594). Remove the
already-verified host b9d8 wrapper to stay inside the same unused MMC code
block. For the wait only, print the driver's output interrupt-status word
after return; no additional MMIO accesses. The two HAL functions are void,
so their printed raw x0 is NOT a success/error status; only END matters.

All power, ACMD41 negotiation, command logic, polling budget and DMA setup
remain unchanged. This is localization, not a claimed fix. Interpret the
last pair: 88E4 BEGIN without END = issue helper; C374 BEGIN without END =
completion helper; WAIT END with error followed by 8594 BEGIN without END
= error reset helper. Wrapper tests also verify saved x2 status-pointer
handling. Actual hardware behavior awaits the next flash test.

## v11: stock PIO isolation, hardware result pending

9530 stops inside 88e4 during ACMD51, before command-completion polling.
This does not prove which instruction hung or whether DMA was enabled.
For the SD-only diagnostic PE, replace be10 strb w9,[x20,#16] with
strb wzr,[x20,#16]. This per-transfer flag skips c19c ADMA preparation,
clears Transfer Mode DMA Enable through c348/c35c/8964, and selects the
stock PIO branch at bf24. Persistent capabilities, IOMMU configuration,
power votes, voltage negotiation and host clock policy are unchanged.

PIO reads two words for the eight-byte SCR and uses c374 for buffer-ready
and transfer-complete polling. Its existing approximately five-second
iteration budget is retained; it cannot recover from a stuck MMIO access.
Replace the earlier d124 trace with c0a0's PIO buffer-ready wait trace;
all other issue/wait/reset and command diagnostics stay. The startup marker
is ELDEN SD v11 BEGIN: PIO forced. No BDS/UI, no card writes.

Checks verify the patched store, unchanged flag consumers and wait loop,
wrapper argument/return preservation, FFS payload equality and power-vote
simulation. These checks are not physical DMA, timing, or SD validation.
Flash boot_a only; preserve boot_b's existing GRUB/FAT test image.

## v12: SD partition and filesystem read probe

Photo 9532 confirms v11 passed ACMD51, created removable BlockIo (119 total),
and read LBA0 with 55AA signature. Access-mode switching still reports a
failure; high-speed support is not established. Retain PIO and power policy.

After successful LBA0 read, connect only that parent nonrecursively, then
BlockIo children whose validated device paths match the parent's prefix.
Enumerate only matching SimpleFileSystem handles; OpenVolume, open
EFI/BOOT/BOOTAA64.EFI read-only and read at most 512 bytes. Close all opened
files. MZ is a header marker, not full PE validity or successful execution.
No LoadImage/StartImage, no BDS/UI, no file writes. Stock BlockIo write
rejection remains. Limit global handle inventory to 512 and matching
handles to 32 per pass. No SD formatting or boot_b modifications.

## v13: GPT transport comparison

9542 reports invalid GPT with repair disabled, PART=0, FS=0. Do not infer
damaged media from command success or that single diagnostic. Keep v11 PIO.
After the existing filesystem probe, read primary LBA1 and backup at LastBlock
using 512-byte reads; print header fields and calculate header CRC in RAM.
Read each bounded entry array both sector-by-sector and as one multi-block
request, compare all padded bytes and report both CRCs over exact array size.
Limit each array to 64KiB, verify media extent by subtraction, use separate
64KiB-aligned buffers and an aligned single-sector scratch buffer. Never
allocate from disk-supplied counts or install partitions from these results.
Header CRC failure does not prevent bounded diagnostic reads. Full GPT
semantic validation is still the partition driver's responsibility.
Final summaries repeat both results after command trace output. Zero means
unchecked, not success. Equal single/multi reads do not exclude repeatable
common-mode transport corruption. No disk writes, repair, or EFI execution.

## v14: independent read-only first-partition FAT listing

Historical experiment: `EldenSdFat.h` and `test-elden-fat.c` have since been
removed. Normal filesystem drivers and the EFI picker now handle discovery;
the following describes the original diagnostic, not the current boot path.

Primary and backup array CRCs passed independently but differ on the supplied
photo. Use only primary GPT's first occupied entry, after header and array
CRC and partition bounds checks; never silently substitute the backup.
The new EldenSdFat.h parses 512-byte FAT16/32 BPBs and walks root, EFI, and
EFI/BOOT directories using existing PIO BlockIo single-sector reads. It
prints 8.3 aliases, directory/file type and file size; long-name entries are
skipped deliberately. FAT12, exFAT, and non-512-byte BPBs are not supported.
Each directory is bounded to 256 sectors and 128 printed entries. No file
contents are loaded or executed and no FAT/partition repair is attempted.
Per-command and per-sector wrappers are disabled to preserve directory
output; power diagnostics and coarse initialization wrappers remain.
test-elden-fat.c compiles the actual header against a synthetic read-only
device and exercises FAT16/32 directory traversal, alignment, invalid cluster
size and out-of-partition BPB bounds under ASan/UBSan. Real media pending.

## v15: valid primary GPT, read-only removable-media policy

9544 confirms independent FAT listing including BOOTAA64.EFI. The blocking
condition was Elden's no-repair gate requiring AlternateLBA == LastBlock.
Within ELDEN_PARTITION_NO_REPAIR only, removable 512-byte media now uses
the validated primary exclusively. USB remains disabled in this build.
Backup failures/location/content mismatches warn, never restore or merge.
Fixed UFS media retains its previous gate. Added removable checks bound
arrays to 512 entries of 128 bytes, validate header/usable/array extents,
recheck CRC on the actual reread used for child creation, and reject the
table on out-of-range or overlapping entries. No changes to FAT or BDS.
PIO, write rejection and terminal log hold remain. Static tests check
policy scoping, no repair, CRC and extent/overlap guards; hardware pending.

## v16: BDS/FrontPage restored, SD before UFS

Remove the terminal SD log hold and compile out the independent GPT/FAT
diagnostic. Keep real SD parent/partition/filesystem connection before BDS.
DeviceBootManagerAfterConsole refreshes standard boot options then sorts
BootOrder using connected BlockIo media: removable (SD), fixed (UFS), other.
This mapping is specific to current Elden with USB paused, not a general
USB/SD classifier. Explicit BootNext remains authoritative. No direct
StartImage shortcut; normal BDS tries options and falls back to FrontPage.
Print sorted Boot numbers/source and completion status. Existing physical
keys and FrontPage reset remain unchanged. Standard BootOrder updates use
the firmware variable service; no SD/GPT writes or repair are introduced.
RPMhDxe remains excluded, no APs are started, and the existing fixed-voltage
SD power votes remain. OS handoff/power ownership and DMA are not validated
by restoring BDS; keep this distinction in hardware reporting.
