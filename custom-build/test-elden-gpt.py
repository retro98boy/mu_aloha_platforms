#!/usr/bin/env python3
"""Execute the read-only GPT validator against mocked media; never access hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenConnect.c').read_text()
body = source[source.index('STATIC BOOLEAN\nEldenValidateGptHeader'):source.index('VOID\nEldenConnectDevices')]
for service in ('AllocatePool', 'FreePool', 'HandleProtocol'):
    body = body.replace('gBS->' + service, 'Mock' + service)
prefix = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define STATIC static
#define VOID void
#define TRUE 1
#define FALSE 0
#define DEBUG(x) ((void)0)
#define EFI_ERROR(x) ((x)!=0)
#define EfiBootServicesData 0
#define EFI_PTAB_HEADER_ID UINT64_C(0x5452415020494645)
typedef uint8_t UINT8;
typedef uint16_t CHAR16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uintptr_t UINTN;
typedef uint64_t EFI_LBA;
typedef int EFI_STATUS;
typedef int BOOLEAN;
typedef void *EFI_HANDLE;
typedef struct {uint8_t b[16];} EFI_GUID;
typedef struct {UINT64 Signature; UINT32 Revision,HeaderSize,CRC32,Reserved;} EFI_TABLE_HEADER;
#pragma pack(push,1)
typedef struct {EFI_TABLE_HEADER Header; EFI_LBA MyLBA,AlternateLBA,FirstUsableLBA,LastUsableLBA; EFI_GUID DiskGUID; EFI_LBA PartitionEntryLBA; UINT32 NumberOfPartitionEntries,SizeOfPartitionEntry,PartitionEntryArrayCRC32;} EFI_PARTITION_TABLE_HEADER;
typedef struct {EFI_GUID PartitionTypeGUID,UniquePartitionGUID; EFI_LBA StartingLBA,EndingLBA; UINT64 Attributes; CHAR16 PartitionName[36];} EFI_PARTITION_ENTRY;
#pragma pack(pop)
typedef struct {UINT32 MediaId; int LogicalPartition,RemovableMedia,MediaPresent,ReadOnly; UINT32 BlockSize,IoAlign; EFI_LBA LastBlock;} MEDIA;
typedef struct IO IO;
struct IO {MEDIA *Media; EFI_STATUS (*ReadBlocks)(IO*,UINT32,EFI_LBA,UINTN,VOID*); EFI_STATUS (*WriteBlocks)(IO*,UINT32,EFI_LBA,UINTN,VOID*); EFI_STATUS (*FlushBlocks)(IO*);};
typedef IO EFI_BLOCK_IO_PROTOCOL;
static void EldenRememberBootSlot(EFI_HANDLE h, IO *io, EFI_PARTITION_TABLE_HEADER *p,
                                 EFI_PARTITION_TABLE_HEADER *b, EFI_PARTITION_ENTRY *e) {}

static int mEldenFixedMediaGptValidated=0;
#define CopyMem memcpy
#define CompareMem memcmp
#define SetMem(p,n,v) memset(p,v,n)
static EFI_STATUS crc(VOID *p,UINTN n,UINT32 *out){UINT32 c=~0u;UINT8 *b=p;while(n--){c^=*b++;for(int j=0;j<8;j++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}*out=~c;return 0;}
static struct {EFI_STATUS (*CalculateCrc32)(VOID*,UINTN,UINT32*);} bs={crc},*gBS=&bs;
static EFI_STATUS MockAllocatePool(int t,UINTN n,VOID **p){*p=malloc(n);return *p?0:1;}
static void MockFreePool(VOID *p){free(p);}
static EFI_STATUS MockHandleProtocol(EFI_HANDLE h,EFI_GUID *g,VOID **p){*p=h;return 0;}
static UINT8 primary[4096],backup[4096],table[16384];
static int writes,mode;
static EFI_STATUS rd(IO *io,UINT32 id,EFI_LBA l,UINTN n,VOID *p){
 assert(id==7);assert(((UINTN)p&65535)==0);
 if(mode==4)return 1;
 if(l==1&&n==4096)memcpy(p,primary,n);
 else if(l==1048575&&n==4096)memcpy(p,backup,n);
 else if((l==2||l==1048571)&&n<=16384){memcpy(p,table,n);if(mode==3&&l!=2)((UINT8*)p)[0]^=1;}
 else {assert(!"unexpected non-GPT read");}
 return 0;
}
static EFI_STATUS wr(IO *io,UINT32 id,EFI_LBA l,UINTN n,VOID *p){writes++;assert(!"unexpected write");return 1;}
static EFI_STATUS flush(IO *io){assert(!"unexpected flush");return 1;}
static MEDIA media={7,0,0,1,0,4096,4,1048575};
static IO io={&media,rd,wr,flush};
static void init(void){
 memset(primary,0,sizeof(primary));memset(backup,0,sizeof(backup));memset(table,0,sizeof(table));
 EFI_PARTITION_ENTRY *e=(void*)table;e->PartitionTypeGUID.b[0]=1;e->StartingLBA=100;e->EndingLBA=199;memcpy(e->PartitionName,L"boot_b",14);
 EFI_PARTITION_TABLE_HEADER *h=(void*)primary;h->Header.Signature=EFI_PTAB_HEADER_ID;h->Header.Revision=0x10000;h->Header.HeaderSize=sizeof(*h);h->MyLBA=1;h->AlternateLBA=1048575;h->FirstUsableLBA=6;h->LastUsableLBA=1048570;h->PartitionEntryLBA=2;h->NumberOfPartitionEntries=128;h->SizeOfPartitionEntry=128;crc(table,sizeof(table),&h->PartitionEntryArrayCRC32);crc(h,sizeof(*h),&h->Header.CRC32);
 memcpy(backup,primary,4096);h=(void*)backup;h->MyLBA=1048575;h->AlternateLBA=1;h->PartitionEntryLBA=1048571;h->Header.CRC32=0;crc(h,sizeof(*h),&h->Header.CRC32);
 writes=mode=0;media.ReadOnly=0;media.BlockSize=4096;media.IoAlign=4;mEldenFixedMediaGptValidated=0;io.WriteBlocks=wr;io.FlushBlocks=flush;
}
'''
suffix = r'''
static void reseal(void) {
 for(int b=0;b<2;b++) {
  EFI_PARTITION_TABLE_HEADER *h=(void*)(b?backup:primary);
  crc(table,h->NumberOfPartitionEntries*128,&h->PartitionEntryArrayCRC32);
  h->Header.CRC32=0;crc(h,sizeof(*h),&h->Header.CRC32);
 }
}
int main(void){
 EFI_HANDLE handles[2]={&io,&io};EFI_GUID guid={0};
 // Relocated boot_b must be accepted; no write API is required.
 init();media.ReadOnly=1;io.WriteBlocks=NULL;io.FlushBlocks=NULL;
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(mEldenFixedMediaGptValidated&&!writes);
 // No boot_b, and any partition name/type, are equally valid.
 init();memset(((EFI_PARTITION_ENTRY*)table)->PartitionName,0,72);reseal();
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(mEldenFixedMediaGptValidated&&!writes);
 for(int count=32;count<=96;count+=64){
 init();((EFI_PARTITION_TABLE_HEADER*)primary)->NumberOfPartitionEntries=count;
 ((EFI_PARTITION_TABLE_HEADER*)backup)->NumberOfPartitionEntries=count;reseal();
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(mEldenFixedMediaGptValidated&&!writes);
 }
 init();primary[16]^=1;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();table[100]^=1;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();mode=3;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();mode=4;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();((EFI_PARTITION_TABLE_HEADER*)backup)->PartitionEntryLBA=1048570;reseal();
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();((EFI_PARTITION_ENTRY*)table)[1]=((EFI_PARTITION_ENTRY*)table)[0];reseal();
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();((EFI_PARTITION_ENTRY*)table)->StartingLBA=1;reseal();
 EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();media.IoAlign=3;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();media.BlockSize=512;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();EldenValidateFixedMediaGpt(handles,2,&guid);assert(mEldenFixedMediaGptValidated);
 // A failed subsequent validation must not retain a previous success.
 primary[16]^=1;EldenValidateFixedMediaGpt(handles,1,&guid);assert(!mEldenFixedMediaGptValidated);
 init();mEldenFixedMediaGptValidated=1;EldenValidateFixedMediaGpt(handles,0,&guid);assert(!mEldenFixedMediaGptValidated);
 puts("PASS: read-only GPT validation, relocated/missing boot_b, CRC/layout/overlap guards, no writes or partition-content reads");
}
'''
with tempfile.TemporaryDirectory(prefix='elden-gpt-test-') as tmp:
    src = Path(tmp)/'test.c'
    src.write_text(prefix+body+suffix)
    exe = Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-fshort-wchar','-fsanitize=address,undefined','-g',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    selector = source[source.index('STATIC UINTN\nEldenSelectParents'):source.index('STATIC VOID\nEldenPartitionConnect')]
    selector = selector.replace('gBS->HandleProtocol', 'MockHandleProtocol')
    selection_test = r'''
int main(void){
 EFI_GUID g={0};EFI_HANDLE all[513],out[16];MEDIA child=media;child.LogicalPartition=1;IO ci=io;ci.Media=&child;
 for(int i=0;i<118;i++)all[i]=i<6?(void*)&io:(void*)&ci;
 assert(EldenSelectParents(all,118,&g,out)==6);
 for(int i=0;i<17;i++)all[i]=&io;
 assert(EldenSelectParents(all,17,&g,out)==0);
 assert(EldenSelectParents(all,513,&g,out)==0);
 puts("PASS: actual parent filter: 118 total -> 6 parents, overflow rejected");
}
'''
    src.write_text(prefix+selector+selection_test)
    subprocess.run(['cc','-std=c11','-fshort-wchar','-fsanitize=address,undefined','-g',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
