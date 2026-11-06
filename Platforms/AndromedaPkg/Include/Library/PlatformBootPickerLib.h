/** @file
  Optional downstream boot picker.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef PLATFORM_BOOT_PICKER_LIB_H_
#define PLATFORM_BOOT_PICKER_LIB_H_

#include <Uefi.h>
#include <Library/UefiBootManagerLib.h>

/**
  Build a transient picker option when no standard priority boot was requested.

  @retval EFI_SUCCESS    BootOption contains an initialized load option.
  @retval EFI_NOT_FOUND  No picker is configured; continue normal boot.
  @retval Other         The picker option could not be constructed.
**/
EFI_STATUS
EFIAPI
PlatformBootPickerGetBootOption (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  );

#endif
