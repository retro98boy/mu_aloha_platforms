# Elden USB host bring-up audit

## Ground truth and limits

Sources inspected: linux-7.1/arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts
(connectors, i2c5, USB overrides), sm8750.dtsi (SoC controller/PHY), and
ZUXOS_1.1.11.263.stock.dtb decompiled with dtc. These describe wiring and software
configuration, not electrical measurements or successful UEFI host operation.
No USB MMIO, PMIC write, VBUS enable, or role switch has been executed by this audit.

## Two connectors, shared controller

| Connector | Data topology in mainline DT | Implication |
|---|---|---|
| connector@0, long edge | DWC3 HS + QMP USB3/DP + SBU mux | USB3/DP-capable path |
| connector@1, short edge | same DWC3 HS, separate connector endpoint/SBU mux | USB2 path; no SS/DP endpoint declared |

Both are data-role/power-role dual. Controller maximum-speed is
super-speed-plus-gen2x1. This is a configured capability, not measured throughput.
The board DTS explicitly warns USB2 is shared and removing either cable resets
DWC3, disconnecting both ports. Do not model connector@1 as a second xHCI.
First host trial should use only one connector, preferably long edge, a USB2
keyboard or storage device, and no simultaneous PC cable on the other port.

## Hardware resources

* DWC3: 0x0a600000. Mainline span 0xfc100; BSP wrapper span 0x100000 and child
  span 0xd93c. Do not mix wrapper and child register layouts blindly.
* M31 eUSB2 PHY: 0x088e3000, span 0x29c. Ref TCSR_USB2_CLKREF_EN,
  reset GCC_QUSB2PHY_PRIM_BCR; supplies L2D 0.88V and L3G 1.2V.
* QMP USB3/DP PHY: 0x088e8000, mainline span 0x4000, BSP span 0x3000.
  clocks GCC_USB3_PRIM_PHY_AUX_CLK, TCSR_USB3_CLKREF_EN,
  GCC_USB3_PRIM_PHY_COM_AUX_CLK, GCC_USB3_PRIM_PHY_PIPE_CLK;
  resets GCC_USB3_PHY_PRIM_BCR and GCC_USB3_DP_PHY_PRIM_BCR;
  power domain GCC_USB3_PHY_GDSC; supplies L3G 1.2V/L2D 0.88V.
* DWC3 clocks: CFG_NOC_USB3_PRIM_AXI, USB30_PRIM_MASTER,
  AGGRE_USB3_PRIM_AXI, USB30_PRIM_SLEEP, USB30_PRIM_MOCK_UTMI.
  Mainline requests master 200MHz, mock UTMI 19.2MHz. Domain USB30_PRIM_GDSC,
  reset USB30_PRIM_BCR. SMMU SID 0x40, distinct from UFS; do not reuse its mapping.
* External repeater: i2c5 (BSP bus @a94000), address 0x4f, PTN3222DUK in mainline;
  L5B 3.1V and L15B 1.8V supplies, PM8550VE GPIO6 active-low reset.
  TLMM GPIO29 output-high selects/enables the eUSB mux default path.
* FSA4480 @0x42: short-edge HS/SBU; @0x43: long-edge SBU, supplied by BOB1.
  They are not interchangeable with the eUSB repeater or the USB3 lane switch.
* GLINK connector orientation GPIO: TLMM61. VBUS sourcing and role negotiation
  require separate verification; a dual-role declaration is not an enable sequence.

BSP confirms the external @4f repeater, reset GPIO6, and c1/c2 override sequences
40/06,22/07,65/08,03/0a. Mainline also supplies the PMIH0108 internal repeater;
the board HS PHY reference explicitly selects the external repeater. Do not
assume both are serial stages merely because both nodes exist.

## Imported firmware stack

Available original modules include UsbConfigDxe, UsbInitDxe, UsbPwrCtrlDxe,
XhciPciEmulationDxe, XhciDxe, UsbBusDxe, UsbKbDxe, UsbMassStorageDxe, plus device
mode modules. UsbfnDwc3Dxe/UsbDeviceDxe/UsbMsdDxe must not be confused with host
mass storage. Other Aloha device recipes include multiple modules for both modes;
copying all of them does not prove the Elden host prerequisites are satisfied.

Decoded original dependencies:

* UsbConfigDxe: b0760469-970c-487a-a4b5-28db7b45cef1,
  1c34f691-d33d-4e14-b8f8-32c6c029e95b, SMEM
  f4e5c7d0-d239-47cb-aacd-7f66ef763238, PlatformInfo
  157a5c45-21b2-43c5-ba7c-822fee5fe599, ACPI SDT
  eb97088e-cfdf-49c6-be4b-d906a5b20e86 (AND).
* UsbInitDxe: e722b03f-b250-42ce-8ebd-5bd51812d037.
* UsbPwrCtrlDxe: TRUE DEPEX, which does NOT mean no runtime dependencies.
  Strings name PmicSchgProtocol, AdcProtocol, PmicVersionProtocol, VBUS enable,
  source-ready and Type-C status. Strings are leads, not proof of control flow.
* XhciPciEmulationDxe: multiple driver-binding and architectural dependencies.

## Implementation gates

1. Reverse UsbConfig/UsbPwrCtrl entry and protocol methods; determine which
   services are essential for host-only operation and whether board repeater/mux
   setup is encoded in the imported XBL environment. Do not remove DEPEX blindly.
2. Audit USB MMIO map and DMA/SMMU ownership before controller initialization.
3. Establish a bounded host-role/VBUS path for one connector, then attach xHCI,
   bus and keyboard/storage class drivers. Preserve UFS and volume-key reset.
4. Enumerate keyboard/USB storage, then read FAT from removable storage. Do not
   format, write media, launch EFI applications or enter unrestricted ConnectAll.

Current result is a source/binary-dependency audit, not a USB-enabled test image.

## Host integration checkpoint (2026-09-13)

Elden DSC/FDF now include the original UsbConfigDxe, UsbInitDxe,
XhciPciEmulationDxe, XhciDxe, UsbBusDxe and UsbMassStorageDxe. The existing
UsbKbHidDxe/HidKeyboardDxe pair supplies the keyboard class path. Binary DEPEX
is retained; UsbPwrCtrlDxe is not force-dispatched and no synthetic dependency
protocols are installed. Pre-BDS now reports AcpiSdt, UsbConfig, Usb2Hc and UsbIo
handle counts. This is integration scaffolding, NOT working USB host support.

Outstanding: AcpiSdt service is absent from the current FDF; stock UsbConfig
also references USB power-control services and creates/attaches its own IOMMU
domain. Resolve their actual call paths before recommending a flash test.
No role-forcing command, VBUS enable, media write, or ConnectAll was added.
The prior UFS patch generator and its DMA/descriptor fixes remain unchanged.

## Dependency repair and entry audit (2026-09-13, next checkpoint)

Supersedes the missing-SDT item above: Elden now includes source AcpiTableDxe;
Pakala already sets PcdInstallAcpiSdtProtocol=TRUE. This supplies genuine table
management services, not board ACPI tables or an OS-ready ACPI description.

UsbConfig SHA256: 11d81930099a11fe7de8fddb8c2548a70acb2d3fb2c43b0ff94390ed3b8a4325.
Entry 0x1df8 calls initialization at 0x3d64. At 0x3fc0 it locates
54b6d3b4-5d33-4f91-8600-6c41d5deb19a (HALIOMMU); 0x3fd4 dereferences the returned
pointer without checking LocateProtocol status. FDF preserves all five stock
dependencies and additionally requires HALIOMMU. test-elden-usb-depex.py pins
the image/instructions and compares the new dependency set to the stock set.

The entry proceeds to create/configure a domain and calls attach at 0x4110,
with name USB0 and argument 0x03000000. This is a firmware API argument, NOT
the DT Stream ID 0x40; do not replace it by 0x40 or copy the UFS bypass.
Its mapping semantics remain under investigation.

USB power protocol is e07df17e-e79e-4150-9378-50623a14994a (GUID at 0x131e8).
The path at 0x3644 checks LocateProtocol status before using method +0x20;
missing-power errors here are unlike the unchecked HALIOMMU lookup. No
UsbPwrCtrl/PmicDxe force-enable has been added. Full build and dependency
regression pass; this checkpoint still is NOT recommended for flashing.

## SM8650 comparison and power integration

Reference fetched without switching the worktree: GUF296/mu_aloha_platforms,
8650-test commit 4234f9221de6acfadea950a377f521aab740a2bf.
Platforms/LanaiPkg/Device/qcom-8650/DXE.inc includes patched PmicDxeLa and
UsbConfigDxe plus PmicGlinkDxe, UsbPwrCtrlDxe and the host/device class stack.
However PlatformBuildNoSb.py selects Silicon/QC/SM8650, a directory absent
from that commit. Thus this is a module-wiring reference, not reproducible
evidence for the exact USB/PMIC patches or working Elden support.

Elden now includes its own unmodified UsbPwrCtrlDxe with additional DEPEX:
PmicSchg ae6ae96e-483f-42ae-9cc1-9fac1b584728,
ADC 61740cc4-916f-4471-b0dc-00c6f43a7fc6,
PmicVersion 4684800a-2755-4edc-b443-7f8ceb3239d3.
This supersedes the previous exclusion of UsbPwrCtrl itself, not the exclusion
of PmicDxe/PON, QcomChargerDxe or watchdog drivers. Initialization at 0x3f60
looks up these three services before querying PMIC details. The current image
does not provide all these services, so this is NOT a functional host release.

Power PE SHA256 f168e7fcde1e5c2dc6a25428167c16b91d21d64ad9a3662fe1b15016951c2ef0.
Static regression pins this PE and its GUIDs; output validation evaluates
the built power DEPEX for all eight combinations (only all-present passes).
USB preflight distinguishes missing power providers from missing host/bus
protocols. No VBUS or role-forcing command was introduced.
