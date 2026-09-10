// SPDX-License-Identifier: BSD-2-Clause-Patent
// Stock integer configuration from xbl_config FDT /soc/uefi/int_param.
// Hardware CPU counts stay unchanged. These flags alone do not park cores.
#include <Library/PlatformConfigurationMapLib.h>

static CONFIGURATION_DESCRIPTOR_EX mConfiguration[] = {
  { "MaxCount", 0x28 },
  { "StrMaxCount", 0xA },
  { "EnableShell", 0x1 },
  { "SecPagePoolCount", 0x800 },
  { "SharedIMEMBaseAddr", 0x14680000 },
  { "DloadCookieAddr", 0x1FD9000 },
  { "DloadCookieValue", 0x10 },
  // V1 override: stock 0x0; avoid unverified threading/flash writes/imagefv dependencies.
  { "SkipDBISetup", 0x1 },
  { "PilSubsysDbgCookieAddr", 0x146806DC },
  { "PilSubsysDbgCookieVal", 0x53444247 },
  { "NumCpus", 0x8 },
  { "NumActiveCores", 0x8 },
  { "MaxLogFileSize", 0x400000 },
  { "UefiMemUseThreshold", 0xE1 },
  { "USBHS1_Config", 0x0 },
  { "UsbFnIoRevNum", 0x10001 },
  { "PwrBtnShutdownFlag", 0x0 },
  { "Sdc1GpioConfigOn", 0x1E92 },
  { "Sdc2GpioConfigOn", 0x1E92 },
  { "Sdc1GpioConfigOff", 0xA00 },
  { "Sdc2GpioConfigOff", 0xA00 },
  { "EnableSDHCSwitch", 0x1 },
  { "EnableUfsIOC", 0x1 },
  { "UfsSmmuConfigForOtherBootDev", 0x1 },
  { "SecurityFlag", 0xC4 },
  // V1 override: stock 0x1; avoid unverified threading/flash writes/imagefv dependencies.
  { "EnableLogFsSyncInRetail", 0x0 },
  { "DetectRetailUserAttentionHotkey", 0x0 },
  { "DetectRetailUserAttentionHotkeyCode", 0x17 },
  { "ShmBridgememSize", 0xA00000 },
  { "MaxCoreCount", 0x8 },
  // V1 override: stock 0x1; avoid unverified threading/flash writes/imagefv dependencies.
  { "EnableMultiThreading", 0x0 },
  // V1 override: stock 0x2; avoid unverified threading/flash writes/imagefv dependencies.
  { "EarlyInitCoreCnt", 0x1 },
  { "IsFunctionalCoreCountToOverrideFlag", 0x0 },
  { "EnableUefiSecAppDebugLogDump", 0x0 },
  { "AllowNonPersistentVarsInRetail", 0x1 },
  // V1 override: stock 0x1; avoid unverified threading/flash writes/imagefv dependencies.
  { "EnableDisplayThread", 0x0 },
  // V1 override: stock 0x1; avoid unverified threading/flash writes/imagefv dependencies.
  { "EnableDisplayImageFv", 0x0 },
  { "DDRInfoNotifyFlag", 0x0 },
  // V1 override: stock 0x1; avoid unverified threading/flash writes/imagefv dependencies.
  { "EnableMultiCoreFvDecompression", 0x0 },
  { "EnableVariablePolicyEngine", 0x0 },
  { "EnableACPIFallback", 0x0 },
  { "DRAM_CLK_PERIOD_ADDR", 0x240BA050 },
  { "Terminator", 0xFFFFFFFF }
};

CONFIGURATION_DESCRIPTOR_EX *GetPlatformConfigurationMap (VOID)
{
  return mConfiguration;
}
