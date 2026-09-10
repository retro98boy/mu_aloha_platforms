"""Exercise the boot picker's real comparator on synthetic storage volumes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenBootManager/EldenBootChoice.c').read_text()
compare = source.split('STATIC INTN Compare(', 1)[1].split('STATIC VOID DrawMenu(', 1)[0]
compare = 'static INTN Compare(' + compare

program = r'''
#include <assert.h>
#include <stddef.h>
#define CONST const
typedef size_t UINTN;
typedef long INTN;
typedef void *EFI_HANDLE;
typedef enum { UsbDisk, SdDisk, UfsDisk, OtherDisk } STORAGE_TYPE;
typedef struct {
  EFI_HANDLE Handle;
  STORAGE_TYPE Type;
  UINTN Lun, Partition, Port, Slot;
} EFI_VOLUME;
''' + compare + r'''
int main(void) {
  EFI_VOLUME usb = {.Type=UsbDisk, .Port=1, .Partition=2};
  EFI_VOLUME sd = {.Type=SdDisk, .Slot=0, .Partition=1};
  EFI_VOLUME ufs_a = {.Type=UfsDisk, .Lun=0, .Partition=15};
  EFI_VOLUME ufs_b = {.Type=UfsDisk, .Lun=1, .Partition=1};
  EFI_VOLUME ufs_c = {.Type=UfsDisk, .Lun=0, .Partition=16};
  assert(Compare(&usb,&sd)<0 && Compare(&sd,&ufs_a)<0);
  assert(Compare(&ufs_a,&ufs_b)<0 && Compare(&ufs_a,&ufs_c)<0);
  assert(Compare(&ufs_a,&ufs_a)==0);
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='elden-efi-choice-') as directory:
    exe = str(Path(directory) / 'test')
    subprocess.run(['cc', '-Wall', '-Werror', '-x', 'c', '-', '-o', exe],
                   input=program, text=True, check=True)
    subprocess.run([exe], check=True)

assert source.count('SetTimer(Timer, TimerRelative, 3 * 10000000ULL)') == 2
assert source.index('Key.ScanCode != SCAN_UP && Key.ScanCode != SCAN_DOWN') < source.rindex('SetTimer(Timer, TimerRelative, 3 * 10000000ULL)')
assert 'return Confirmed ? Selected : 0' in source
print('PASS: actual EFI boot priority comparator and default-on-timeout path')
