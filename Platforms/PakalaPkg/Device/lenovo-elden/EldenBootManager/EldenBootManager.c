#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/BlockIo.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/MsBootOptionsLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
VOID EldenConnectDevices (VOID);
VOID EldenMarkBootSuccessful (VOID);
STATIC BOOLEAN IsMenu(EFI_BOOT_MANAGER_LOAD_OPTION *Option) {
  EFI_DEVICE_PATH_PROTOCOL *Node;
  EFI_GUID Menu={0x4042708a,0x0f2d,0x4823,{0xac,0x60,0x0d,0x77,0xb3,0x11,0x18,0x89}};
  if (!Option->FilePath) return FALSE;
  for(Node=Option->FilePath;!IsDevicePathEnd(Node);Node=NextDevicePathNode(Node)) {
    if(DevicePathType(Node)==MEDIA_DEVICE_PATH && DevicePathSubType(Node)==MEDIA_PIWG_FW_FILE_DP &&
       DevicePathNodeLength(Node)==sizeof(MEDIA_FW_VOL_FILEPATH_DEVICE_PATH) &&
       CompareGuid(&((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node)->FvFileName,&Menu)) return TRUE;
  }
  return FALSE;
}
EFI_STATUS EFIAPI EldenChooseEfiBoot(EFI_BOOT_MANAGER_LOAD_OPTION *Option);
VOID EFIAPI DeviceBootManagerBdsEntry(VOID) {
  EldenConnectDevices();
}
EFI_DEVICE_PATH_PROTOCOL **EFIAPI DeviceBootManagerOnDemandConInConnect(VOID) { return NULL; }
EFI_HANDLE EFIAPI DeviceBootManagerBeforeConsole(EFI_DEVICE_PATH_PROTOCOL **Path,BDS_CONSOLE_CONNECT_ENTRY **Consoles) {
  EFI_HANDLE *Handles=NULL, Display=NULL; UINTN N=0,I; EFI_DEVICE_PATH_PROTOCOL *P;
  *Path=NULL; *Consoles=NULL;
  if(!EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleTextInProtocolGuid,NULL,&N,&Handles))) {
    for(I=0;I<N;I++) {
      P=DevicePathFromHandle(Handles[I]); if(!P) continue;
      EfiBootManagerUpdateConsoleVariable(ConIn,P,NULL);
      gBS->ConnectController(Handles[I],NULL,NULL,FALSE);
    }
    FreePool(Handles);
  }
  Handles=NULL; N=0;
  if(!EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,&gEfiGraphicsOutputProtocolGuid,NULL,&N,&Handles))) {
    for(I=0;I<N;I++) if((P=DevicePathFromHandle(Handles[I]))!=NULL) {
      // PlatformBootManagerBeforeConsole owns and frees the returned path.
      // DevicePathFromHandle is borrowed protocol storage, never pool memory.
      *Path=DuplicateDevicePath(P);
      if (*Path==NULL) {
        break;
      }
      Display=Handles[I];
      EfiBootManagerUpdateConsoleVariable(ConOut,P,NULL);
      EfiBootManagerUpdateConsoleVariable(ErrOut,P,NULL); break;
    }
    FreePool(Handles);
  }
  return Display;
}
EFI_DEVICE_PATH_PROTOCOL **EFIAPI DeviceBootManagerAfterConsole(VOID) {
  // Aloha has reached normal BDS with storage and console initialized.
  EldenMarkBootSuccessful();
  // Keep BootOrder as a fallback when no usable EFI partition is found.
  EfiBootManagerRefreshAllBootOption();
  return NULL;
}
EFI_STATUS EFIAPI DeviceBootManagerPriorityBoot(EFI_BOOT_MANAGER_LOAD_OPTION *Option) {
  return EldenChooseEfiBoot(Option);
}
VOID EFIAPI DeviceBootManagerProcessBootCompletion(EFI_BOOT_MANAGER_LOAD_OPTION *Option) {
  if(IsMenu(Option) && !EFI_ERROR(Option->Status)) {
    gRT->ResetSystem(EfiResetCold,EFI_SUCCESS,0,NULL);
  }
}
VOID EFIAPI DeviceBootManagerUnableToBoot(VOID) {
  EFI_BOOT_MANAGER_LOAD_OPTION Menu; EFI_STATUS S;
  S=MsBootOptionsLibGetBootManagerMenu(&Menu,"ELDEN");
  if(!EFI_ERROR(S)) {
    EfiBootManagerBoot(&Menu);
    DeviceBootManagerProcessBootCompletion(&Menu);
    EfiBootManagerFreeLoadOption(&Menu);
  }
}
