# Minimal imported service set; preserve stock PON configuration.
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/EldenStorageDxe/EldenStorageDxe.inf
# Platform-owned entry: EldenStartSd checks the original five dependencies.
# FREEFORM prevents dispatcher discovery/double-loading before SD power setup.
FILE FREEFORM = F10F76DB-42C1-533F-34A8-69BE24653110 {
  SECTION PE32 = Build/EldenDiagnostic/SdccDxe.efi
  SECTION UI = "SdccDxe"
}
# Stock power library locates these three services before hardware setup.
# Do not load PmicDxe wholesale merely to satisfy them: its power-policy
# writes are not yet qualified. UsbPwrCtrl init itself can enable VBUS.
# Keep an explicit FALSE gate even if firmware happens to supply all services.
FILE DRIVER = 11FAED4C-B21F-4D88-8E48-C4C28A1E50DF {
  SECTION DXE_DEPEX_EXP = { FALSE AND gEldenUsbPmicSchgProtocolGuid AND gEldenUsbAdcProtocolGuid AND gEldenUsbPmicVersionProtocolGuid }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.efi
  SECTION UI = "UsbPwrCtrlDxe"
}
# USB isolation build: UsbConfig and UsbPwrCtrl stay gated off, and the
# controller/host drivers below are omitted. No Linux-side USB changes.
# Actual SDT service; no board ACPI tables or fake protocol stubs.
INF MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/TLMMDxe/TLMMDxe.inf
FILE DRIVER = 04DE8591-D2B3-4077-BBBE-B12070094EB6 {
  SECTION DXE_DEPEX = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/I2CDxe/I2CDxe.depex
  SECTION PE32 = Build/EldenDiagnostic/I2CDxe.efi
  SECTION UI = "I2C"
}
# Do not enter UsbConfig during this trial, even when all services are ready.
FILE DRIVER = CD823A4D-7DEC-4531-AE5D-4134FA4127B8 {
  # USB isolation trial: keep the payload but never dispatch its entry point.
  SECTION DXE_DEPEX_EXP = { FALSE }
  SECTION PE32 = Build/EldenDiagnostic/UsbConfigDxe.efi
  SECTION UI = "UsbConfigDxe"
}
# USB isolation trial: omit the USB controller and host stack from the FV.
# Shared Clock/HALIOMMU/I2C/SD/UFS services remain unchanged.
# EldenStorageDxe starts UFS only after software/SID/HWIO/UIC preflight.
FILE FREEFORM = 0D35CD8E-97EA-4F9A-96AF-0F0D89F76567 {
  SECTION PE32 = Build/EldenDiagnostic/UFSDxe.efi
  SECTION UI = "UFSDxe"
}
FILE DRIVER = CB29F4D1-7F37-4692-A416-93E82E219711 {
  SECTION DXE_DEPEX_EXP = { gEldenDalSysProtocolGuid AND gEldenNpaLogProtocolGuid }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/NpaDxe/NpaDxe.efi
  SECTION UI = "NpaDxe"
}
# Incremental PwrUtils trial: stock CmdDb dependency; no NPA/SCMI/UFS.
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/PwrUtilsDxe/PwrUtilsDxe.inf
# Retain stock dependencies plus Env constructor and PwrUtils table service.
FILE DRIVER = 4DB5DEA6-5302-4D1A-8A82-677A683B0D29 {
  SECTION DXE_DEPEX_EXP = { gEldenDalSysProtocolGuid AND gEldenClockChipInfoProtocolGuid AND gEldenClockCmdDbProtocolGuid AND gEldenEnvServicesProtocolGuid AND gEldenPwrUtilsProtocolGuid AND gEldenNpaProtocolGuid }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/ClockDxe/ClockDxe.efi
  SECTION UI = "ClockDxe"
}
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/EldenGop/EldenGop.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/EldenKeys/EldenKeys.inf
# SPMI protocol only: no ButtonsDxe or PmicDxe/PON configuration.
FILE DRIVER = 2A7B4BEF-80CD-49E1-B473-374BA4D673FC {
  SECTION DXE_DEPEX_EXP = { gEldenDalSysProtocolGuid }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/SPMIDxe/SPMIDxe.efi
  SECTION UI = "SPMIDxe"
}
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/CmdDbDxe/CmdDbDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/RNGDxe/RngDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/EnvDxe/EnvDxeEnhanced.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/ParserDxe/ParserEnhancedDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/FvUtilsDxe/FvUtilsEnhancedDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/SmemDxe/SmemDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/DALSYSDxe/DALSYSDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/HWIODxe/HWIODxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/ChipInfoDxe/ChipInfoDxe.inf
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/PlatformInfoDxe/PlatformInfoDxe.inf
# DALModEnv.c:115 in this exact ULog binary requires the DALSys protocol.
# Stock DEPEX is TRUE; encode the missing dependency without patching its PE.
FILE DRIVER = E43128A8-8692-42B6-8AFA-676158578D18 {
  SECTION DXE_DEPEX_EXP = { gEldenDalSysProtocolGuid }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/ULogDxe/ULogDxe.efi
  SECTION UI = "ULogDxe"
}
INF $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/IPCCDxe/IPCCDxe.inf
# Original LA shared-memory allocator required by RPMBListenerAllocMem.
# Imported INF lacks DEPEX; encode normal boot-services-only dispatch explicitly.
FILE DRIVER = 41EBB6A6-2C19-4C02-841D-848CE004A282 {
  SECTION DXE_DEPEX_EXP = { TRUE }
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/ShmBridgeDxe/ShmBridgeDxeLA.efi
  SECTION UI = "ShmBridgeDxeLA"
}
# Stock entry publishes its protocol before parsing IORT. The platform checks
# Env/DALSys, then records the real StartImage result before trusting its table.
FILE FREEFORM = 9A00771F-36D4-4DD5-8916-C48ED9B16B86 {
  SECTION PE32 = $(PACKAGE_NAME)/Device/$(TARGET_DEVICE)/Binaries/QcomPkg/Drivers/HALIOMMUDxe/HALIOMMUDxe.efi
  SECTION UI = "HALIOMMUDxe"
}
