# Elden PMIC dependency investigation

The existing PmicDxe exclusion remains intentional. This audit does not claim
USB works or that a safe PMIC patch has been completed.

Pinned PmicDxeLa SHA256:
7203955b1a6dbda18186ca45fad129a7b0ad7c2f4f15b306c47f50c0dd3deb7c.

Entry implementation at RVA 0x1628 has distinct phases:

| Call site | Target | Evidence |
|---|---|---|
| 0x164c | 0x13688 | resource/config initialization before publication |
| 0x1660 | 0x1750 | InstallMultipleProtocolInterfaces, large protocol set |
| 0x1670 | 0x133e4 | another InstallMultipleProtocolInterfaces call |
| 0x16dc | 0x13b00 | additional policy processing, argument 2 |
| 0x16e4 | 0x1570 | two CreateEventEx registrations, callback 0x156c -> 0x4168 |
| 0x16f0 | 0x13450 | explicit register writes followed by more calls |
| 0x170c | 0x42c4 | ADC entry; followed by AdcEntryPoint log |

0x13450 issues calls to the byte-write wrapper 0x86b0 with first argument 7:
0x2690=0x33, 0x2848=0x0f, 0x284a=0x60, 0x284b=0x5a,
0x2980=0x9b, 0x2954=0, 0x2a54=0, 0x287f=0; later 0x2848=0x8f.
These are observed call arguments, not a verified peripheral/PON register map.
Do not call all these writes PON writes merely from their proximity to init.

0x13688 itself calls target/config initialization, resource-manager setup,
RPMH/PRM/XO setup (names supported by adjacent log strings), and an inventory
loop. Its full transitive write set has NOT been established. Therefore neither
skipping only 0x13450 nor jumping straight to protocol publication is yet a
validated service-only initialization path. ADC has its own initialization.

Next implementation gate: resolve transitive initialization and callback
writes, then preserve necessary service state while excluding boot policy.
No original PE, PMIC hardware state or UFS behavior changed in this audit.
test-elden-pmic-boundary.py verifies the image hash and direct branch targets.

## Offline policy-bypass candidate

prepare-elden-pmic-candidate.py changes CBZ W8 at 0x1678 from destination
0x16d8 to 0x16f8. Both publication results must already be zero to take this
path; W21 retains the first publication's zero result. Resource initialization,
publication failure paths, timing/log call at 0x1700 and ADC initialization
remain unchanged. It bypasses policy 0x13b00, registration 0x1570 and writes
0x13450 without replacing a protocol method with fabricated success.

The registered GUIDs at 0x35048 and 0x35078 match ReadyToBoot and
ExitBootServices in MdePkg.dec. Callback 0x4168 logs pm_exit_cleanup and
invokes an optional located protocol method. It is not established as harmless
logging. Skipping its registration also removes cleanup; OS handoff is NOT
validated by this experiment.

Resource manager 0x2c3fc dispatches peripheral-type-specific initializers via
a jump table at 0x2c514-0x2c520. Thus this candidate does not prove the remaining
initialization is read-only. It is deliberately absent from build-elden.sh and
the firmware FDF. Tests pin the original hash, one changed byte, conditional
branch destination and build exclusion. No new flash image is authorized by
these static tests.

## USB power entry is not passive

UsbPwrCtrlDxe (SHA256
f168e7fcde1e5c2dc6a25428167c16b91d21d64ad9a3662fe1b15016951c2ef0)
calls 0x4dec at 0x4184 during entry. The per-port policy calls 0x4b68
with enable=1 for port type 3. Function 0x4b68 references the stock
UsbPwrCtrlLib_SetVbusEn string at 0x727c and dispatches power backends.
Whether Elden selects this port type is not yet established. Provider presence
alone must not release this entry. FDF therefore has an explicit FALSE operand;
the built DEPEX is tested with all eight PMIC-provider combinations.
This blocks automatic dispatch, not an independently executed copy of the PE.

Correction to tentative ABI interpretation: Schg method +0x110 (0x25608)
fills a structure with two 16-byte arrays and a count at offset 0x20; it is
not a scalar PMIC-index getter. Do not call it with a four-byte output buffer.
The existing preflight only enumerates handles and does not invoke it.

## Fixed long-edge USB2 / 5V request: board control boundary

The user now explicitly requests fixed long-edge host and 5V output. This
authorizes the feature, but does not identify the electrical control backend.
Stock ZUXOS_1.1.11.263 DT declares `/soc/qcom,pmic_glink` with channel
`PMIC_RTR_ADSP_APPS`, subsystem `lpass`, protection domain
`msm/adsp/charger_pd`. Battery charger and UCSI are children of that node.
The stock UCSI node also references TLMM GPIO29; that is not evidence of a
VBUS enable pin. Mainline Elden identifies connector 0 as long edge and
uses GPIO29 for the eUSB data mux default. VBUS must be qualified separately.

The local BSP `drivers/power/supply/qti_battery_charger.c` and
`drivers/usb/typec/ucsi/ucsi_glink.c` provide message-based interfaces, not
an Elden long-edge direct 5V enable recipe. `PmicGlinkDxe.efi` contains
the matching channel names, but its presence does not establish a live
ADSP endpoint or a callable, qualified source-role method in current Aloha.
`UsbPwrCtrlDxe`'s previously audited direct backend remains another possible
route; its port/backend selection and protection semantics are not resolved.

Do not release its FALSE DEPEX merely because 5V output was requested.
Before a powered test image, establish connector/backend mapping, existing
VBUS/source conflict detection, current limit/protection behavior, and the
disable/error path. No powered image or new PMIC write was produced by
this inspection. A board power schematic or Elden-specific stock OTG
implementation would resolve the presently missing board-specific mapping.
