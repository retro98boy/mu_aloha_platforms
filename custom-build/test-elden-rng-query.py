#!/usr/bin/env python3
"""Host test of the constructor's query/retry and size guards without trace flags."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/Override/MdePkg/Library/DxeRngLib/DxeRngLib.c').read_text()
start = source.index('  RngArraySize = 0;', source.index('DxeRngLibConstructor ('))
end = source.index('  RngArrayCnt =', start)
code = r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int EFI_STATUS;
typedef size_t UINTN;
typedef uint32_t UINT32;
typedef struct { char b[16]; } EFI_RNG_ALGORITHM;
#define EFI_INVALID_PARAMETER 2
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_NOT_FOUND 14
#define EFI_DEVICE_ERROR 7
#define EFI_ERROR(s) ((s)!=0)
#define DEBUG(x) do {} while(0)
static int mode, calls;
struct proto { EFI_STATUS (*GetInfo)(void *, UINTN *, EFI_RNG_ALGORITHM *); };
static EFI_STATUS getinfo(void *p, UINTN *size, EFI_RNG_ALGORITHM *list) {
  (void)p; calls++;
  if (!list) {
    if (mode==0) { *size=16; return EFI_BUFFER_TOO_SMALL; }
    if (mode==2) return EFI_DEVICE_ERROR;
    return EFI_INVALID_PARAMETER;
  }
  assert(*size==16);
  *size=mode==3?17:mode==4?8192:mode==5?0:16;
  return 0;
}
static struct proto protocol={getinfo};
static struct proto *mRngProtocol=&protocol;
static EFI_STATUS query(void) {
 EFI_STATUS Status; UINTN RngArraySize; EFI_RNG_ALGORITHM ProbeAlgorithm;
'''
code += source[start:end]
code += r'''
 return 0;
}
int main(void) {
 for(mode=0;mode<6;mode++) {
  calls=0;
  assert(query()==(mode<2?0:EFI_DEVICE_ERROR));
  assert(calls==((mode==0||mode==2)?1:2));
 }
}
'''
with tempfile.TemporaryDirectory(prefix='elden-rng-query-') as tmp:
    binary = str(Path(tmp) / 'test')
    subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-x', 'c', '-', '-o', binary], input=code, text=True, check=True)
    subprocess.run([binary], check=True)
print('PASS: standard query, stock NULL rejection/retry, real failure and malformed sizes')
