// Elden read-only PMIC buttons. No PON configuration or reset writes.
#include <Uefi.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextInEx.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
typedef struct SPMI SPMI;
struct SPMI {
  UINT64 Revision;
  UINT32 (EFIAPI *Read)(SPMI *, UINT32, UINT32, UINT32, UINT32, UINT8 *, UINT32, UINT32 *);
};
STATIC EFI_GUID mSpmiGuid = {0xfa5f306b,0xf47d,0x4ac4,{0xa4,0x7d,0x88,0x2f,0x82,4,0xec,0x30}};
STATIC SPMI *mSpmi;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL mIn;
STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL mEx;
STATIC EFI_KEY_DATA mQueue[16];
STATIC UINTN mHead, mTail;
STATIC struct { EFI_KEY_DATA Key; EFI_KEY_NOTIFY_FUNCTION Fn; } mNotify[16];
STATIC struct { UINTN Samples, Held; BOOLEAN Raw, Down, Long, Armed; } mButton[3];
STATIC struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } mPath = {
  {{HARDWARE_DEVICE_PATH,HW_VENDOR_DP,{sizeof(VENDOR_DEVICE_PATH),0}},
   {0x87503222,0x9d17,0x4503,{0xab,0x62,0xdf,0xc7,0x83,0x60,0x43,2}}},
  {END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{4,0}}
};
STATIC VOID Emit(UINT16 Scan, CHAR16 Char) {
  EFI_KEY_DATA K;
  UINTN I, Next = (mTail+1)%16;
  ZeroMem(&K,sizeof(K)); K.Key.ScanCode=Scan; K.Key.UnicodeChar=Char;
  if (Next != mHead) { mQueue[mTail]=K; mTail=Next; }
  for (I=0; I<16; I++) {
    if (mNotify[I].Fn && mNotify[I].Key.Key.ScanCode==Scan &&
        mNotify[I].Key.Key.UnicodeChar==Char &&
        mNotify[I].Key.KeyState.KeyShiftState==0 &&
        mNotify[I].Key.KeyState.KeyToggleState==0) mNotify[I].Fn(&K);
  }
}
STATIC VOID EFIAPI Poll(EFI_EVENT Event, VOID *Context) {
  UINT8 Up, Pon; UINT32 N; UINTN I; BOOLEAN Down[3];
  if (!mSpmi) {
    if (EFI_ERROR(gBS->LocateProtocol(&mSpmiGuid,NULL,(VOID **)&mSpmi))) return;
    if (mSpmi->Revision!=0x10003 || !mSpmi->Read) { mSpmi=NULL; return; }
  }
  N=0;
  if (mSpmi->Read(mSpmi,0,1,0,0x8d10,&Up,1,&N)!=0 || N!=1) goto Failed;
  N=0;
  // Stock DT: PMK8550 SID0 pon_hlos@1300. Gen3 RT_STS bits 6/7.
  if (mSpmi->Read(mSpmi,0,0,0,0x1310,&Pon,1,&N)!=0 || N!=1) goto Failed;
  Down[0]=(Up&1)==0; Down[1]=(Pon&BIT6)!=0; Down[2]=(Pon&BIT7)!=0;
  for (I=0; I<3; I++) {
    if (Down[I]!=mButton[I].Raw) { mButton[I].Raw=Down[I]; mButton[I].Samples=0; }
    if (mButton[I].Samples<2) mButton[I].Samples++;
    if (mButton[I].Samples<2) continue;
    if (!mButton[I].Armed) { if (!Down[I]) mButton[I].Armed=TRUE; continue; }
    if (Down[I]!=mButton[I].Down) {
      mButton[I].Down=Down[I];
      if (Down[I]) { mButton[I].Held=0; mButton[I].Long=FALSE; }
      else if (!mButton[I].Long) Emit(I==0?SCAN_UP:I==1?SCAN_DOWN:SCAN_NULL,I==2?CHAR_CARRIAGE_RETURN:0);
    }
    if (Down[I] && !mButton[I].Long && ++mButton[I].Held>=40) {
      mButton[I].Long=TRUE;
      if (I==0) Emit(SCAN_NULL,CHAR_TAB);
      if (I==1) Emit(SCAN_ESC,0);
      // Long power is reserved for the existing hardware reset mechanism.
    }
  }
  return;
Failed:
  ZeroMem(mButton,sizeof(mButton)); // Never turn a failed read into a key release.
}
STATIC VOID EFIAPI Wait(EFI_EVENT Event, VOID *Context) { if (mHead!=mTail) gBS->SignalEvent(Event); }
STATIC EFI_STATUS EFIAPI ResetEx(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, BOOLEAN Verify) {
  EFI_TPL T=gBS->RaiseTPL(TPL_CALLBACK); mHead=mTail=0; gBS->RestoreTPL(T); return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN Verify) { return ResetEx(&mEx,Verify); }
STATIC EFI_STATUS EFIAPI ReadEx(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_DATA *Key) {
  EFI_TPL T; if (!Key) return EFI_INVALID_PARAMETER;
  T=gBS->RaiseTPL(TPL_CALLBACK);
  if (mHead==mTail) { gBS->RestoreTPL(T); return EFI_NOT_READY; }
  *Key=mQueue[mHead]; mHead=(mHead+1)%16; gBS->RestoreTPL(T); return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Read(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key) {
  EFI_KEY_DATA K; EFI_STATUS S; if (!Key) return EFI_INVALID_PARAMETER;
  S=ReadEx(&mEx,&K); if (!EFI_ERROR(S)) *Key=K.Key; return S;
}
STATIC EFI_STATUS EFIAPI State(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_TOGGLE_STATE *State) { return State?EFI_UNSUPPORTED:EFI_INVALID_PARAMETER; }
STATIC EFI_STATUS EFIAPI Register(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_DATA *Key, EFI_KEY_NOTIFY_FUNCTION Fn, VOID **Handle) {
  UINTN I; EFI_TPL T;
  if (!Key || !Fn || !Handle) return EFI_INVALID_PARAMETER;
  T=gBS->RaiseTPL(TPL_CALLBACK);
  for(I=0;I<16;I++) if (!mNotify[I].Fn) { mNotify[I].Key=*Key; mNotify[I].Fn=Fn; *Handle=&mNotify[I]; gBS->RestoreTPL(T); return EFI_SUCCESS; }
  gBS->RestoreTPL(T); return EFI_OUT_OF_RESOURCES;
}
STATIC EFI_STATUS EFIAPI Unregister(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, VOID *Handle) {
  UINTN I; EFI_TPL T=gBS->RaiseTPL(TPL_CALLBACK);
  for(I=0;I<16;I++) if (Handle==&mNotify[I] && mNotify[I].Fn) { mNotify[I].Fn=NULL; gBS->RestoreTPL(T); return EFI_SUCCESS; }
  gBS->RestoreTPL(T); return EFI_INVALID_PARAMETER;
}
EFI_STATUS EFIAPI EldenKeysEntry(EFI_HANDLE Image, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS S; EFI_EVENT Timer=NULL; EFI_HANDLE Handle=NULL;
  mIn.Reset=Reset; mIn.ReadKeyStroke=Read;
  mEx.Reset=ResetEx; mEx.ReadKeyStrokeEx=ReadEx; mEx.SetState=State;
  mEx.RegisterKeyNotify=Register; mEx.UnregisterKeyNotify=Unregister;
  S=gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_NOTIFY,Wait,NULL,&mIn.WaitForKey); if(EFI_ERROR(S)) return S;
  S=gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_NOTIFY,Wait,NULL,&mEx.WaitForKeyEx); if(EFI_ERROR(S)) goto Fail;
  S=gBS->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Poll,NULL,&Timer); if(EFI_ERROR(S)) goto Fail;
  S=gBS->SetTimer(Timer,TimerPeriodic,200000); if(EFI_ERROR(S)) goto Fail;
  S=gBS->InstallMultipleProtocolInterfaces(&Handle,&gEfiDevicePathProtocolGuid,&mPath,
      &gEfiSimpleTextInProtocolGuid,&mIn,&gEfiSimpleTextInputExProtocolGuid,&mEx,NULL);
  if(!EFI_ERROR(S)) return S;
Fail:
  if(Timer) gBS->CloseEvent(Timer);
  if(mEx.WaitForKeyEx) gBS->CloseEvent(mEx.WaitForKeyEx);
  gBS->CloseEvent(mIn.WaitForKey); return S;
}
