# Elden UFS hardware dependencies — DT audit, 2026-09-13

## Evidence and limits

- Stock: `/home/zhenlin/workspace/qualcomm/lenovo-elden/ZUXOS_1.1.11.263.stock.dtb`, decoded with dtc; phandles below belong ONLY to this DTB.
- Mainline: `/home/zhenlin/workspace/linux/linux-7.1/arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts`, UFS overrides at1097; regulator definitions earlier in that file.
- SoC: same directory `sm8750.dtsi`, PHY/HCI nodes at5329/5352.
- BSP driver/bindings: `/home/zhenlin/workspace/qualcomm/sm8750-kernel-platform/kernel_platform/msm-kernel`, `drivers/phy/qualcomm/phy-qcom-ufs.c`, `drivers/ufs/host/ufs-qcom.c`, `include/dt-bindings/clock/qcom,gcc-sun.h` and `qcom,rpmh.h`.

These are configured requirements, not measurements of current UEFI rail voltage,
clock waveform, reset state or firmware votes. Mainline omission does not prove
an electrically unnecessary rail. No hardware configuration changed for this audit.

## Supply mapping

| Consumer | Stock phandle / resource | DT voltage | Mainline UFS reference |
|---|---|---|---|
| device VCC | 3fa / L17B / ldob17 | 2.504V fixed | vreg_l17b_2p5 |
| device VCCQ + proxy vote | 400 / L1D / ldod1 | 1.200V fixed | vreg_l1d_1p2 |
| PHY VDDA | 417 / L1J / ldoj1 | rail0.880–0.920V; init and PHY minimum0.912V | vreg_l1j_0p91 |
| PHY PLL | 34e / L3G / ldog3 | 1.200–1.256V, init1.200V | vreg_l3g_1p2 |
| device reference-clock supply | 411 / L2I / ldoi2 | 1.200V fixed | not explicitly on UFS node |
| PHY QREF | 412 / L3I / ldoi3 | 0.880–0.912V, init0.880V | not explicitly on UFS PHY node |
| PHY REFGEN | 34f / L1F / ldof1 | 0.880–0.920V, init0.880V | not explicitly on UFS PHY node |
| VCCQ parent vote | 40e / S7I / smpi7 | 1.224–1.340V, init1.256V | regulators-1 vdd-l1-supply points to S7I |

Stock load properties (not measured current): VCC1300000uA, VCCQ1200000uA,
VCCQ proxy1200000uA, parent210000uA, reference-clock100uA;
PHY213000uA, PLL18300uA, QREF64500uA, REFGEN206000uA.
Mainline agrees on VCC/VCCQ load properties. Mainline L17B upstream is BOB2.
BSP phy-qcom-ufs.c parses QREF/REFGEN and enables them in power-on; these are
not merely unused DT strings. Host ufs-qcom.c parses vccq-parent and vddp-ref-clk.
Linux load/vote policy must not be translated directly to arbitrary SPMI writes.

## Clock mapping

Stock HCI /soc/ufshc@1d84000:

| clock-name | Source |
|---|---|
| core_clk | GCC_UFS_PHY_AXI_CLK |
| bus_aggr_clk | GCC_AGGRE_UFS_PHY_AXI_CLK |
| iface_clk | GCC_UFS_PHY_AHB_CLK |
| core_clk_unipro | GCC_UFS_PHY_UNIPRO_CORE_CLK |
| core_clk_ice | GCC_UFS_PHY_ICE_CORE_CLK |
| ref_clk | RPMH_LN_BB_CLK3 |
| tx_lane0_sync_clk | GCC_UFS_PHY_TX_SYMBOL_0_CLK |
| rx_lane0_sync_clk | GCC_UFS_PHY_RX_SYMBOL_0_CLK |
| rx_lane1_sync_clk | GCC_UFS_PHY_RX_SYMBOL_1_CLK |

Stock freq-table: AXI/UniPro/ICE100–403MHz, other entries0/0 (not 'clock off').
Mainline HCI has the same list except ICE, with AXI/UniPro OPP100/403MHz.

PHY stock: ref_clk_src=RPMH_CXO_PAD_CLK (ID27), ref_aux_clk=GCC_UFS_PHY_PHY_AUX_CLK,
qref_clk=TCSR clock provider ID1; RX0/RX1/TX0 mux clocks use GCC symbol SRCs,
and three corresponding PHY symbol clock references use GCC symbol clocks.
Mainline PHY: ref=RPMH_CXO_CLK, ref_aux=GCC_UFS_PHY_PHY_AUX_CLK,
qref=TCSR_UFS_CLKREF_EN. Thus CXO_PAD versus CXO is a real description difference;
do not assume identical firmware clock handles from Linux numeric IDs.

The earlier nine UEFI GCC names cover AXI, ICE, UniPro, aggregate, AHB, AUX,
TX0/RX0/RX1. They do NOT establish RPMh LN_BB_CLK3, CXO_PAD, TCSR qref gating,
symbol mux selection or frequency correctness. State=1 alone is not clock proof.

## Domains, reset and bus topology

- PHY01d80000+2000; HCI01d84000+3000. Stock additionally ICE01d88000+18000,
  MCQ SQD01da5000+2000, MCQ VS01da4000+10.
- Host power: GCC_UFS_PHY_GDSC, stock phandle61 at177004, CX parent.
- PHY power: GCC_UFS_MEM_PHY_GDSC, stock phandle60 at19e000, MX parent.
  Mainline expresses these as power-domains rather than regulator supplies.
- Device RESET_N: TLMM GPIO215, active low, both trees agree.
- Host reset: GCC_UFS_PHY_BCR (stock GCC reset ID23); PHY reset is supplied by HCI reset provider, index0.
- Two lanes per direction. IRQ SPI265 level high. SMMU stream ID0x60 agrees
  with current UEFI SID mapping; agreement does not prove a working DMA domain.
- Interconnect paths: ufs-ddr and cpu-ufs. Mainline includes RPMh OPP votes.
- Stock sets qcom,bypass-pbl-rst-wa; preserve as a difference to trace in BSP
  reset logic, not authorization to toggle/reset hardware blindly.

## Implication for bring-up

9395 local GET success proves only that those local accesses completed. It does
not validate external reference clocks, PMIC votes, PHY link health or RESET_N.
Next audit should map UEFI rail/resource consumers to all eight supply entries,
verify reference-clock ownership and reset state, alongside pending UIC error
evidence. Do not claim a missing supply caused resultA until firmware state is
checked. Do not blindly re-enable shared rails or reset a live inherited link.
This audit produces no new flash image and does not supersede uic-evidence-v1.

## 9396 runtime follow-up: topology-read v1

Photo: AHIT=0, IE=0, IS400, PA/DL/N/T/DME error registers0 before/after failed
peer requests. No evidence of enabled automatic hibernation timer or captured
layer error. Zero latches do not establish a healthy link or rule out all faults.

New package elden-aloha-topology-read-v1 adds BEFORE/AFTER reads only:
TCSR_UFS_CLKREF_EN=f204008+1000=f205008 (enable bit0),
dedicated UFS_RESET config=f1e2004 and output=f1e3000 (output bit0).
Mainline TLMM basef100000 + e2004/e3000 matches BSP basef000000 +1e2004/1e3000.
Do NOT use GPIO215 *1000 for this dedicated pad. Registers are inside existing
TLMM NS_DEVICE mapping f000000..fffffff. BEGIN printed before each access.
Output latch is not external pad voltage; gate enable is not a waveform check.
No rail writes, reset changes or AUTO retry. PMIC rail/RPMh votes remain unaudited
at runtime; do not infer them from these three registers. Hardware pending.

## 9397 and runtime rail configuration

Photo AFTER: TCSR clkref1, reset config8, reset output1. BEFORE topology group
is cropped; do not claim both phases matched. These are register values only.
XBL extracted fdt-000080dc.dts /soc/ufs0@1D80000/volt_regs contains six rails:
vcc=ldob17(2504mV), vccq=ldod1(1200), vdda=ldog3(1200),
vdda_core=ldoj1(912), qref=ldoi3(880), vdda_csi=ldof1(880).
All *_on entries en01/mode07. This extracted firmware version is not proof of
the active Env content. elden-aloha-rail-config-v1 reads all six active names
and *_on mVolt/en/mode properties before UFS starts, using the already audited
Env Find/Size/Read ABI. Raw bytes logged, lengths validated, no PMIC access.
RAIL-CONFIG END6/6 means property reads completed, NOT actual voltage or enable.
Physical PMIC address mapping remains unverified; do not guess from rail names.
