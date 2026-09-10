# Elden USB reference audit

## Hardware boundary

### Fixed-long-edge USB2 scope (9474 / 9476 follow-up)

The v8 hardware snapshot is `1,10,1C,1,64,1F4` (hex). AE78 passes
config+4 as the I2C-open argument through AB58/E288/E300; config+8 is
stored in the transfer configuration. Do not call 0x10 the slave address.
Local BSP `sun-qrd.dtsi` has an ONNN USB/DP redriver at SE5 address 0x1c.
Elden stock DT instead declares an external eUSB2 repeater at 0x4f, as
does mainline `sm8750-lenovo-elden.dts` (PTN3222). This is evidence of a
configuration mismatch, not proof that every field came from QRD.

The generator now patches A1B8's feature-flag load to `mov w8,wzr`.
A1BC then takes the original disabled-feature path A1C4/A1C8, returning
FALSE. The direct caller at 66E4 ignores this result and continues at
66E8. No redriver is reported initialized. The v8 defensive probe remains
at AE7C should another route reach that initializer. No USB2 repeater,
IOMMU, host-mode or power routine is bypassed by this new instruction.

This is an OFFLINE candidate, not a USB2-host-enabled release: the long
edge mux polarity, PTN3222 initialization, VBUS/CC and host DMA still need
qualification. In particular a redriver exclusion is not a DWC3 speed cap.
Do not ask for a flash solely to retest the already-observed missing USB
power protocol. Preserve UsbPwrCtrl's FALSE gate and the working UFS path.

Further source qualification: linux-7.1 `drivers/phy/phy-nxp-ptn3222.c`
has no I2C transfer in probe/init. Init enables two supplies and deasserts
reset; exit asserts reset and disables supplies. Thus mandatory QUP FIFO
recovery is NOT established for a minimal USB2 route. This does not prove
Elden needs no BSP tuning, or that its inherited power/reset state is valid.
Do not substitute the QRD 0x1c register sequence with address 0x4f.

Offline validation: the defensive probe cases and 15 disabled-feature cases
pass in Unicorn; the normal disabled return is FALSE, with no device writes.
Full Docker build and actual-FD USB PE/dependency checks pass. No new flash
package was produced: this change alone cannot enumerate a USB device.

v8 implementation: hash-pinned AE7C call is replaced by a branch into unused
text padding at 12EB0. It reads six words from the already-validated config
pointer x20+18, prints them, and branches to the stock AE94 failure path.
AB58 is not called by this path, so no redriver I2C open/FIFO reload occurs.
Three Unicorn cases verify field order, no memory writes, branch destination
and preserved callee-saved registers. Logging service is mocked, so this is
not runtime validation. The FD checker compares the actual USB PE against
the generator, while USB power remains explicitly gated. No host policy or
IOMMU bypass is applied. Request one hardware config snapshot next.

Source follow-up: the v7 zero-context enabled-GSI reload branch is disabled
again, now marked HOLD. Existing v7 artifacts are unchanged and remain
known-failing. 16 instruction-model tests pass with zero contexts rejected.
No new device image was built as part of this source safeguard.

Redriver trace: A1C0 tail-branches to AEC0. AEC0 validates core 0 and a config
argument below 5, then AF00..AF0C calls AD6C if context/config is absent.
AD6C checks configuration enable fields, then AE7C calls AB58, which calls
E288. E288 locates a protocol and enters its wrapper; AB58 logs I2C init
failure on nonzero result. The requested redriver configuration is acted
upon only after this initialization. This does not establish every caller
or every mode, but defeats the assumption that changing the controller
startup argument alone eliminates the observed initialization dependency.

9472: v7 prints AP0..AP4 zero context/scratch, then hangs/reboots before
the recovery return line. This bounds the failure to the post-snapshot path;
it does not identify an instruction, watchdog cause, or prove loader entry.
Do not repeat v7 as a known-working image. Zero AP contexts are not sufficient
qualification for live firmware reload. PMIC/VBUS gates must remain intact.

## Local SM8550 UsbConfig byte comparison

xiaomi-socrates stock SHA256:
346008b17d2fceac17a6624d1bc78268f9d0dd1a74974643bce084b20621a967
patched SHA256:
a655add6d4949244049dcb9659e4fd25f4ae7ff0988d2140ba37a96e1ed50455
Both 86016 bytes; only 15 bytes differ in five instruction words:

* 17A4: mov w1,4 -> mov w1,1, before call 47F0. The nearby string is
  UsbConfigStartControllerCb / UsbStartController failed. Enum semantics
  are not yet established for this exact binary; do not label 1 host yet.
* 3F70/3F74: conditional branch -> unconditional branch to 41C8 and NOP.
  This skips a region starting with LocateProtocol using GUID
  54b6d3b4-5d33-4f91-8600-6c41d5deb19a (Elden HAL IOMMU GUID).
  The complete skipped region and later consumers need tracing before porting.
* 538C/5390: conditional branch -> unconditional branch to 53C0 and NOP.
  Skips an indirect call through object+18 with string USB0; the object
  protocol and exact operation remain unqualified.

Odin 2 has exactly the same five changed word offsets and 15 changed bytes.
NX729J changes four words at 3F64/3F68/536C/5370, 14 bytes. Offsets are
binary-specific, not portable patches. These observations do not prove USB
works on hardware, or that bypassing IOMMU is safe for Elden DMA.

Kailua DXE manifests include original I2C, patched UsbConfig/PMIC, UsbPwrCtrl,
UsbInit, USB bus and mass storage. Elden still lacks PMIC Version, USB Power
and NPA USB resource providers. Fixing FIFO alone cannot satisfy that chain.

## Next qualification

### Elden mapping, not copied SM8550 offsets

Elden UsbConfig has controller start at 4470. It normalizes 8->1 and 10->4
at 44FC..4514. Mode 4 at 45A0 selects the peripheral handle path; its error
branch 4708 references FF39 (Peripheral Client Handle). The alternate accepted
mode 1 path at 463C references FFEF (Host Client Handle) through 4718.
Thus the mode semantics match the reference for these binaries.

Elden differs in callback structure: 1C84 calls 4F30, which conditionally
starts core 0 with mode 4 at 4F88..4F90 when current mode is 10000. Another
callback at 1CBC loads globals 14674/14678, calls 4FD0, and that routine
sets a mode override of 4 at 4FFC..5004 and calls 4470 with 4 at 5044..504C.
The globals are written by 1960 (arguments w2/w1 saved at 19A8).
Changing only the first mov to 1 would leave the second peripheral override
intact. Do not equate a one-word mode patch with a complete host policy.

The 9472 fault occurs in UsbConfig's redriver initialization before a recovery
return. Mode selection alone has not been shown to bypass that dependency.
No live power/IOMMU patch was made during this static mapping.

### Follow-up: mode and USB0 object provenance

The stock 47F0 routine accepts primary-core mode 1 or 4 (4998..49A0).
4C50..4C58 routes mode 4 to installation/connection of the Peripheral Client
Handle (strings 105C8/1061D). The other accepted value 1 routes to Host Client
Handle (strings 1066C/106BB). Thus the 17A4 edit changes the startup callback
from peripheral to host, for this pinned binary. 4940..4974 also normalizes
8 to 1 and 10(hex) to 4. This is not yet an Elden offset or enum verification.

The previously unidentified USB0 call uses the SAME HAL IOMMU object:
3FEC loads the located HAL protocol; +8 returns the object at global 13770;
its +0 creates the domain stored at 13778. +40 configures that domain;
+10 attaches USB0 (4130, with argument 03000000). The patched-out +18 call
at 539C uses the same object/domain and USB0 name. This establishes it is
part of the IOMMU lifecycle, not a PMIC/USB power protocol call. The exact
+18 method name still requires an interface definition/provider analysis.
Consequently the two branch patches are related lifecycle changes, not two
independent general-purpose initialization fixes.

Resolve the controller-mode enum, the skipped USB0 operation, and all
HAL IOMMU consumers. Compare the corresponding Elden functions semantically,
not by offsets. Independently qualify the minimal PMIC provider path before
any USB power enable. No reference binary or PMIC policy has been copied.
