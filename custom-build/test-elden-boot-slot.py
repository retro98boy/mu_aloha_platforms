#!/usr/bin/env python3
"""Run the actual slot acknowledgement code on RAM-backed GPT copies."""
import ast
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
# Reuse the host UEFI types and CRC implementation from the read-only test.
tree = ast.parse((root / 'custom-build/test-elden-gpt.py').read_text())
prefix = next(ast.literal_eval(n.value) for n in tree.body
              if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'prefix' for t in n.targets))
prefix = prefix[:prefix.index('static UINT8 primary')]
start = prefix.index('static void EldenRememberBootSlot')
prefix = prefix[:start] + prefix[prefix.index('static int mEldenFixedMediaGptValidated', start):]
source = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenBootSlot.c').read_text()
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
source = source.replace('gBS->AllocatePool', 'MockAllocatePool').replace('gBS->FreePool', 'MockFreePool')
support = r'''
#define BIT50 (UINT64_C(1)<<50)
#define BIT54 (UINT64_C(1)<<54)
#define BIT55 (UINT64_C(1)<<55)
#define MAX_UINTN UINTPTR_MAX
#define EFI_SUCCESS 0
#define EFI_DEVICE_ERROR 1
#define EFI_COMPROMISED_DATA 2
#define MESSAGING_DEVICE_PATH 3
#define MSG_USB_DP 5
#define MSG_USB_CLASS_DP 15
#define MSG_USB_WWID_DP 16
#define MSG_SD_DP 26
typedef struct {int type;} EFI_DEVICE_PATH_PROTOCOL;
static EFI_DEVICE_PATH_PROTOCOL path[2];
static EFI_DEVICE_PATH_PROTOCOL *DevicePathFromHandle(void *h){return path;}
static int IsDevicePathValid(void *p,int n){return 1;}
static int IsDevicePathEnd(EFI_DEVICE_PATH_PROTOCOL *p){return p==path+1;}
static EFI_DEVICE_PATH_PROTOCOL *NextDevicePathNode(EFI_DEVICE_PATH_PROTOCOL *p){return p+1;}
static int DevicePathType(EFI_DEVICE_PATH_PROTOCOL *p){return p->type?3:1;}
static int DevicePathSubType(EFI_DEVICE_PATH_PROTOCOL *p){return p->type;}
BOOLEAN EldenFixedMediaGptIsValid(void){return mEldenFixedMediaGptValidated;}
static UINT8 ph[4096],bh[4096],pt[16384],bt[16384];
static int writes,flushes,fail_write;
static EFI_LBA order[4];
static UINT8 *region(EFI_LBA l,UINTN n){
 if(l==1&&n==4096)return ph;
 if(l==1048575&&n==4096)return bh;
 if(l==2&&n==16384)return pt;
 if(l==1048571&&n==16384)return bt;
 assert(!"outside GPT");return NULL;
}
static EFI_STATUS rd(IO *i,UINT32 id,EFI_LBA l,UINTN n,VOID *p){
 assert(id==7);assert(((UINTN)p&65535)==0);memcpy(p,region(l,n),n);return 0;
}
static EFI_STATUS wr(IO *i,UINT32 id,EFI_LBA l,UINTN n,VOID *p){
 assert(id==7);assert(writes<4);order[writes++]=l;
 if(writes==fail_write)return 1;
 memcpy(region(l,n),p,n);return 0;
}
static EFI_STATUS flush(IO *i){flushes++;return 0;}
static MEDIA media={7,0,0,1,0,4096,4,1048575};
static IO io={&media,rd,wr,flush};
'''
tests = r'''
static void init(int active){
 memset(ph,0,sizeof(ph));memset(bh,0,sizeof(bh));memset(pt,0,sizeof(pt));
 EFI_PARTITION_ENTRY *e=(void*)pt;
 for(int i=0;i<2;i++){e[i].PartitionTypeGUID.b[0]=1;e[i].StartingLBA=100+i*100;e[i].EndingLBA=199+i*100;}
 memcpy(e[0].PartitionName,L"boot_a",14);memcpy(e[1].PartitionName,L"boot_b",14);
 e[active].Attributes=BIT50|(UINT64_C(3)<<48)|(UINT64_C(6)<<51)|4;
 EFI_PARTITION_TABLE_HEADER *h=(void*)ph;
 h->Header.Signature=EFI_PTAB_HEADER_ID;h->Header.Revision=0x10000;h->Header.HeaderSize=sizeof(*h);
 h->MyLBA=1;h->AlternateLBA=1048575;h->FirstUsableLBA=6;h->LastUsableLBA=1048570;
 h->PartitionEntryLBA=2;h->NumberOfPartitionEntries=128;h->SizeOfPartitionEntry=128;
 crc(pt,sizeof(pt),&h->PartitionEntryArrayCRC32);crc(h,sizeof(*h),&h->Header.CRC32);
 memcpy(bh,ph,sizeof(ph));h=(void*)bh;h->MyLBA=1048575;h->AlternateLBA=1;h->PartitionEntryLBA=1048571;
 h->Header.CRC32=0;crc(h,sizeof(*h),&h->Header.CRC32);memcpy(bt,pt,sizeof(pt));
 writes=flushes=fail_write=0;mBootIo=NULL;mBootDisks=0;mAttempted=0;
 mEldenFixedMediaGptValidated=1;media.ReadOnly=0;media.MediaId=7;path[0].type=0;
}
static void remember(void){EldenRememberBootSlot(&io,&io,(void*)ph,(void*)bh,(void*)pt);}
static void valid(UINT8 *hb,UINT8 *tb){
 EFI_PARTITION_TABLE_HEADER h;memcpy(&h,hb,sizeof(h));UINT32 old=h.Header.CRC32,c;
 h.Header.CRC32=0;crc(&h,sizeof(h),&c);assert(c==old);crc(tb,16384,&c);assert(c==h.PartitionEntryArrayCRC32);
}
int main(void){
 for(int active=0;active<2;active++){
  init(active);UINT8 before[16384];memcpy(before,pt,sizeof(pt));remember();EldenMarkBootSuccessful();
  assert(writes==4&&flushes==4);assert(order[0]==1048571&&order[1]==1048575&&order[2]==2&&order[3]==1);
  ((EFI_PARTITION_ENTRY*)before)[active].Attributes|=BIT54;
  assert(!memcmp(before,pt,sizeof(pt))&&!memcmp(pt,bt,sizeof(pt)));valid(ph,pt);valid(bh,bt);
  mAttempted=0;mBootDisks=0;remember();writes=flushes=0;EldenMarkBootSuccessful();assert(!writes&&!flushes);
 }
 init(0);remember();media.ReadOnly=1;EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();media.MediaId=8;EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();mEldenFixedMediaGptValidated=0;EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();ph[16]^=1;EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();pt[100]^=1;EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();bt[100]^=1;EldenMarkBootSuccessful();assert(!writes);
 init(0);((EFI_PARTITION_ENTRY*)pt)[1].Attributes|=BIT50;remember();EldenMarkBootSuccessful();assert(!writes);
 init(0);((EFI_PARTITION_ENTRY*)pt)[0].Attributes=0;remember();EldenMarkBootSuccessful();assert(!writes);
 init(0);remember();remember();EldenMarkBootSuccessful();assert(!writes);
 init(0);path[0].type=MSG_USB_DP;remember();EldenMarkBootSuccessful();assert(!writes);
 init(0);((EFI_PARTITION_ENTRY*)pt)[0].Attributes|=BIT55;remember();EldenMarkBootSuccessful();assert(!writes);
 for(int failure=1;failure<=4;failure++){
  init(0);remember();fail_write=failure;EldenMarkBootSuccessful();assert(writes==failure);
  if(failure<=2)valid(ph,pt);else valid(bh,bt);
 }
 puts("PASS: slots A/B, exact success-bit-only update, CRCs, backup-first write order, zero writes on repeat/ambiguity/invalid metadata, write-failure containment");
}
'''
with tempfile.TemporaryDirectory(prefix='elden-slot-test-') as temp:
    src = Path(temp) / 'test.c'
    src.write_text(prefix + support + source + tests)
    exe = Path(temp) / 'test'
    subprocess.run(['cc', '-std=c11', '-fshort-wchar', '-fsanitize=address,undefined', '-g', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
