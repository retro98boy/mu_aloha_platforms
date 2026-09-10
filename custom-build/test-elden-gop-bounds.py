#!/usr/bin/env python3
"""Host regression of the actual GOP rectangle predicate (not hardware/Blt proof)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenGop/EldenGop.c').read_text()
rect = src[src.index('STATIC BOOLEAN Rect('):src.index('STATIC EFI_STATUS EFIAPI Blt(')]
code = '''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define STATIC static
#define BOOLEAN int
#define UINTN uintptr_t
#define FB_WIDTH 1904U
#define FB_HEIGHT 3040U
''' + rect + '''
int main(void) {
 assert(Rect(0,0,1904,3040));
 assert(Rect(1903,3039,1,1));
 assert(!Rect(1904,0,1,1));
 assert(!Rect(0,3040,1,1));
 assert(!Rect(1903,3039,2,1));
 assert(!Rect(0,0,UINTPTR_MAX,1));
 assert(!Rect(UINTPTR_MAX,0,1,1));
 puts("PASS: actual GOP rectangle bounds and overflow rejection; not full GOP test");
}
'''
with tempfile.TemporaryDirectory(prefix='elden-gop-test-') as tmp:
    exe = str(Path(tmp) / 'test')
    subprocess.run(['clang','-x','c','-o',exe,'-'], input=code, text=True, check=True)
    subprocess.run([exe], check=True)
