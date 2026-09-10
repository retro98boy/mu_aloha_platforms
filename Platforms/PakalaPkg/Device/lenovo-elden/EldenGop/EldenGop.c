// SPDX-License-Identifier: BSD-2-Clause-Patent
// Elden stock live framebuffer; no panel, clock, GPIO or PON configuration.
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/CacheMaintenanceLib.h>

#define FB_BASE 0xFC800000ULL
#define FB_WIDTH 1904U
#define FB_HEIGHT 3040U
#define FB_BYTES ((UINTN)FB_WIDTH * FB_HEIGHT * 4)
STATIC FRAME_BUFFER_CONFIGURE *mConfig;
STATIC struct {
  VENDOR_DEVICE_PATH Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} mDisplayPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof(VENDOR_DEVICE_PATH), 0}},
   {0x87503222,0x9d17,0x4503,{0xab,0x62,0xdf,0xc7,0x83,0x60,0x43,0x01}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {sizeof(EFI_DEVICE_PATH_PROTOCOL), 0}}
};
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo = {
  0, FB_WIDTH, FB_HEIGHT, PixelBlueGreenRedReserved8BitPerColor,
  {0, 0, 0, 0}, FB_WIDTH
};
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode = {
  1, 0, &mInfo, sizeof(mInfo), FB_BASE, FB_BYTES
};

STATIC EFI_STATUS EFIAPI Query(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  UINT32 Mode, UINTN *Size, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  if (This == NULL || Size == NULL || Info == NULL) return EFI_INVALID_PARAMETER;
  *Info = NULL;
  *Size = 0;
  if (Mode != 0) return EFI_INVALID_PARAMETER;
  *Info = AllocateCopyPool(sizeof(mInfo), &mInfo);
  if (*Info == NULL) return EFI_OUT_OF_RESOURCES;
  *Size = sizeof(mInfo);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI Set(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode)
{
  if (This == NULL) return EFI_INVALID_PARAMETER;
  if (Mode != 0) return EFI_UNSUPPORTED;
  // GOP SetMode clears the visible surface; initialization never calls this.
  ZeroMem((VOID *)(UINTN)FB_BASE, FB_BYTES);
  WriteBackDataCacheRange((VOID *)(UINTN)FB_BASE, FB_BYTES);
  return EFI_SUCCESS;
}

STATIC BOOLEAN Rect(UINTN X, UINTN Y, UINTN W, UINTN H)
{
  return X < FB_WIDTH && Y < FB_HEIGHT && W <= FB_WIDTH-X && H <= FB_HEIGHT-Y;
}

STATIC EFI_STATUS EFIAPI Blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer, EFI_GRAPHICS_OUTPUT_BLT_OPERATION Op,
  UINTN SX, UINTN SY, UINTN DX, UINTN DY, UINTN W, UINTN H, UINTN Delta)
{
  RETURN_STATUS Status;
  EFI_TPL Tpl;
  UINTN X, Y, Row;
  if (This == NULL || mConfig == NULL || W == 0 || H == 0 ||
      (UINT32)Op >= EfiGraphicsOutputBltOperationMax) return EFI_INVALID_PARAMETER;
  if (Op != EfiBltVideoToVideo && Buffer == NULL) return EFI_INVALID_PARAMETER;
  if ((Op == EfiBltVideoToBltBuffer || Op == EfiBltVideoToVideo) && !Rect(SX,SY,W,H))
    return EFI_INVALID_PARAMETER;
  if (Op != EfiBltVideoToBltBuffer && !Rect(DX,DY,W,H)) return EFI_INVALID_PARAMETER;
  if (Op == EfiBltVideoToBltBuffer || Op == EfiBltBufferToVideo) {
    X = Op == EfiBltVideoToBltBuffer ? DX : SX;
    Y = Op == EfiBltVideoToBltBuffer ? DY : SY;
    if (W > MAX_UINTN/4 || X > MAX_UINTN/4-W) return EFI_INVALID_PARAMETER;
    Row = Delta == 0 ? W*4 : Delta;
    if (Row % 4 != 0 || Row < (X+W)*4 || Y > MAX_UINTN-H ||
        Y+H > MAX_UINTN/Row) return EFI_INVALID_PARAMETER;
  }
  Tpl = gBS->RaiseTPL(TPL_NOTIFY);
  Status = FrameBufferBlt(mConfig, Buffer, Op, SX, SY, DX, DY, W, H, Delta);
  if (!RETURN_ERROR(Status) && Op != EfiBltVideoToBltBuffer)
    WriteBackDataCacheRange((VOID *)(UINTN)FB_BASE, FB_BYTES);
  gBS->RestoreTPL(Tpl);
  return (EFI_STATUS)Status;
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop = {Query, Set, Blt, &mMode};

EFI_STATUS EFIAPI EldenGopInitialize(EFI_HANDLE Image, EFI_SYSTEM_TABLE *System)
{
  EFI_STATUS Status;
  EFI_HANDLE Handle = NULL;
  VOID *Existing;
  UINTN Size = 0;
  Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, &Existing);
  if (!EFI_ERROR(Status)) return EFI_ALREADY_STARTED;
  Status = FrameBufferBltConfigure((VOID *)(UINTN)FB_BASE, &mInfo, NULL, &Size);
  if (Status != EFI_BUFFER_TOO_SMALL) return EFI_DEVICE_ERROR;
  mConfig = AllocatePool(Size);
  if (mConfig == NULL) return EFI_OUT_OF_RESOURCES;
  Status = FrameBufferBltConfigure((VOID *)(UINTN)FB_BASE, &mInfo, mConfig, &Size);
  if (!EFI_ERROR(Status)) {
    Status = gBS->InstallMultipleProtocolInterfaces(&Handle,
      &gEfiDevicePathProtocolGuid, &mDisplayPath,
      &gEfiGraphicsOutputProtocolGuid, &mGop, NULL);
  }
  if (EFI_ERROR(Status)) { FreePool(mConfig); mConfig = NULL; }
  return Status;
}
