/** @file
  Elden storage initialization, scheduled by the DXE dispatcher.
  Owns vendor storage payload startup and board-specific SD power setup.

  Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <PiDxe.h>
#include <Protocol/DevicePath.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/Security.h>
#include <Protocol/Cpu.h>
#include <Protocol/Metronome.h>
#include <Protocol/Timer.h>
#include <Protocol/Bds.h>
#include <Protocol/WatchdogTimer.h>
#include <Protocol/Runtime.h>
#include <Protocol/Variable.h>
#include <Protocol/VariableWrite.h>
#include <Protocol/Capsule.h>
#include <Protocol/MonotonicCounter.h>
#include <Protocol/Reset.h>
#include <Protocol/RealTimeClock.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
extern EFI_GUID gEldenUfsReadyProtocolGuid;


STATIC BOOLEAN mEldenIommuReturned = FALSE;
STATIC BOOLEAN mEldenUfsReturned = FALSE;
STATIC BOOLEAN mEldenSdReturned = FALSE;
STATIC EFI_STATUS mEldenIommuStartStatus = EFI_NOT_READY;
STATIC EFI_STATUS mEldenUfsStartStatus = EFI_NOT_READY;
STATIC EFI_STATUS mEldenSdStartStatus = EFI_NOT_READY;
STATIC UINTN mEldenSdImageBase = 0;
STATIC UINTN mEldenSdImageSize = 0;
STATIC EFI_GUID mEldenIommuFile = {0x9a00771f,0x36d4,0x4dd5,{0x89,0x16,0xc4,0x8e,0xd9,0xb1,0x6b,0x86}};
STATIC EFI_GUID mEldenUfsFile = {0x0d35cd8e,0x97ea,0x4f9a,{0x96,0xaf,0x0f,0x0d,0x89,0xf7,0x65,0x67}};
STATIC EFI_GUID mEldenSdFile = {0xf10f76db,0x42c1,0x533f,{0x34,0xa8,0x69,0xbe,0x24,0x65,0x31,0x10}};
STATIC EFI_GUID mEnv = {0x0389b776,0x625f,0x11eb,{0x83,0xbe,0xc7,0x41,0xa9,0x13,0xde,0x34}};
STATIC EFI_GUID mDalSys = {0xae37b942,0x457f,0x4c91,{0xa1,0x96,0xd9,0x66,0x9f,0xd3,0x47,0xa3}};
STATIC EFI_GUID mClock = {0x241afae6,0x885f,0x4f6c,{0xa7,0xea,0xc2,0x8e,0xab,0x79,0xc3,0xe5}};
STATIC EFI_GUID mIommu = {0x54b6d3b4,0x5d33,0x4f91,{0x86,0x00,0x6c,0x41,0xd5,0xde,0xb1,0x9a}};
STATIC EFI_GUID mNpa = {0x79d6c870,0x725e,0x489e,{0xa0,0xa1,0x27,0xe7,0xa5,0xd0,0xcb,0x35}};

STATIC EFI_STATUS
EldenRequireProtocols (EFI_GUID **Guids, UINTN Count)
{
  UINTN I;
  VOID *Interface;
  EFI_STATUS Status;
  for (I = 0; I < Count; I++) {
    Status = gBS->LocateProtocol(Guids[I], NULL, &Interface);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }
  return EFI_SUCCESS;
}

// FREEFORM FV files cannot be dispatched automatically. Use the normal FV
// device path LoadImage path (including image authentication), then StartImage.
// Never retry a selected image after load/start failure: entry may have changed
// hardware even if it returns an error. Only retain image bounds after success.
STATIC EFI_STATUS
EldenStartFvImage (EFI_GUID *File, BOOLEAN *Returned, EFI_STATUS *StartStatus,
                  UINTN *ImageBase, UINTN *ImageSize)
{
  EFI_HANDLE *Handles = NULL, Image = NULL;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;
  EFI_DEVICE_PATH_PROTOCOL *Path, *FvPath;
  UINTN LoadedBase;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH Node;
  EFI_FV_FILETYPE Type;
  EFI_FV_FILE_ATTRIBUTES Attributes;
  UINT32 Authentication;
  UINTN Count = 0, I, Size;
  EFI_STATUS Status;

  *Returned = FALSE;
  *StartStatus = EFI_NOT_READY;
  if (ImageBase != NULL) *ImageBase = 0;
  if (ImageSize != NULL) *ImageSize = 0;
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiFirmwareVolume2ProtocolGuid,
                                  NULL, &Count, &Handles);
  if (EFI_ERROR(Status)) return Status;
  Status = EFI_NOT_FOUND;
  for (I = 0; I < Count; I++) {
    Status = gBS->HandleProtocol(Handles[I], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv);
    if (EFI_ERROR(Status)) continue;
    Size = 0;
    Status = Fv->ReadFile(Fv, File, NULL, &Size, &Type, &Attributes, &Authentication);
    if (Status == EFI_NOT_FOUND) continue;
    if (EFI_ERROR(Status)) break;
    if (Type != EFI_FV_FILETYPE_FREEFORM) { Status = EFI_UNSUPPORTED; break; }
    FvPath = DevicePathFromHandle(Handles[I]);
    if (FvPath == NULL) { Status = EFI_NOT_FOUND; break; }
    EfiInitializeFwVolDevicepathNode(&Node, File);
    Path = AppendDevicePathNode(FvPath, (EFI_DEVICE_PATH_PROTOCOL *)&Node);
    if (Path == NULL) { Status = EFI_OUT_OF_RESOURCES; break; }
    Status = gBS->LoadImage(FALSE, gImageHandle, Path, NULL, 0, &Image);
    FreePool(Path);
    if (EFI_ERROR(Status)) {
      // EFI_SECURITY_VIOLATION can still return an allocated image handle.
      if (Image != NULL) gBS->UnloadImage(Image);
      break;
    }
    Status = gBS->HandleProtocol(Image, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
    if (EFI_ERROR(Status)) { gBS->UnloadImage(Image); break; }
    // StartImage failure may unload the image; never dereference Loaded then.
    Size = (UINTN)Loaded->ImageSize;
    LoadedBase = (UINTN)Loaded->ImageBase;
    *StartStatus = gBS->StartImage(Image, NULL, NULL);
    *Returned = TRUE;
    Status = *StartStatus;
    if (!EFI_ERROR(Status)) {
      if (ImageBase != NULL) *ImageBase = LoadedBase;
      if (ImageSize != NULL) *ImageSize = Size;
    }
    break;
  }
  FreePool(Handles);
  return Status;
}

STATIC EFI_STATUS
EldenStartSd (VOID)
{
  EFI_GUID *Dependencies[] = {&mClock, &gEfiTimerArchProtocolGuid, &mEnv, &mIommu, &mNpa};
  EFI_STATUS Status;
  // HALIOMMU publishes its protocol before it can fail parsing IORT.
  if (!mEldenIommuReturned || EFI_ERROR(mEldenIommuStartStatus)) return EFI_NOT_READY;
  Status = EldenRequireProtocols(Dependencies, ARRAY_SIZE(Dependencies));
  if (EFI_ERROR(Status)) return Status;
  return EldenStartFvImage(&mEldenSdFile, &mEldenSdReturned, &mEldenSdStartStatus,
                           &mEldenSdImageBase, &mEldenSdImageSize);
}

#include <Library/CacheMaintenanceLib.h>
STATIC BOOLEAN mEldenSoftwareReady = FALSE;
STATIC BOOLEAN mEldenSidReady = FALSE;
STATIC BOOLEAN mEldenHwioReady = FALSE;
STATIC BOOLEAN mEldenSdPowerCompleted = FALSE;

// Qualified HLOS DRV2 active TCS1 transaction. This migration adds no RPMh
// driver or AP startup. Runs synchronously during DXE before boot selection;
// the old "BDS remains stopped" diagnostic comment no longer described boot.
// TCS0's IRQ configuration is never changed. Ownership remains specific to
// this bring-up image; adding RpmhDxe/APs needs a separate ownership design.
STATIC EFI_STATUS
EldenSdVrmVote (UINT32 Address, UINT32 Value, UINT32 *Completion)
{
  CONST UINTN Rsc = 0x16520d00;
  CONST UINTN Tcs = 0x16520fa0; // TCS1: 0xd00 + 0x2a0
  UINT32 Saved[3], I;
  EFI_TPL OldTpl;
  EFI_STATUS Status = EFI_NOT_READY;
  BOOLEAN Allowed = FALSE;
  BOOLEAN SawIncomplete;

  *Completion = 0;
  // Exact CmdDb addresses qualified by photo 9516, not SPMI addresses.
  switch (Address) {
    case 0x41700: case 0x41600: Allowed = Value == 2960; break;
    case 0x42200: Allowed = Value == 1200; break;
    case 0x41704: case 0x41604: case 0x42204: Allowed = Value == 1; break;
    case 0x41708: case 0x41608: case 0x42208: Allowed = Value == 7; break;
    default: break;
  }
  if (!Allowed) return EFI_ACCESS_DENIED;
  OldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  // Fail closed on any difference from the qualified idle environment.
  // In particular, do not steal an IRQ-enabled TCS or clear others' IRQs.
  if (*(volatile UINT32 *)Rsc != 1 || *(volatile UINT32 *)(Rsc + 4) != 0) goto Done;
  for (I = 0; I < 3; I++) {
    UINTN T = Rsc + I * 0x2a0;
    if (*(volatile UINT32 *)(T + 0x24) != 0 ||
        *(volatile UINT32 *)(T + 0x28) != 1 ||
        *(volatile UINT32 *)(T + 0x2c) != 0) goto Done;
  }
  Saved[0] = *(volatile UINT32 *)(Tcs + 0x34);
  Saved[1] = *(volatile UINT32 *)(Tcs + 0x38);
  Saved[2] = *(volatile UINT32 *)(Tcs + 0x3c);
  // RPMH v3: length=8, write, response required. One command per transfer.
  *(volatile UINT32 *)(Tcs + 0x34) = 0x10108;
  *(volatile UINT32 *)(Tcs + 0x38) = Address;
  *(volatile UINT32 *)(Tcs + 0x3c) = Value;
  MemoryFence();
  *(volatile UINT32 *)(Tcs + 0x2c) = 1;
  *(volatile UINT32 *)(Tcs + 0x24) = 0x10000;
  MemoryFence();
  if (*(volatile UINT32 *)(Tcs + 0x24) != 0x10000) {
    Status = EFI_DEVICE_ERROR;
    goto Done; // Stop the trial; do not guess at recovery state.
  }
  // A completion already present before trigger is not evidence for this
  // request. Require an observed incomplete sample before accepting done.
  SawIncomplete = ((*(volatile UINT32 *)(Tcs + 0x40) & BIT16) == 0);
  *(volatile UINT32 *)(Tcs + 0x24) = 0x1010000;
  MemoryFence();
  Status = EFI_TIMEOUT;
  // Photo 9519: command completed, but masked IRQ status stayed zero.
  // Poll command completion and TCS idle, not the masked IRQ status.
  // No Stall/event dispatch at HIGH_LEVEL. This is an iteration bound,
  // deliberately not described as a calibrated millisecond timeout.
  for (I = 0; I < 1000000; I++) {
    *Completion = *(volatile UINT32 *)(Tcs + 0x40);
    if ((*Completion & BIT16) == 0) SawIncomplete = TRUE;
    if (SawIncomplete && ((*Completion & 0x10100) == 0x10100) &&
        (*(volatile UINT32 *)(Tcs + 0x28) == 1)) {
      Status = EFI_SUCCESS;
      break;
    }
    CpuPause();
  }
  if (Status == EFI_TIMEOUT) {
    // Report actual status; stale complete or non-idle TCS still times out.
    // This sample is NOT accepted as fresh completion: it could be stale.
    *Completion = *(volatile UINT32 *)(Tcs + 0x40);
    goto Done; // Never reuse an in-flight TCS.
  }
  // Clear trigger, then mode; only reclaim our completed TCS1.
  *(volatile UINT32 *)(Tcs + 0x24) = 0x10000;
  MemoryFence();
  if (*(volatile UINT32 *)(Tcs + 0x24) != 0x10000) { Status = EFI_DEVICE_ERROR; goto Done; }
  *(volatile UINT32 *)(Tcs + 0x24) = 0;
  MemoryFence();
  if (*(volatile UINT32 *)(Tcs + 0x24) != 0) { Status = EFI_DEVICE_ERROR; goto Done; }
  *(volatile UINT32 *)(Tcs + 0x2c) = 0;
  *(volatile UINT32 *)(Rsc + 8) = BIT1;
  *(volatile UINT32 *)(Tcs + 0x34) = Saved[0];
  *(volatile UINT32 *)(Tcs + 0x38) = Saved[1];
  *(volatile UINT32 *)(Tcs + 0x3c) = Saved[2];
  MemoryFence();
Done:
  gBS->RestoreTPL(OldTpl);
  return Status;
}

// Stock CmdDbDxe: protocol table RVA 0x6098, revision 0x10000,
// QueryAddress at +0x10 -> RVA 0x14bc (This, Name, UINT32 *Address).
// Query returns EFI_SUCCESS even when the resource is absent (Address=0).
// These are command-DB addresses, NOT SPMI register addresses.
STATIC VOID
EldenEnableSdPower (VOID)
{
  EFI_GUID Guid = {0x1c34f691,0xd33d,0x4e14,{0xb8,0xf8,0x32,0xc6,0xc0,0x29,0xe9,0x5b}};
  typedef EFI_STATUS (EFIAPI *QUERY)(VOID *, CONST CHAR8 *, UINT32 *);
  UINT64 *Protocol = NULL;
  CONST CHAR8 *Names[] = {"ldob9", "ldob8", "ldoi2"};
  CONST UINT32 Millivolts[] = {2960, 2960, 1200};
  CONST UINT32 Expected[] = {0x41700, 0x41600, 0x42200};
  UINT32 Completion, J;
  UINT32 Address, Version, Config, Major;
  UINTN I, Base;
  EFI_STATUS S;

  S = gBS->LocateProtocol(&Guid, NULL, (VOID **)&Protocol);
  if (EFI_ERROR(S) || Protocol == NULL || Protocol[0] != 0x10000 || Protocol[2] == 0) {
    return;
  }
  for (I = 0; I < ARRAY_SIZE(Names); I++) {
    Address = 0;
    S = ((QUERY)(UINTN)Protocol[2])(Protocol, Names[I], &Address);
    // Linux cmd-db.c: address bits 18:16 identify the hardware slave;
    // all three must be VRM (4). Do not use an arbitrary returned address.
    if (EFI_ERROR(S) || Address == 0 || (Address & 3) != 0 ||
        ((Address >> 16) & 7) != 4 || Address != Expected[I]) {
      return;
    }
  }
  // sm8750.dtsi apps_rsc drv-2. Already mapped NS_DEVICE by APSS_HM.
  // Never reuse the SM8450 example's 0x17a20000 base on this SoC.
  Base = 0x16520000;
  Version = *(volatile UINT32 *)Base;
  Config = *(volatile UINT32 *)(Base + 0xc);
  Major = (Version >> 16) & 0xff;
  if (Major != 3 || Version != 0x30800 || Config != 0x800c8104) {
    return;
  }
  for (I = 0; I < ARRAY_SIZE(Names); I++) {
    // Configure voltage and mode BEFORE enable; keep all rails enabled.
    CONST UINT32 Offsets[] = {0, 8, 4};
    UINT32 Values[3];
    Values[0] = Millivolts[I]; Values[1] = 7; Values[2] = 1;
    for (J = 0; J < ARRAY_SIZE(Offsets); J++) {
      S = EldenSdVrmVote(Expected[I] + Offsets[J], Values[J], &Completion);
      if (EFI_ERROR(S)) {
        // Do not retry a failed power vote.
        return;
      }
    }
  }
  mEldenSdPowerCompleted = TRUE;
}

// Fixed-voltage diagnostic adapter, installed ONLY after all nine real
// requests completed. Keep 585c's DT initialization and stack epilogue.
// No fake NPA handles: voltage switching at 4d24 still fails closed (raw 4).
// Reject drive != 1 and disable requests, especially shared ldoi2 power-off.
STATIC BOOLEAN
EldenSdInstallPowerAdapter (VOID)
{
  CONST UINT32 Original[] = {
    0x7100069f, 0x54001be1, 0xd00000b4, 0xf944ee88,
    0xb5000208, 0xd00000a8
  };
  CONST UINT32 Adapter[] = {
    0x7100069f, 0x54001be1, // cmp w20,1; b.ne 5c6c (invalid request)
    0x7100067f, 0x54000001 | (((0x5c6c-0x58f8)/4)<<5),
    0x2a1f03e0, 0x14000000 | ((0x5c70-0x5900)/4)
  };
  VOID *Target;
  if (!mEldenSdPowerCompleted || mEldenSdImageBase == 0 ||
      mEldenSdImageSize != 0x1f000 ||
      *(volatile UINT8 *)(mEldenSdImageBase + 0x1b9a9) != 1) return FALSE;
  Target = (VOID *)(mEldenSdImageBase + 0x58ec);
  if (CompareMem(Target, Original, sizeof(Original)) != 0) return FALSE;
  CopyMem(Target, Adapter, sizeof(Adapter));
  WriteBackDataCacheRange(Target, sizeof(Adapter));
  InvalidateInstructionCacheRange(Target, sizeof(Adapter));
  MemoryFence();
  return CompareMem(Target, Adapter, sizeof(Adapter)) == 0;
}

STATIC VOID EFIAPI
EldenSdEvent (EFI_EVENT Event, VOID *Context)
{
  (VOID)Event;
  (VOID)Context;
}

STATIC VOID
EldenInitializeSd (VOID)
{
  // Stock SdccDxe registers removable enumeration at RVA 0x3cfc..0x3d28.
  // This is NOT ReadyToBoot or ExitBootServices.
  EFI_GUID Enumerate = {0xb7972c36,0x8a4c,0x4a56,{0x8b,0x02,0x11,0x59,0xb5,0x2d,0x4b,0xfb}};
  EFI_EVENT Event;
  EFI_STATUS S;

  S = EldenStartSd();
  if (!mEldenSdReturned || EFI_ERROR(mEldenSdStartStatus)) return;
  // The outer DXE dispatcher schedules consumers after this entry returns.
  S = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, EldenSdEvent, NULL, &Enumerate, &Event);
  if (EFI_ERROR(S)) return;
  S = gBS->SignalEvent(Event);
  gBS->CloseEvent(Event);
  EldenEnableSdPower();
  if (EldenSdInstallPowerAdapter()) {
    S = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, EldenSdEvent, NULL, &Enumerate, &Event);
    if (!EFI_ERROR(S)) {
      S = gBS->SignalEvent(Event);
      gBS->CloseEvent(Event);
    }
  }
}

// Compare inherited vs failed-init link state without power-mode writes.
STATIC
BOOLEAN
EldenCheckUfsLink (VOID)
{
  volatile UINT32 *Hci = (volatile UINT32 *)(UINTN)0x01d84000;
  CONST UINT32 Attrs[] = {0x1571,0x1568,0x1583,0x1560,0x1580,0x156a,0x0002,0x0002,0x1561};
  CONST UINT32 Cmds[] = {1,1,1,1,1,1,1,3,3};
  UINT32 I, Poll;
  EFI_STATUS DelayStatus;
  for (I = 0; I < ARRAY_SIZE(Attrs); I++) {
    if (Hci[0x34/4] != 1 || (Hci[0x30/4] & 8) == 0 ||
        Hci[0x58/4] != 0 || Hci[0x78/4] != 0) {
      break;
    }
    DelayStatus = gBS->Stall(10000);
    if (EFI_ERROR(DelayStatus)) break;
    Hci[0x20/4] = BIT10;
    MemoryFence();
    Hci[0x94/4] = Attrs[I] << 16;
    Hci[0x98/4] = 0;
    Hci[0x9c/4] = 0;
    MemoryFence();
    Hci[0x90/4] = Cmds[I];
    MemoryFence();
    for (Poll = 0; Poll < 100; Poll++) {
      if ((Hci[0x20/4] & BIT10) != 0) break;
      DelayStatus = gBS->Stall(1000);
      if (EFI_ERROR(DelayStatus)) break;
    }
    if (Poll == 100 || EFI_ERROR(DelayStatus)) {
      break;
    }
    Hci[0x20/4] = BIT10;
    MemoryFence();
  }
  return I == ARRAY_SIZE(Attrs);
}
EFI_STATUS EFIAPI
EldenStorageEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  STATIC BOOLEAN Attempted = FALSE;
  EFI_GUID *Arch[] = {
    &gEfiSecurityArchProtocolGuid,
    &gEfiCpuArchProtocolGuid,
    &gEfiMetronomeArchProtocolGuid,
    &gEfiTimerArchProtocolGuid,
    &gEfiBdsArchProtocolGuid,
    &gEfiWatchdogTimerArchProtocolGuid,
    &gEfiRuntimeArchProtocolGuid,
    &gEfiVariableArchProtocolGuid,
    &gEfiVariableWriteArchProtocolGuid,
    &gEfiCapsuleArchProtocolGuid,
    &gEfiMonotonicCounterArchProtocolGuid,
    &gEfiResetArchProtocolGuid,
    &gEfiRealTimeClockArchProtocolGuid,
  };
  EFI_GUID *IommuDependencies[] = {&mEnv, &mDalSys};
  EFI_STATUS Status;

  // DEPEX waits for architectural and vendor services; SD notifications must
  // run synchronously at TPL_APPLICATION before their results are consumed.
  (VOID)ImageHandle;
  (VOID)SystemTable;
  if (EfiGetCurrentTpl() != TPL_APPLICATION) return EFI_UNSUPPORTED;
  if (Attempted) return EFI_ALREADY_STARTED;
  Status = EldenRequireProtocols(Arch, ARRAY_SIZE(Arch));
  if (EFI_ERROR(Status)) return Status;
  Attempted = TRUE;
  Status = EldenRequireProtocols(IommuDependencies, ARRAY_SIZE(IommuDependencies));
  if (!EFI_ERROR(Status)) {
    Status = EldenStartFvImage(&mEldenIommuFile, &mEldenIommuReturned,
                              &mEldenIommuStartStatus, NULL, NULL);
  }
  // A failed HALIOMMU entry may leave its early protocol pointing into an
  // unloaded image. Do not dispatch consumers or continue storage preflight.
  if (EFI_ERROR(Status)) return Status;
  {
    // Elden mapped IMEM (NS_DEVICE). Stock UFSDxe 684c checks these
    // exact 32-bit fields before 67f4 converts the boot selector.
    volatile CONST UINT32 *Imem = (volatile CONST UINT32 *)(UINTN)0x14680000;
    UINT32 Magic, Version, Raw;
    STATIC CONST UINT32 BootMap[29] = {
      0,1,2,3,4,5,6,7,8,0,10,11,9,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,27,28
    };
    Magic = Imem[0];
    if (Magic == 0xc1f8db40) {
      Version = Imem[1];
      if (Version >= 4) {
        Raw = Imem[0x50/4];
        if (Raw <= 28 && ((0x18001dffU >> Raw) & 1U) != 0) {
          if (BootMap[Raw] == 8) {
            // UFSDxe 41a0/422c: This, ASCII region name, UINTN*.
            // HWIODxe table 60d0 rev10000 +8 ->1428 -> DAL +48.
            // Mapping can change DAL state; only the guarded HCI probe reads it.
            typedef EFI_STATUS (EFIAPI *ELDEN_HWIO_MAP)(VOID *, CONST CHAR8 *, UINTN *);
            EFI_GUID HwioGuid = {0x9e61de7c,0x9927,0x4f28,{0x9f,0x3d,0x32,0x07,0x05,0xd8,0x18,0x01}};
            CONST CHAR8 *Regions[2] = {"+UFS_MEM_UFS", "+UFS_MEM_ICE"};
            UINTN *Hwio = NULL;
            UINTN Address;
            UINT32 MapIndex, MapCount = 0;
            EFI_STATUS MapStatus;
            MapStatus = gBS->LocateProtocol(&HwioGuid, NULL, (VOID **)&Hwio);
            if (!EFI_ERROR(MapStatus) && Hwio != NULL && Hwio[0] == 0x10000 && Hwio[1] != 0) {
              for (MapIndex = 0; MapIndex < 2; MapIndex++) {
                Address = 0;
                MapStatus = ((ELDEN_HWIO_MAP)Hwio[1])(Hwio, Regions[MapIndex], &Address);
                if (EFI_ERROR(MapStatus) || Address == 0) {
                  break;
                }
                MapCount++;
                if (MapIndex == 0 && Address == 0x01d84000) {
                  {
                    // UFSDxe 1b128 locates this before IOMMU domain creation.
                    EFI_GUID IommuGuid = {0x54b6d3b4,0x5d33,0x4f91,{0x86,0x00,0x6c,0x41,0xd5,0xde,0xb1,0x9a}};
                    VOID *Iommu = NULL;
                    EFI_STATUS IommuStatus;
                    IommuStatus = gBS->LocateProtocol(&IommuGuid, NULL, &Iommu);
                    if (mEldenIommuReturned && !EFI_ERROR(mEldenIommuStartStatus) &&
                        !EFI_ERROR(IommuStatus) && Iommu != NULL) {
                      // Stock 0x152c only stores the table address into its sole
                      // output argument. No This argument or status return.
                      typedef VOID (EFIAPI *IOMMU_GET_TABLE)(VOID **);
                      UINTN *Protocol = (UINTN *)Iommu;
                      VOID *Table = NULL;
                      if (Protocol[1] != 0) {
                        ((IOMMU_GET_TABLE)Protocol[1])(&Table);
                        if (Table != NULL) {
                          // Stock table +0: allocate/zero 64-byte software domain.
                          // +8: release it; fresh domains have no attached nodes.
                          typedef INT32 (EFIAPI *IOMMU_CREATE)(VOID **);
                          typedef INT32 (EFIAPI *IOMMU_DESTROY)(VOID *);
                          typedef INT32 (EFIAPI *IOMMU_CONFIGURE)(VOID *, UINT32);
                          UINTN *Methods = (UINTN *)Table;
                          VOID *Domain = NULL;
                          INT32 DomainStatus;
                          if (Methods[0] != 0 && Methods[1] != 0 && Methods[8] != 0) {
                            DomainStatus = ((IOMMU_CREATE)Methods[0])(&Domain);
                            if (DomainStatus == 0 && Domain != NULL) {
                              UINT32 Mode, Passed = 0;
                              UINT8 Before[64];
                              // Configure 0x20f4 updates only this object while
                              // its attachment list is empty (0x2030 early exit).
                              for (Mode = 0; Mode < 2; Mode++) {
                                DomainStatus = ((IOMMU_CONFIGURE)Methods[8])(Domain, Mode);
                                if (DomainStatus != 0) break;
                                Passed++;
                              }
                              if (Passed == 2) {
                                CopyMem (Before, Domain, sizeof(Before));
                                DomainStatus = ((IOMMU_CONFIGURE)Methods[8])(Domain, 2);
                                if (DomainStatus == 2 && CompareMem(Before, Domain, sizeof(Before)) == 0) Passed++;
                              }
                              DomainStatus = ((IOMMU_DESTROY)Methods[1])(Domain);
                              Domain = NULL;
                              if (DomainStatus == 0) Passed++;
                              mEldenSoftwareReady = (Passed == 4);
                            }
                          }
                        }
                      }
                    }
                    {
                      EFI_GUID EnvGuid = {0x0389b776,0x625f,0x11eb,{0x83,0xbe,0xc7,0x41,0xa9,0x13,0xde,0x34}};
                      UINTN *ConfigEnv = NULL;
                      UINT64 IortNode[2] = {0, 0};
                      CONST CHAR8 *Keys[3] = {"Signature", "Revision", "Checksum"};
                      UINT32 K, Word, Count = 0;
                      INT32 Ret;
                      typedef INT32 (EFIAPI *IORT_FIND)(VOID *, CONST CHAR8 *);
                      typedef INT32 (EFIAPI *IORT_READ)(VOID *, CONST CHAR8 *, VOID *, UINT32);
                      IommuStatus = gBS->LocateProtocol(&EnvGuid, NULL, (VOID **)&ConfigEnv);
                      if (!EFI_ERROR(IommuStatus) && ConfigEnv != NULL && ConfigEnv[6] && ConfigEnv[0xa8/8]) {
                        Ret = ((IORT_FIND)ConfigEnv[6])(IortNode, "/soc/iort");
                        if (Ret == 0) {
                          for (K = 0; K < 3; K++) {
                            Word = 0;
                            Ret = ((IORT_READ)ConfigEnv[0xa8/8])(IortNode, Keys[K], &Word, sizeof(Word));
                            if (Ret != 0) break;
                            Count++;
                          }
                        }
                      }
                      if (Count == 3) {
                        CONST CHAR8 *SmmuKeys[7] = {"Type", "BaseAddress", "Span", "Flags", "Model", "NumContextInterrupts", "NumberofMappings"};
                        Count = 0;
                        ZeroMem(IortNode, sizeof(IortNode));
                        Ret = ((IORT_FIND)ConfigEnv[6])(IortNode, "/soc/iort/APPS_MMU500_SMMU_APP");
                        if (Ret == 0) {
                          for (K = 0; K < 7; K++) {
                            Word = 0;
                            Ret = ((IORT_READ)ConfigEnv[0xa8/8])(IortNode, SmmuKeys[K], &Word, sizeof(Word));
                            if (Ret != 0) break;
                            Count++;
                          }
                        }
                        {
                          CONST CHAR8 *MapKeys[5] = {"InputBase", "NumIDs", "OutputBase", "Flags", "OutputReference"};
                          CONST UINT32 Expected[5] = {0x03000000, 1, 0x60, 0, 0x56};
                          UINT32 Matches = 0;
                          ZeroMem(IortNode, sizeof(IortNode));
                          Ret = ((IORT_FIND)ConfigEnv[6])(IortNode,
                            "/soc/iort/NAMEDNODE_UFS_95MEM/SIDMappings/SIDMappings_0");
                          if (Ret == 0) {
                            for (K = 0; K < 5; K++) {
                              Word = 0;
                              Ret = ((IORT_READ)ConfigEnv[0xa8/8])(IortNode, MapKeys[K], &Word, sizeof(Word));
                              if (Ret != 0 || Word != Expected[K]) break;
                              Matches++;
                            }
                          }
                          mEldenSidReady = (Matches == 5);
                        }
                      }
                    }
                  }
                }
              }
            }
            mEldenHwioReady = (MapCount == 2);
          }
        }
      }
    }
  }

  if (mEldenSoftwareReady && mEldenSidReady && mEldenHwioReady)
    mEldenSoftwareReady = EldenCheckUfsLink();
  if (mEldenSoftwareReady && mEldenSidReady && mEldenHwioReady)
    EldenStartFvImage(&mEldenUfsFile, &mEldenUfsReturned,
                     &mEldenUfsStartStatus, NULL, NULL);

  EldenInitializeSd();
  // Publish success only after the vendor entry returned successfully.
  // BlockIo alone is not proof: a failed vendor entry can publish it early.
  if (mEldenUfsReturned && !EFI_ERROR(mEldenUfsStartStatus)) {
    EFI_HANDLE Ready = NULL;
    Status = gBS->InstallProtocolInterface(&Ready, &gEldenUfsReadyProtocolGuid,
                                          EFI_NATIVE_INTERFACE, NULL);
    if (EFI_ERROR(Status)) return Status;
  }
  // No recursive Dispatch(): the outer dispatcher will discover consumers.
  // BDS connects controllers after all runnable DXE drivers have returned.
  return EFI_SUCCESS;
}
