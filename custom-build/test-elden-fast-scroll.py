#!/usr/bin/env python3
"""Host-check actual scroll/reset/dirty flush bodies with guarded RAM."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/AndromedaPkg/Library/FrameBufferSerialPortLib/FrameBufferSerialPortLib.c').read_text()

def function(signature):
    start = source.index(signature + '\n{')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

header = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef size_t UINTN;
typedef uint32_t UINT32;
typedef uint8_t UINT8;
#define STATIC static
#define VOID void
#define MAX_UINTN SIZE_MAX
#define ELDEN_COMPACT_LOG 1
#define FONT_HEIGHT 12
#define SCALE_FACTOR 3
#define FB_BGRA8888_BLACK 0
static UINTN gWidth=1904,gHeight=3040;
static struct { uintptr_t Address; } DisplayMemoryRegion;
static struct { UINT32 Background; } m_Color;
static UINTN mDirtyStart=MAX_UINTN,mDirtyEnd;
static UINTN flushes,lastStart,lastSize;
static void WriteBackDataCacheRange(void *p,UINTN n) {
  flushes++; lastStart=(uintptr_t)p-DisplayMemoryRegion.Address; lastSize=n;
  assert(lastStart+n<=gWidth*gHeight*4);
}
static void CopyMem(void *d,const void*s,UINTN n){memmove(d,s,n);}
static void SetMem32(void *d,UINTN n,UINT32 v){
  assert(n%4==0); for(UINTN i=0;i<n/4;i++) ((UINT32*)d)[i]=v;
}
'''
bodies = '\n'.join(function(s) for s in [
    'STATIC VOID EldenDirtyRows(UINTN First, UINTN Rows)',
    'void ResetFb(void)', 'void FbConScrollUp(void)', 'void FbConFlush(void)'])
test = r'''
int main(void) {
 UINTN count=gWidth*gHeight,row=gWidth*36;
 UINT32 *allocation=malloc((count+2)*4); assert(allocation);
 UINT32 *p=allocation+1; DisplayMemoryRegion.Address=(uintptr_t)p;
 allocation[0]=allocation[count+1]=0x12345678;
 for(UINTN i=0;i<count;i++)p[i]=(UINT32)i;
 m_Color.Background=0xff123456;
 FbConScrollUp(); assert(flushes==0);
 for(UINTN i=0;i<count-row;i++)assert(p[i]==i+row);
 for(UINTN i=count-row;i<count;i++)assert(p[i]==m_Color.Background);
 FbConFlush(); assert(flushes==1&&lastStart==0&&lastSize==count*4);
 FbConFlush(); assert(flushes==1);
 EldenDirtyRows(36,36); EldenDirtyRows(72,36);
 FbConFlush(); assert(flushes==2&&lastStart==36*gWidth*4&&lastSize==72*gWidth*4);
 ResetFb(); assert(flushes==2); FbConFlush(); assert(flushes==3);
 for(UINTN i=0;i<count;i++)assert(p[i]==0);
 assert(allocation[0]==0x12345678&&allocation[count+1]==0x12345678);
 free(allocation); return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='elden-scroll-test-') as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(header + bodies + test)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual scroll/reset/dirty flush bodies; overlap, guards, coalescing, empty flush; cache mocked')
