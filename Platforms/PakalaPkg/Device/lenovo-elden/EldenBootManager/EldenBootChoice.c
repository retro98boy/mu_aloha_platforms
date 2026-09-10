// Select a bootable EFI filesystem without changing BootOrder or mounting media
// for write. EldenKeys maps volume +/- to arrows and power to Enter.
#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>

#define BOOT_FILE L"\\EFI\\BOOT\\BOOTAA64.EFI"
#define MENU_ROWS 6

typedef enum {
  UsbDisk,
  SdDisk,
  UfsDisk,
  OtherDisk
} STORAGE_TYPE;

typedef struct {
  EFI_HANDLE Handle;
  STORAGE_TYPE Type;
  UINTN Lun;
  UINTN Partition;
  UINTN Port;
  UINTN Slot;
} EFI_VOLUME;

STATIC BOOLEAN HasLoader(EFI_HANDLE Handle) {
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
  EFI_STATUS Status;
  UINT8 Magic[2];
  UINTN Size = sizeof(Magic);

  Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR(Status) || Fs == NULL) return FALSE;
  Status = Fs->OpenVolume(Fs, &Root);
  if (EFI_ERROR(Status) || Root == NULL) return FALSE;
  Status = Root->Open(Root, &File, BOOT_FILE, EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR(Status) && File != NULL) {
    Status = File->Read(File, &Size, Magic);
    File->Close(File);
  } else if (!EFI_ERROR(Status)) {
    Status = EFI_NOT_FOUND;
  }
  Root->Close(Root);
  return !EFI_ERROR(Status) && Size == sizeof(Magic) &&
         Magic[0] == 'M' && Magic[1] == 'Z';
}

STATIC VOID Describe(EFI_HANDLE Handle, EFI_VOLUME *Volume) {
  EFI_DEVICE_PATH_PROTOCOL *Path, *Node;
  EFI_BLOCK_IO_PROTOCOL *Io;
  BOOLEAN Usb = FALSE, Sd = FALSE, Ufs = FALSE;

  Volume->Handle = Handle;
  Volume->Type = OtherDisk;
  Path = DevicePathFromHandle(Handle);
  if (Path != NULL && IsDevicePathValid(Path, 4096)) {
    for (Node = Path; !IsDevicePathEnd(Node); Node = NextDevicePathNode(Node)) {
      if (DevicePathType(Node) == MESSAGING_DEVICE_PATH) {
        switch (DevicePathSubType(Node)) {
        case MSG_USB_DP:
          if (DevicePathNodeLength(Node) >= sizeof(USB_DEVICE_PATH))
            Volume->Port = ((USB_DEVICE_PATH *)Node)->ParentPortNumber + 1;
          Usb = TRUE;
          break;
        case MSG_USB_CLASS_DP:
        case MSG_USB_WWID_DP:
          Usb = TRUE;
          break;
        case MSG_SD_DP:
          if (DevicePathNodeLength(Node) >= sizeof(SD_DEVICE_PATH))
            Volume->Slot = ((SD_DEVICE_PATH *)Node)->SlotNumber;
          Sd = TRUE;
          break;
        case MSG_UFS_DP:
          if (DevicePathNodeLength(Node) >= sizeof(UFS_DEVICE_PATH))
            Volume->Lun = ((UFS_DEVICE_PATH *)Node)->Lun;
          Ufs = TRUE;
          break;
        }
      } else if (DevicePathType(Node) == MEDIA_DEVICE_PATH &&
                 DevicePathSubType(Node) == MEDIA_HARDDRIVE_DP &&
                 DevicePathNodeLength(Node) >= sizeof(HARDDRIVE_DEVICE_PATH)) {
        Volume->Partition = ((HARDDRIVE_DEVICE_PATH *)Node)->PartitionNumber;
      }
    }
  }

  // The vendor SD/UFS paths can omit standard nodes; on this board SD is
  // removable and UFS is fixed. USB must be checked before that fallback.
  if (Usb) Volume->Type = UsbDisk;
  else if (Sd) Volume->Type = SdDisk;
  else if (Ufs) Volume->Type = UfsDisk;
  else if (!EFI_ERROR(gBS->HandleProtocol(Handle, &gEfiBlockIoProtocolGuid,
                                        (VOID **)&Io)) && Io != NULL && Io->Media != NULL)
    Volume->Type = Io->Media->RemovableMedia ? SdDisk : UfsDisk;
}

STATIC INTN Compare(CONST EFI_VOLUME *A, CONST EFI_VOLUME *B) {
  if (A->Type != B->Type) return A->Type < B->Type ? -1 : 1;
  if (A->Type == UfsDisk && A->Lun != B->Lun) return A->Lun < B->Lun ? -1 : 1;
  if (A->Type == UsbDisk && A->Port != B->Port) return A->Port < B->Port ? -1 : 1;
  if (A->Type == SdDisk && A->Slot != B->Slot) return A->Slot < B->Slot ? -1 : 1;
  if (A->Partition != B->Partition) return A->Partition < B->Partition ? -1 : 1;
  return 0;
}

STATIC VOID DrawMenu(CONST EFI_VOLUME *Volumes, UINTN Count, UINTN Selected) {
  UINTN First = (Selected / MENU_ROWS) * MENU_ROWS;
  UINTN Last = First + MENU_ROWS;
  UINTN Index;
  CONST EFI_VOLUME *Volume;
  CHAR16 Label[32];

  if (Last > Count) Last = Count;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"+---------------------------------------+\r\n");
  Print(L"| EFI boot    Vol +/-: move   Power: OK |\r\n");
  Print(L"+---------------------------------------+\r\n");
  for (Index = First; Index < Last; Index++) {
    Volume = &Volumes[Index];
    switch (Volume->Type) {
    case UsbDisk:
      if (Volume->Port) UnicodeSPrint(Label, sizeof(Label), L"USB port %u  p%u", Volume->Port, Volume->Partition);
      else UnicodeSPrint(Label, sizeof(Label), L"USB  p%u", Volume->Partition);
      break;
    case SdDisk:
      UnicodeSPrint(Label, sizeof(Label), L"SD slot %u  p%u", Volume->Slot, Volume->Partition);
      break;
    case UfsDisk:
      UnicodeSPrint(Label, sizeof(Label), L"UFS LUN %u  p%u", Volume->Lun, Volume->Partition);
      break;
    default:
      UnicodeSPrint(Label, sizeof(Label), L"Other disk  p%u", Volume->Partition);
      break;
    }
    Print(L"| %c %2u. %-31s |\r\n", Index == Selected ? L'>' : L' ', Index + 1, Label);
  }
  if (Count > MENU_ROWS) Print(L"  %u-%u / %u\r\n", First + 1, Last, Count);
  Print(L"+---------------------------------------+\r\n");
  Print(L"Auto boot first in 3s; arrows/Enter work.\r\n");
}

STATIC UINTN Choose(CONST EFI_VOLUME *Volumes, UINTN Count) {
  EFI_EVENT Timer, Events[2];
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  UINTN Selected = 0, EventIndex;
  BOOLEAN Confirmed = FALSE;

  if (gST->ConOut == NULL) return 0;
  DrawMenu(Volumes, Count, Selected);
  if (gST->ConIn == NULL || gST->ConIn->WaitForKey == NULL) return 0;
  Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &Timer);
  if (EFI_ERROR(Status)) return 0;
  Status = gBS->SetTimer(Timer, TimerRelative, 3 * 10000000ULL);
  if (EFI_ERROR(Status)) { gBS->CloseEvent(Timer); return 0; }
  Events[0] = gST->ConIn->WaitForKey;
  Events[1] = Timer;

  for (;;) {
    if (!EFI_ERROR(gBS->CheckEvent(Timer))) break;
    Status = gBS->WaitForEvent(2, Events, &EventIndex);
    if (EFI_ERROR(Status) || EventIndex == 1) break;
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) continue;
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { Confirmed = TRUE; break; }
    if (Key.ScanCode != SCAN_UP && Key.ScanCode != SCAN_DOWN) continue;
    if (Key.ScanCode == SCAN_UP && Selected > 0) Selected--;
    if (Key.ScanCode == SCAN_DOWN && Selected + 1 < Count) Selected++;
    // A navigation key gives another full three seconds, even at a list edge.
    Status = gBS->SetTimer(Timer, TimerRelative, 3 * 10000000ULL);
    if (EFI_ERROR(Status)) break;
    DrawMenu(Volumes, Count, Selected);
  }
  gBS->CloseEvent(Timer);
  return Confirmed ? Selected : 0;
}

EFI_STATUS EFIAPI EldenChooseEfiBoot(EFI_BOOT_MANAGER_LOAD_OPTION *Option) {
  EFI_HANDLE *Handles = NULL;
  EFI_VOLUME *Volumes = NULL, Entry;
  EFI_DEVICE_PATH_PROTOCOL *LoaderPath;
  EFI_STATUS Status;
  UINTN HandleCount = 0, Count = 0, Index, Position, Choice;

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid,
                                   NULL, &HandleCount, &Handles);
  if (EFI_ERROR(Status) || HandleCount == 0) return EFI_NOT_FOUND;
  Volumes = AllocateZeroPool(HandleCount * sizeof(*Volumes));
  if (Volumes == NULL) { FreePool(Handles); return EFI_OUT_OF_RESOURCES; }

  for (Index = 0; Index < HandleCount; Index++) {
    if (DevicePathFromHandle(Handles[Index]) == NULL ||
        !IsDevicePathValid(DevicePathFromHandle(Handles[Index]), 4096) ||
        !HasLoader(Handles[Index])) continue;
    ZeroMem(&Entry, sizeof(Entry));
    Describe(Handles[Index], &Entry);
    for (Position = Count; Position > 0 && Compare(&Entry, &Volumes[Position - 1]) < 0; Position--)
      Volumes[Position] = Volumes[Position - 1];
    Volumes[Position] = Entry;
    Count++;
  }

  Status = EFI_NOT_FOUND;
  if (Count != 0) {
    Choice = Choose(Volumes, Count);
    LoaderPath = FileDevicePath(Volumes[Choice].Handle, BOOT_FILE);
    if (LoaderPath != NULL) {
      Status = EfiBootManagerInitializeLoadOption(Option, LoadOptionNumberUnassigned,
                 LoadOptionTypeBoot, LOAD_OPTION_ACTIVE, L"Elden EFI boot",
                 LoaderPath, NULL, 0);
      FreePool(LoaderPath);
    } else {
      Status = EFI_OUT_OF_RESOURCES;
    }
  }
  FreePool(Volumes);
  FreePool(Handles);
  return Status;
}
