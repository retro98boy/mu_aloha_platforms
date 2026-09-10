// SPDX-License-Identifier: BSD-2-Clause-Patent
// Read-only interactive diagnostic: no variables, disks, reset or PMIC writes.
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

STATIC VOID DrawIndicator (EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, UINTN Page)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Colors[] = {
    { 0, 0, 255, 0 }, { 0, 255, 0, 0 }, { 255, 0, 0, 0 }
  };
  UINTN Width;
  UINTN Height;
  if (Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {
    return;
  }
  Width = Gop->Mode->Info->HorizontalResolution;
  Height = Gop->Mode->Info->VerticalResolution;
  if (Width < 32 || Height < 32) {
    return;
  }
  // Visible heartbeat, not a backlight change. Keep console text intact.
  Gop->Blt (Gop, &Colors[Page % 3], EfiBltVideoFill,
            0, 0, Width - 32, Height - 32, 32, 32, 0);
}

EFI_STATUS EFIAPI UefiMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_EVENT Events[1];
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
  UINTN EventCount = 1;
  UINTN Index;
  UINTN Seconds = 0;
  UINTN Page = 0;
  (VOID)ImageHandle;
  if (SystemTable == NULL || SystemTable->ConOut == NULL) {
    return EFI_NOT_READY;
  }
  Print (L"\r\nHello Elden - Aloha V1 bring-up\r\n");
  Print (L"Stage: EFI application entered. Build: %a %a\r\n", __DATE__, __TIME__);
  Print (L"This is NOT stock firmware's old logo.\r\n");
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
  if (!EFI_ERROR (Status) && Gop != NULL && Gop->Mode != NULL && Gop->Mode->Info != NULL) {
    Print (L"GOP: %ux%u; test square at bottom right.\r\n",
           Gop->Mode->Info->HorizontalResolution, Gop->Mode->Info->VerticalResolution);
  } else {
    Gop = NULL;
    Print (L"GOP unavailable; text console test only.\r\n");
  }
  Print (L"Key input disabled: preserve power-button behavior.\r\n");
  Print (L"No backlight control or PMIC reset settings are changed.\r\n");
  Print (L"Automatic return after 60 seconds.\r\n");

  Status = gBS->CreateEvent (EVT_TIMER, TPL_APPLICATION, NULL, NULL, &Events[0]);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->SetTimer (Events[0], TimerPeriodic, 10000000);
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (Events[0]);
    return Status;
  }
  DrawIndicator (Gop, Page);
  while (Seconds < 60) {
    Status = gBS->WaitForEvent (EventCount, Events, &Index);
    if (EFI_ERROR (Status)) {
      break;
    }
    if (Index == 0) {
      Seconds++;
      DrawIndicator (Gop, Page + Seconds);
      Print (L"\rAlive=%u s  Page=%u    ", Seconds, Page);
    }
  }
  gBS->SetTimer (Events[0], TimerCancel, 0);
  gBS->CloseEvent (Events[0]);
  Print (L"\r\nElden diagnostic returning to caller.\r\n");
  return Status;
}
