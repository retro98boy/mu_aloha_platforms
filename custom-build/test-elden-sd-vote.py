"""Host-test the actual vote function with a simulated register window.

This verifies software guards/cleanup/timeout, not the hardware completion ABI.
"""
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenStorageDxe/EldenStorageDxe.c').read_text()
body = main.split('EldenSdVrmVote (', 1)[1].split('// Stock CmdDbDxe:', 1)[0]
body = 'static int EldenSdVrmVote (' + body
body = re.sub(r'\*\(volatile UINT32 \*\)\(([^\n]+?)\)', r'*reg(\1)', body)
body = body.replace('*(volatile UINT32 *)Rsc', '*reg(Rsc)')
body = body.replace('gBS->RaiseTPL', 'MockRaiseTPL').replace('gBS->RestoreTPL', 'MockRestoreTPL')
assert 'volatile UINT32 *' not in body
shim = r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef uint32_t UINT32;
typedef uintptr_t UINTN;
typedef int EFI_STATUS;
typedef int EFI_TPL;
typedef int BOOLEAN;
#define CONST const
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_NOT_READY 1
#define EFI_ACCESS_DENIED 2
#define EFI_DEVICE_ERROR 3
#define EFI_TIMEOUT 4
#define TPL_HIGH_LEVEL 31
#define BIT1 2
#define BIT16 0x10000
static uint32_t regs[0x1000/4];
static int tpl, suppress, bad, votes, busy;
static volatile uint32_t *reg(uintptr_t a) {
  assert(a >= 0x16520d00 && a < 0x16521d00 && !(a & 3));
  return &regs[(a-0x16520d00)/4];
}
static void tick(void) {
  if (*reg(0x16520fc4)==0x1010000 && !suppress && !*reg(0x16520d04)) {
    assert(tpl==31);
    assert(*reg(0x16520fd4)==0x10108);
    /* IRQ stays masked: command completion must be sufficient. */
    *reg(0x16520fe0)=bad ? 0x100 : 0x10100;
    if (busy) *reg(0x16520fc8)=0;
    if (!votes) votes++;
  }
  if (*reg(0x16520d08)==2) {
    *reg(0x16520d04)&=~2;
    *reg(0x16520d08)=0;
  }
}
static void MemoryFence(void) { tick(); }
static void CpuPause(void) { tick(); }
static int MockRaiseTPL(int t) { int old=tpl; tpl=t; return old; }
static void MockRestoreTPL(int t) { tpl=t; }
static void reset(void) {
  memset(regs,0,sizeof(regs)); tpl=4; votes=suppress=bad=busy=0;
  *reg(0x16520d00)=1;
  for (int i=0;i<3;i++) *reg(0x16520d28+i*0x2a0)=1;
  *reg(0x16520fd4)=0xaaaa;
  *reg(0x16520fd8)=0xbbbb;
  *reg(0x16520fdc)=0xcccc;
}
'''
tests = r'''
int main(void) {
  uint32_t c, before[sizeof(regs)/4];
  uint32_t a[]={0x41700,0x41600,0x42200};
  uint32_t v[]={2960,2960,1200};
  for(int i=0;i<3;i++) for(int j=0;j<3;j++) {
    reset(); uint32_t off[]={0,8,4}, val[]={v[i],7,1};
    assert(EldenSdVrmVote(a[i]+off[j],val[j],&c)==0);
    assert(c==0x10100 && votes==1 && tpl==4);
    assert(*reg(0x16520d00)==1 && *reg(0x16520d04)==0);
    assert(*reg(0x16520fc4)==0 && *reg(0x16520fcc)==0);
    assert(*reg(0x16520fd4)==0xaaaa && *reg(0x16520fd8)==0xbbbb);
    assert(*reg(0x16520fdc)==0xcccc);
  }
  reset(); memcpy(before,regs,sizeof(regs));
  assert(EldenSdVrmVote(0x42204,0,&c)==EFI_ACCESS_DENIED);
  assert(EldenSdVrmVote(0x42200,1800,&c)==EFI_ACCESS_DENIED);
  assert(EldenSdVrmVote(0x50000,1,&c)==EFI_ACCESS_DENIED);
  assert(!memcmp(before,regs,sizeof(regs)) && tpl==4);
  reset(); *reg(0x16520d00)=3; memcpy(before,regs,sizeof(regs));
  assert(EldenSdVrmVote(0x41700,2960,&c)==EFI_NOT_READY);
  assert(!memcmp(before,regs,sizeof(regs)) && tpl==4);
  reset(); *reg(0x16520d2c)=1;
  assert(EldenSdVrmVote(0x41700,2960,&c)==EFI_NOT_READY && !votes);
  reset(); suppress=1;
  *reg(0x16520fe0)=0x10100; /* stale completion must not imply success */
  assert(EldenSdVrmVote(0x41700,2960,&c)==EFI_TIMEOUT && tpl==4);
  assert(c==0x10100); /* real timeout sample, not initialized zero */
  assert(*reg(0x16520fcc)==1); /* no reuse/recovery of timed out request */
  reset(); bad=1;
  assert(EldenSdVrmVote(0x41700,2960,&c)==EFI_TIMEOUT && tpl==4);
  reset(); busy=1;
  assert(EldenSdVrmVote(0x41700,2960,&c)==EFI_TIMEOUT && c==0x10100 && tpl==4);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='elden-vote-') as d:
    p = Path(d)
    (p / 'test.c').write_text(shim + body + tests)
    subprocess.run(['cc', '-O2', '-Wall', '-Werror', str(p/'test.c'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True)
print('PASS: actual vote function: nine allowed writes, forbidden/off votes, busy, completion, timeout; simulated MMIO only')
