#!/usr/bin/env python3
"""Compile the actual bounded screen filter; preserve failures/fragments."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/AndromedaPkg/Library/FrameBufferSerialPortLib/FrameBufferSerialPortLib.c').read_text()
body = source.split('// ELDEN_FILTER_BEGIN:', 1)[1].split('\n', 1)[1].split('// ELDEN_FILTER_END', 1)[0]
header = '''#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef unsigned char UINT8;
typedef size_t UINTN;
typedef char CHAR8;
typedef int BOOLEAN;
#define CONST const
#define STATIC static
#define TRUE 1
#define FALSE 0
'''
test = r'''
int main(void) {
 const char *hide[] = {"PROGRESS CODE: V03040003 I0\n", "Loading driver at 0x123\n",
 "Memory Allocation 0x123\n"};
 const char *keep[] = {"ELDEN DRIVER BEGIN guid\n", "ELDEN DRIVER END guid: Device Error\n",
 "ASSERT: failed\n", "ERROR CODE: 123\n", "ELDEN ENV Parse 0 END raw=-1\n",
 "ELDEN COLD OPEN_INIT END x0=0\n", "ELDEN COLD HOST_INIT BEGIN x0=0\n",
 "PROGRESS CODE: x\nASSERT: failed\n", "Loading driver at ", "", "ELDEN DRIVER END guid: Success but failed\n"};
 for (size_t i=0;i<sizeof(hide)/sizeof(*hide);i++) {
   assert(EldenHideRoutineLog((const UINT8*)hide[i],strlen(hide[i])));
   for (size_t n=0;n<strlen(hide[i]);n++)
     assert(!EldenHideRoutineLog((const UINT8*)hide[i],n));
 }
 for (size_t i=0;i<sizeof(keep)/sizeof(*keep);i++)
   assert(!EldenHideRoutineLog((const UINT8*)keep[i],strlen(keep[i])));
 const UINT8 bounded[]={'P','R','O','G'};
 assert(!EldenHideRoutineLog(bounded,sizeof(bounded)));
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='elden-log-test-') as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(header + body + test)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual bounded log filter; errors, assertions, fragments and multiline writes preserved')
