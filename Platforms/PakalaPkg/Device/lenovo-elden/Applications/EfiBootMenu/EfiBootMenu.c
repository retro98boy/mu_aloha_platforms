// SPDX-License-Identifier: BSD-2-Clause-Patent
// Local EFI picker application; returning hands control back to standard BDS.
// PmicKeysDxe maps volume +/- to arrows and power to Enter.
#include <Uefi.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiFont.h>
#include <Library/BaseLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/PeCoffLib.h>

#define BOOT_FILE L"\\EFI\\BOOT\\BOOTAA64.EFI"
#define MENU_ROWS 6
#define MENU_COLUMNS 72
#define MENU_LINES (MENU_ROWS + 9)
#define MENU_TEXT_SIZE 2048

STATIC VOID AppendMenuText(CHAR16 *Text, UINTN Size, CONST CHAR16 *Format, ...) {
  VA_LIST Args;
  UINTN Used = StrLen(Text) * sizeof(*Text);

  VA_START(Args, Format);
  UnicodeVSPrint(Text + Used / sizeof(*Text), Size - Used, Format, Args);
  VA_END(Args);
}

STATIC EFI_STATUS DrawScaledMenu(CHAR16 *Text) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_HII_FONT_PROTOCOL *Font;
  EFI_FONT_DISPLAY_INFO FontInfo;
  EFI_IMAGE_OUTPUT Image, *ImagePtr = &Image;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  EFI_STATUS Status;
  UINTN Scale, Width, Height, X, Y;
  CHAR16 *Line, *End;

  Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Gop);
  if (EFI_ERROR(Status)) return Status;
  if (Gop->Mode == NULL || Gop->Mode->Info == NULL) return EFI_NOT_READY;
  Status = gBS->LocateProtocol(&gEfiHiiFontProtocolGuid, NULL, (VOID **)&Font);
  if (EFI_ERROR(Status)) return Status;

  // Render the system 8x19 font into a private bitmap. Only this application's
  // menu is enlarged; console modes, HII fonts and the shared UI theme stay intact.
  ZeroMem(&Image, sizeof(Image));
  Image.Width = MENU_COLUMNS * EFI_GLYPH_WIDTH;
  Image.Height = MENU_LINES * EFI_GLYPH_HEIGHT;
  Scale = MIN(3, MIN(Gop->Mode->Info->HorizontalResolution / Image.Width,
                     Gop->Mode->Info->VerticalResolution / Image.Height));
  if (Scale == 0) return EFI_UNSUPPORTED;
  Width = Image.Width * Scale;
  Height = Image.Height * Scale;
  Image.Image.Bitmap = AllocateZeroPool(Image.Width * Image.Height * sizeof(*Pixels));
  if (Image.Image.Bitmap == NULL) return EFI_OUT_OF_RESOURCES;
  Pixels = AllocatePool(Width * Height * sizeof(*Pixels));
  if (Pixels == NULL) { FreePool(Image.Image.Bitmap); return EFI_OUT_OF_RESOURCES; }
  ZeroMem(&FontInfo, sizeof(FontInfo));
  FontInfo.ForegroundColor.Red = 255;
  FontInfo.ForegroundColor.Green = 255;
  FontInfo.ForegroundColor.Blue = 255;
  // Position each row explicitly: HII treats CR and LF as separate line breaks.
  // Restore the console-formatted text so it remains usable if rendering fails.
  for (Line = Text, Y = 0; *Line != L'\0' && Y < Image.Height; Y += EFI_GLYPH_HEIGHT) {
    End = StrStr(Line, L"\r\n");
    if (End != NULL) *End = L'\0';
    Status = Font->StringToImage(Font, EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_OUT_FLAG_CLIP |
                                      EFI_HII_OUT_FLAG_TRANSPARENT,
                                 Line, &FontInfo, &ImagePtr, 0, Y, NULL, NULL, NULL);
    if (End != NULL) *End = L'\r';
    if (EFI_ERROR(Status) || End == NULL) break;
    Line = End + 2;
  }
  if (!EFI_ERROR(Status)) {
    for (Y = 0; Y < Height; Y++) {
      for (X = 0; X < Width; X++) {
        Pixels[Y * Width + X] = Image.Image.Bitmap[(Y / Scale) * Image.Width + X / Scale];
      }
    }
    Status = Gop->Blt(Gop, Pixels, EfiBltBufferToVideo, 0, 0,
                      (Gop->Mode->Info->HorizontalResolution - Width) / 2,
                      (Gop->Mode->Info->VerticalResolution - Height) / 2,
                      Width, Height, 0);
  }
  FreePool(Pixels);
  FreePool(Image.Image.Bitmap);
  return Status;
}

STATIC RETURN_STATUS EFIAPI
ReadBootImage(VOID *FileHandle, UINTN FileOffset, UINTN *ReadSize, VOID *Buffer)
{
  EFI_FILE_PROTOCOL *File = FileHandle;
  EFI_STATUS Status;

  Status = File->SetPosition(File, FileOffset);
  if (EFI_ERROR(Status)) return (RETURN_STATUS)Status;
  return (RETURN_STATUS)File->Read(File, ReadSize, Buffer);
}

typedef enum {
  UsbDisk,
  SdDisk,
  UfsDisk,
  OtherDisk
} BOOT_MENU_STORAGE_TYPE;

typedef struct {
  EFI_HANDLE Handle;
  BOOT_MENU_STORAGE_TYPE Type;
  UINTN Lun;
  UINTN Partition;
  UINTN Port;
  UINTN Slot;
  CHAR16 PartitionName[37];
} BOOT_MENU_VOLUME;

STATIC BOOLEAN HasEfiLoader(EFI_HANDLE Handle) {
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
  EFI_STATUS Status;
  PE_COFF_LOADER_IMAGE_CONTEXT Context;
  BOOLEAN Supported = FALSE;

  Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR(Status) || Fs == NULL) return FALSE;
  Status = Fs->OpenVolume(Fs, &Root);
  if (EFI_ERROR(Status) || Root == NULL) return FALSE;
  Status = Root->Open(Root, &File, BOOT_FILE, EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR(Status) && File != NULL) {
    // Inspect without loading or executing candidates. LoadImage still owns
    // authentication and the final load checks for the selected application.
    ZeroMem(&Context, sizeof(Context));
    Context.Handle = File;
    Context.ImageRead = ReadBootImage;
    Status = (EFI_STATUS)PeCoffLoaderGetImageInfo(&Context);
    Supported = !EFI_ERROR(Status) && Context.Machine == IMAGE_FILE_MACHINE_ARM64 &&
                Context.ImageType == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION;
    File->Close(File);
  }
  Root->Close(Root);
  return Supported;
}

STATIC VOID ReadPartitionName(EFI_HANDLE Handle, CHAR16 *Name) {
  EFI_PARTITION_INFO_PROTOCOL *Info;
  UINTN I;

  Name[0] = 0;
  // PartitionDxe already validated and cached the GPT entry. Do not rescan
  // media merely to label a menu item, and do not assume the name is terminated.
  if (!EFI_ERROR(gBS->HandleProtocol(Handle, &gEfiPartitionInfoProtocolGuid,
                                    (VOID **)&Info)) && Info != NULL &&
      Info->Revision == EFI_PARTITION_INFO_PROTOCOL_REVISION && Info->Type == PARTITION_TYPE_GPT) {
    for (I = 0; I < ARRAY_SIZE(Info->Info.Gpt.PartitionName); I++) {
      CHAR16 Ch = Info->Info.Gpt.PartitionName[I];
      if (Ch == 0) break;
      Name[I] = Ch < 0x20 || Ch == 0x7f ? L'?' : Ch;
    }
    Name[I] = 0;
  }
}

STATIC VOID DescribeBootVolume(EFI_HANDLE Handle, BOOT_MENU_VOLUME *Volume) {
  EFI_DEVICE_PATH_PROTOCOL *Path, *Node;
  BOOLEAN Usb = FALSE, Sd = FALSE, Ufs = FALSE;

  Volume->Handle = Handle;
  Volume->Type = OtherDisk;
  ReadPartitionName(Handle, Volume->PartitionName);
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

  if (Usb) Volume->Type = UsbDisk;
  else if (Sd) Volume->Type = SdDisk;
  else if (Ufs) Volume->Type = UfsDisk;
}

STATIC INTN CompareBootVolumes(CONST BOOT_MENU_VOLUME *A, CONST BOOT_MENU_VOLUME *B) {
  if (A->Type != B->Type) return A->Type < B->Type ? -1 : 1;
  if (A->Type == UfsDisk && A->Lun != B->Lun) return A->Lun < B->Lun ? -1 : 1;
  if (A->Type == UsbDisk && A->Port != B->Port) return A->Port < B->Port ? -1 : 1;
  if (A->Type == SdDisk && A->Slot != B->Slot) return A->Slot < B->Slot ? -1 : 1;
  if (A->Partition != B->Partition) return A->Partition < B->Partition ? -1 : 1;
  return 0;
}

STATIC VOID DrawBootMenu(CONST BOOT_MENU_VOLUME *Volumes, UINTN Count, UINTN Selected) {
  // The separate Skip row stays visible on every page. When selected,
  // leave the final page of EFI entries visible above it.
  UINTN First = ((Selected < Count ? Selected : Count - 1) / MENU_ROWS) * MENU_ROWS;
  UINTN Last = First + MENU_ROWS;
  UINTN Index;
  CONST BOOT_MENU_VOLUME *Volume;
  CHAR16 Label[20];
  CHAR16 Text[MENU_TEXT_SIZE] = {0};

  if (Last > Count) Last = Count;
  gST->ConOut->ClearScreen(gST->ConOut);
  AppendMenuText(Text, sizeof(Text), L"+----------------------------------------------------------------+\r\n");
  AppendMenuText(Text, sizeof(Text), L"| EFI boot    Vol +/-: move   Power: OK                            |\r\n");
  AppendMenuText(Text, sizeof(Text), L"+----------------------------------------------------------------+\r\n");
  for (Index = First; Index < Last; Index++) {
    Volume = &Volumes[Index];
    switch (Volume->Type) {
    case UsbDisk:
      if (Volume->Port) UnicodeSPrint(Label, sizeof(Label), L"USB port %u p%u", Volume->Port, Volume->Partition);
      else UnicodeSPrint(Label, sizeof(Label), L"USB p%u", Volume->Partition);
      break;
    case SdDisk:
      UnicodeSPrint(Label, sizeof(Label), L"SD slot %u p%u", Volume->Slot, Volume->Partition);
      break;
    case UfsDisk:
      UnicodeSPrint(Label, sizeof(Label), L"UFS LUN %u p%u", Volume->Lun, Volume->Partition);
      break;
    default:
      UnicodeSPrint(Label, sizeof(Label), L"Other disk p%u", Volume->Partition);
      break;
    }
    AppendMenuText(Text, sizeof(Text), L"| %c %2u. %-19s %-36s |\r\n", Index == Selected ? L'>' : L' ', Index + 1,
          Label, Volume->PartitionName[0] ? Volume->PartitionName : L"(no GPT name)");
  }
  if (Count > MENU_ROWS) AppendMenuText(Text, sizeof(Text), L"  %u-%u / %u\r\n", First + 1, Last, Count);
  AppendMenuText(Text, sizeof(Text), L"+----------------------------------------------------------------+\r\n");
  AppendMenuText(Text, sizeof(Text), L"| %c     Skip boot (continue normal flow)                         |\r\n",
        Selected == Count ? L'>' : L' ');
  AppendMenuText(Text, sizeof(Text), L"+----------------------------------------------------------------+\r\n");
  AppendMenuText(Text, sizeof(Text), L"Auto-run selected entry in 3s; arrows/Enter work.\r\n");
  if (EFI_ERROR(DrawScaledMenu(Text))) gST->ConOut->OutputString(gST->ConOut, Text);
}

STATIC UINTN ChooseBootEntry(CONST BOOT_MENU_VOLUME *Volumes, UINTN Count) {
  EFI_EVENT Timer = NULL, Events[2];
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  UINTN Selected = 0, EventIndex;
  BOOLEAN CursorVisible;

  if (Count == 0) return 0;
  if (gST->ConOut == NULL) return 0;
  CursorVisible = gST->ConOut->Mode->CursorVisible;
  gST->ConOut->EnableCursor(gST->ConOut, FALSE);
  DrawBootMenu(Volumes, Count, Selected);
  if (gST->ConIn == NULL || gST->ConIn->WaitForKey == NULL) goto Done;
  Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &Timer);
  if (EFI_ERROR(Status)) goto Done;
  Status = gBS->SetTimer(Timer, TimerRelative, 3 * 10000000ULL);
  if (EFI_ERROR(Status)) goto Done;
  Events[0] = gST->ConIn->WaitForKey;
  Events[1] = Timer;

  for (;;) {
    if (!EFI_ERROR(gBS->CheckEvent(Timer))) break;
    Status = gBS->WaitForEvent(2, Events, &EventIndex);
    if (EFI_ERROR(Status) || EventIndex == 1) break;
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) continue;
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) break;
    if (Key.ScanCode != SCAN_UP && Key.ScanCode != SCAN_DOWN) continue;
    if (Key.ScanCode == SCAN_UP && Selected > 0) Selected--;
    if (Key.ScanCode == SCAN_DOWN && Selected < Count) Selected++;
    // A navigation key gives another full three seconds, even at a list edge.
    Status = gBS->SetTimer(Timer, TimerRelative, 3 * 10000000ULL);
    if (EFI_ERROR(Status)) break;
    DrawBootMenu(Volumes, Count, Selected);
  }
Done:
  if (Timer != NULL) gBS->CloseEvent(Timer);
  gST->ConOut->ClearScreen(gST->ConOut);
  gST->ConOut->EnableCursor(gST->ConOut, CursorVisible);
  return Selected;
}

STATIC EFI_STATUS BuildSelectedBootOption(EFI_BOOT_MANAGER_LOAD_OPTION *Option) {
  EFI_HANDLE *Handles = NULL;
  BOOT_MENU_VOLUME *Volumes = NULL, Entry;
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
        !HasEfiLoader(Handles[Index])) continue;
    ZeroMem(&Entry, sizeof(Entry));
    DescribeBootVolume(Handles[Index], &Entry);
    for (Position = Count; Position > 0 && CompareBootVolumes(&Entry, &Volumes[Position - 1]) < 0; Position--)
      Volumes[Position] = Volumes[Position - 1];
    Volumes[Position] = Entry;
    Count++;
  }

  Status = EFI_NOT_FOUND;
  if (Count != 0) {
    Choice = ChooseBootEntry(Volumes, Count);
    // The final row declines to construct an OS load option. The application
    // entry point then returns normally to its BDS caller.
    if (Choice < Count) {
      LoaderPath = FileDevicePath(Volumes[Choice].Handle, BOOT_FILE);
      if (LoaderPath != NULL) {
        Status = EfiBootManagerInitializeLoadOption(Option, LoadOptionNumberUnassigned,
                   LoadOptionTypeBoot, LOAD_OPTION_ACTIVE, L"Lenovo Y700 Gen4 EFI boot",
                   LoaderPath, NULL, 0);
        FreePool(LoaderPath);
      } else {
        Status = EFI_OUT_OF_RESOURCES;
      }
    }
  }
  FreePool(Volumes);
  FreePool(Handles);
  return Status;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_BOOT_MANAGER_LOAD_OPTION Option;
  EFI_STATUS Status;
  (VOID)ImageHandle;
  (VOID)SystemTable;

  Status = BuildSelectedBootOption(&Option);
  // No selection (including Skip) is a normal application exit, not a failed
  // priority-boot lookup. Standard BDS handles the successful app return;
  // the picker itself never opens FrontPage or changes BootOrder/BootNext.
  if (Status == EFI_NOT_FOUND) return EFI_SUCCESS;
  if (EFI_ERROR(Status)) return Status;
  EfiBootManagerBoot(&Option);
  Status = Option.Status;
  EfiBootManagerFreeLoadOption(&Option);
  return Status;
}
