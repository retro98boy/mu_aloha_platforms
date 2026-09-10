"""Execute actual Emit/Poll C routines with a mocked read-only SPMI transport."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenKeys/EldenKeys.c').read_text()
code = s[s.index('STATIC VOID Emit'):s.index('STATIC VOID EFIAPI Wait')]
prefix = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define STATIC static
#define VOID void
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define BIT6 64
#define BIT7 128
#define SCAN_UP 1
#define SCAN_DOWN 2
#define SCAN_NULL 0
#define SCAN_ESC 23
#define CHAR_TAB 9
#define CHAR_CARRIAGE_RETURN 13
#define EFI_ERROR(x) ((x)!=0)
#define ZeroMem(p,n) memset(p,0,n)
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint16_t CHAR16;
typedef uint32_t UINT32; typedef size_t UINTN; typedef int BOOLEAN;
typedef void *EFI_EVENT;
typedef struct { struct {UINT16 ScanCode,UnicodeChar;} Key;
 struct {UINT32 KeyShiftState;UINT8 KeyToggleState;} KeyState;} EFI_KEY_DATA;
typedef struct SPMI SPMI;
struct SPMI { uint64_t Revision; UINT32 (*Read)(SPMI*,UINT32,UINT32,UINT32,UINT32,UINT8*,UINT32,UINT32*); };
static UINT8 up=1,pon=0; static int fail;
static UINT32 rd(SPMI *p,UINT32 bus,UINT32 sid,UINT32 prio,UINT32 addr,UINT8 *v,UINT32 len,UINT32 *n) {
 assert(bus==0 && prio==0 && len==1);
 assert((sid==1 && addr==0x8d10)||(sid==0 && addr==0x1310));
 if(fail) return 1;
 *v=sid?up:pon;*n=1;return 0;
}
static SPMI spmi={0x10003,rd},*mSpmi=&spmi;
static int locate(void*a,void*b,void**c){*c=&spmi;return 0;}
static struct {int(*LocateProtocol)(void*,void*,void**);} bs={locate},*gBS=&bs;
static int mSpmiGuid;
static EFI_KEY_DATA mQueue[16]; static UINTN mHead,mTail;
static struct {EFI_KEY_DATA Key; void(*Fn)(EFI_KEY_DATA*);} mNotify[16];
static struct {UINTN Samples,Held;BOOLEAN Raw,Down,Long,Armed;} mButton[3];
'''
tests = r'''
static void ticks(int n){while(n--)Poll(NULL,NULL);}
static void clear(void){mHead=mTail=0;memset(mButton,0,sizeof(mButton));up=1;pon=0;fail=0;ticks(3);}
int main(void){
 clear();up=0;ticks(5);up=1;ticks(3);assert(mTail==1&&mQueue[0].Key.ScanCode==1);
 clear();pon=64;ticks(5);pon=0;ticks(3);assert(mTail==1&&mQueue[0].Key.ScanCode==2);
 clear();pon=128;ticks(5);pon=0;ticks(3);assert(mTail==1&&mQueue[0].Key.UnicodeChar==13);
 clear();up=0;ticks(45);up=1;ticks(3);assert(mTail==1&&mQueue[0].Key.UnicodeChar==9);
 clear();pon=64;ticks(45);pon=0;ticks(3);assert(mTail==1&&mQueue[0].Key.ScanCode==23);
 clear();pon=128;ticks(45);pon=0;ticks(3);assert(mTail==0);
 clear();up=0;ticks(1);up=1;ticks(3);assert(mTail==0);
 clear();up=0;ticks(5);fail=1;ticks(1);fail=0;up=1;ticks(3);assert(mTail==0);
 clear();memset(mButton,0,sizeof(mButton));up=0;ticks(50);up=1;ticks(3);assert(mTail==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='elden-keys-') as d:
    exe = str(Path(d)/'test')
    subprocess.run(['cc','-x','c','-','-o',exe],input=prefix+code+tests,text=True,check=True)
    subprocess.run([exe],check=True)
print('PASS: actual Poll/Emit; short, long, debounce, failed reads and held-at-boot. Not PMIC hardware proof.')
