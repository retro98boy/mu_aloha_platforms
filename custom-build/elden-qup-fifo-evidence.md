# Elden QUP FIFO investigation

## v7 zero-context engineering trial

9469 reports all ten AP0..4 TX/RX context/scratch pairs zero; 9470 reaches
normal terminal stop with UFS intact. v7 accepts GSI=F only if all twenty
words are zero; other values retain rejection. Immediately before stock
loader entry, GENI and DMA activity are rechecked. No GPI commands are sent.
This is not proof of exclusive ownership across EEs, nor an atomic lock;
zero reads and an idle snapshot are only bounded engineering conditions.
16 instruction-model tests cover zero-context recovery, nonzero rejection,
unknown event bits, and GENI becoming busy after the snapshot (no writes).
PMIC/VBUS gates remain unchanged; USB operation remains unverified.

## v6: bounded GPI ownership snapshot (not a takeover fix)

9465 shows v5 reason=6, status=0 mode=1 dma=0 gsi=F. The loader has not
executed. GSI event enable is configuration, not channel activity proof.
Stock 1.1.11.263 DT SE5 has qcom,shared and DMA tuples (0/1,5,3,64,0).
BSP msm_gpi.c:gpi_of_dma_xlate decodes the final argument's static selector;
zero selects dynamic assignment. SE5 is not GPI instance 5.
Stock GPI1 has base A00000, EE offset 10000, dynamic mask 1E and static mask 1.
BSP msm_gpi_mmio.h plus msm_gpi.c EE subtraction gives context address
A10000+n*4000+k*80 and scratch +60. v6 reads only instances 0..4,
channels 0/1, only those two registers, after clock enable and idle/protocol
checks. Five lines report raw context/scratch pairs. It sends no GPI command
and retains rejection reason 6. It does not assume a Linux AP mask proves
exclusive runtime ownership or that a snapshot eliminates races.

The 13 instruction-model cases verify the exact 20-read allowlist, five
correct log tuples, no writes on rejection, balanced clocks and saved ABI.
These are model checks, not device-access or USB-host validation. PMIC and
UsbPower gates remain unchanged. Need hardware state before any proposed
shared-channel stop/reset or firmware reload with GSI events enabled.

9460/9458: normal terminal stop, UFS partitions intact. The pinned I2C
rejection hook prints base=A94000 disable=1. This matches mainline sm8750.dtsi
i2c5 and its gpi_dma1 TX/RX channel arguments (0/1, SE 5, I2C).

I2CDxe original 59ec loads context+0x18, 59f0 reads base+0x64,
59f4 tests bit 0. No wrong-engine evidence remains from this observation.
Do not bypass this check and call the FIFO transport successful.

Local Linux 7.1 drivers/soc/qcom/qcom-geni-se.c defines a DISTINCT writable
SE_FIFO_IF_DISABLE at 0x2008. geni_load_se_fw clears its bit 0 for FIFO mode
after resource enable, firmware/config RAM programming, clock sequencing,
transfer-mode configuration and DMA-interface enable. Thus read-only at 0x64
does not mean permanent hardware disable, but this firmware-load sequence is
not evidence that a live isolated write at 0x2008 is safe on Elden.

Local drivers/i2c/busses/i2c-qcom-geni.c selects GPI when IF_DISABLE_RO bit 0
is set. The imported I2CDxe has an explicit GPI-not-supported message in its
client cleanup branch at 60e8..60f8. This alone does not establish every GPI
entry path; installing GpiDxe alone has not been shown to connect a transport.
No GPI/FIFO mode switch or PMIC modification was performed in this audit.

The available named qupv3fw.elf under the Elden tree is from the
TB322_ZUXOS_1.5.10.259_Tool image directory, not the current 1.1.11.263.
Do not silently substitute it or flash that partition. Obtain the matching
firmware before qualifying a firmware-reinitialization alternative.

## Supplied qupfw_a.img

User supplied ../qupfw_a.img, 81920 bytes, SHA256
62ae4aad00c2579e65bf57e01f85dfa2df7ad97eaad0fb2089704c2f05aabcb6.
ELF32 with 14 program headers; 11 SEFW segments. Bounds inspection succeeds.
I2C protocol=3, core_version=0x40000000, fw_version=0x0204,
cfg_version=9, code=343 words, configuration=69 index/value pairs.
All 11 complete SEFW segments match the local 1.5.10.259 qupv3fw.elf.
Whole files do not match (1542 differing prefix bytes and 17200 zero padding
bytes in the dump). This is payload comparison, not signature verification
or proof of runtime firmware revision/ownership. No partition was written.
The prior missing-payload concern is resolved; safe live reinitialization is
still unqualified. inspect-elden-qupfw.py reproduces the read-only comparison.

## v4 bounded recovery implementation

Further static tracing identifies I2CDxe 0x6e68..0x71c8 as the stock loader.
0x8b58 selects the embedded I2C header at 0xc1a8, size 0x6d1. Its complete
SHA256 is 311a91dce7f1c6c38f52f885f3f1b7da06b8da09507acb1e2a47a171f59a13de;
the supplied qupfw segment at 0x63f0 is byte-identical. No replacement blob
or qupfw partition update is needed. 0x6ea4..0x6eb0 returns immediately if
the protocol already matches; that path does not restore FIFO mode.
0x706c..0x7088 configures FIFO/DMA/GSI routing, 0x713c..0x7160 copies code,
0x7178..0x71c4 finalizes clocks and writes DMA_IF_EN=1/FIFO_IF_DISABLE=0.

v4 retains the original guard and diverts only its failure branch. The new
hook checks SE base A94000, clocks-on return, inactive M/S GENI commands,
I2C protocol, DMA_MODE_EN clear and GSI_EVENT_EN zero. It then reproduces
the loader's exact stack/register setup and enters 0x6eb4, skipping only
the same-protocol shortcut. Original code and embedded firmware stay intact.
It checks FIFO/protocol readback, balances successfully acquired clocks,
and reports a single numeric reason. Success rejoins 0x59f8; all refusals
rejoin 0x59c8. No PMIC, VBUS or persistent-media writes are added.

test-elden-i2c-recovery.py executes the assembled hook and original loader
body in Unicorn, with clock services, memcpy and MMIO reflection modeled.
Ten cases cover success, wrong SE, clock failures, M/S busy, wrong protocol,
DMA/GSI enabled and failed FIFO readback; callee-saved registers and SP
are checked. Refusal cases 1..6 perform no modeled loader MMIO writes.
The copied firmware RAM matches the embedded payload. This tests control
flow/ABI, not real ownership, asynchronous races, clocks or electrical safety.
In particular idle/GSI flags are not complete cross-environment ownership
proof. Hardware qualification remains pending; v4 is an engineering trial,
not completed USB host support, and it does not force an active GPI takeover.

## v5: mode is not activity

9461 reports recovery=5; 9462 retains 112 partition children and 5 SimpleFS.
The BSP msm-kernel/include/linux/qcom-geni-se-common.h defines E40 as
SE_DMA_DEBUG_REG0, bit0 DMA_TX_ACTIVE and bit1 DMA_RX_ACTIVE (lines 69,92-94).
The BSP I2C debug dump reads this register (i2c-msm-geni.c, around line384).
Mainline geni_se_select_dma_mode/select_gpi_mode set +258 as configuration,
not as a busy indication. v5 checks E40 activity instead; it retains the
GSI-event rejection as a conservative ownership boundary. GSI events are
also configuration, not proof of an executing transfer.

Snapshot +40/+258/+E40/+E18 is collected only after base and clock checks,
before recovery, and printed once. Tests additionally assert no MMIO reads
on wrong-base/clock-failure paths and verify the actual emitted snapshot.
13 emulated cases pass, including DMA mode=1/activity=0 success and TX/RX
activity refusal. Mode/GSI changes and asynchronous ownership remain subject
to real-board qualification. No new PMIC, GPI channel or persistent writes.
