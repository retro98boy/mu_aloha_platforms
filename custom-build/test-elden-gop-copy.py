#!/usr/bin/env python3
"""Execute actual VideoToVideo code against a snapshot-copy reference."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/Override/MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.c').read_text()
start = src.index('RETURN_STATUS\nFrameBufferBltLibVideoToVideo (')
end = src.index('\n}', start) + 2
fn = src[start:end]
prefix = '''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define IN
#define UINTN uintptr_t
#define INTN intptr_t
#define UINT8 uint8_t
#define RETURN_STATUS int
#define RETURN_INVALID_PARAMETER 2
#define RETURN_SUCCESS 0
#define CopyMem memmove
typedef struct {UINTN Width,Height,PixelsPerScanLine,BytesPerPixel; UINT8 *FrameBuffer;} FRAME_BUFFER_CONFIGURE;
'''
suffix = '''
int main(void) {
 uint32_t fb[48], expected[48], saved[48];
 FRAME_BUFFER_CONFIGURE c={6,5,8,4,(UINT8*)fb};
 unsigned cases=0, failures=0;
 for(unsigned h=1;h<=5;h++) for(unsigned w=1;w<=6;w++)
 for(unsigned sy=0;sy+h<=5;sy++) for(unsigned sx=0;sx+w<=6;sx++)
 for(unsigned dy=0;dy+h<=5;dy++) for(unsigned dx=0;dx+w<=6;dx++) {
  for(unsigned i=0;i<48;i++) saved[i]=expected[i]=fb[i]=i+1;
  for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++)
   expected[(dy+y)*8+dx+x]=saved[(sy+y)*8+sx+x];
  int rc=FrameBufferBltLibVideoToVideo(&c,sx,sy,dx,dy,w,h);
  cases++;
  if(rc || memcmp(fb,expected,sizeof(fb))) failures++;
 }
 printf("cases=%u failures=%u\\n",cases,failures);
 return failures ? 1 : 0;
}
'''
# Extra backing memory allows the old erroneous last-row access to be detected
# by value comparison without invoking an out-of-bounds host access.
with tempfile.TemporaryDirectory(prefix='elden-copy-test-') as tmp:
    exe = str(Path(tmp) / 'test')
    for old in (True, False):
        body = fn.replace('(Height - 1) * LineStride', 'Height * LineStride') if old else fn
        subprocess.run(['clang','-x','c','-O0','-o',exe,'-'], input=prefix+body+suffix,
                       text=True, check=True)
        result = subprocess.run([exe], capture_output=True, text=True)
        print(('OLD: ' if old else 'FIXED: ') + result.stdout.strip())
        assert result.returncode == (1 if old else 0)
