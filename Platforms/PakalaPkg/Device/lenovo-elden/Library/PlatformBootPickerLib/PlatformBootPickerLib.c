// SPDX-License-Identifier: BSD-2-Clause-Patent
// Build a transient option for the local picker; never register it in BootOrder.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformBootPickerLib.h>

#define ELDEN_BOOT_MENU_FILE_GUID \
  { 0x87503222, 0x9d17, 0x4503, { 0xab, 0x62, 0xdf, 0xc7, 0x83, 0x60, 0x43, 0x08 } }

EFI_STATUS
EFIAPI
PlatformBootPickerGetBootOption(EFI_BOOT_MANAGER_LOAD_OPTION *Option)
{
  EFI_GUID Menu = ELDEN_BOOT_MENU_FILE_GUID;
  EFI_LOADED_IMAGE_PROTOCOL *Image;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH File;
  EFI_DEVICE_PATH_PROTOCOL *Parent, *Path;
  EFI_STATUS Status;

  Status = gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Image);
  if (EFI_ERROR(Status)) return Status;
  if (Image == NULL) return EFI_NOT_FOUND;
  Parent = DevicePathFromHandle(Image->DeviceHandle);
  if (Parent == NULL) return EFI_NOT_FOUND;
  EfiInitializeFwVolDevicepathNode(&File, &Menu);
  Path = AppendDevicePathNode(Parent, (EFI_DEVICE_PATH_PROTOCOL *)&File);
  if (Path == NULL) return EFI_OUT_OF_RESOURCES;
  // Describe the picker as an application, not an automatic OS boot entry.
  // Standard BDS still owns the launch lifecycle and post-return policy.
  Status = EfiBootManagerInitializeLoadOption(Option, LoadOptionNumberUnassigned,
             LoadOptionTypeBoot, LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP,
             L"Lenovo Y700 Gen4 local EFI picker", Path, NULL, 0);
  FreePool(Path);
  return Status;
}
