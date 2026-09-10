// SPDX-License-Identifier: BSD-2-Clause-Patent
// Host contract test of the real Elden tables/MemoryPeim with mocked UEFI sinks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include <PiPei.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformMemoryMapLib.h>
#include <Library/PlatformConfigurationMapLib.h>

EFI_STATUS EFIAPI MemoryPeim (EFI_PHYSICAL_ADDRESS Base, UINT64 Size);
static ARM_MEMORY_REGION_DESCRIPTOR mResources[128];
static UINTN mResourceCount, mAllocations, mMmuCount;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

INTN EFIAPI AsciiStrCmp (CONST CHAR8 *A, CONST CHAR8 *B) { return strcmp(A, B); }
VOID EFIAPI CpuDeadLoop (VOID) { abort(); }

VOID EFIAPI BuildResourceDescriptorHob (EFI_RESOURCE_TYPE Type,
  EFI_RESOURCE_ATTRIBUTE_TYPE Attr, EFI_PHYSICAL_ADDRESS Base, UINT64 Size)
{
  (void)Type; (void)Attr;
  CHECK(mResourceCount < 128);
  for (UINTN i = 0; i < mResourceCount; i++) {
    CHECK(Base >= mResources[i].PhysicalBase + mResources[i].Length ||
          mResources[i].PhysicalBase >= Base + Size);
  }
  mResources[mResourceCount].PhysicalBase = Base;
  mResources[mResourceCount++].Length = Size;
}

VOID EFIAPI BuildMemoryAllocationHob (EFI_PHYSICAL_ADDRESS Base,
                                     UINT64 Size, EFI_MEMORY_TYPE Type)
{
  // The whole DXE container must never become a conventional allocation HOB.
  CHECK(Type != EfiConventionalMemory);
  // The caller's BuildStackHob owns the new stack allocation, not MemoryPeim.
  CHECK(Base >= 0xD7C00000 || Base + Size <= 0xD7BC0000);
  mAllocations++;
}

RETURN_STATUS EFIAPI ArmConfigureMmu (ARM_MEMORY_REGION_DESCRIPTOR *Table,
                                     VOID **Base, UINTN *Size)
{
  UINTN i;
  for (i = 0; Table[i].Length != 0; i++) {
    CHECK(i < 127);
    CHECK(Table[i].PhysicalBase == Table[i].VirtualBase);
    for (UINTN j = 0; j < i; j++) {
      CHECK(Table[i].PhysicalBase >= Table[j].PhysicalBase + Table[j].Length ||
            Table[j].PhysicalBase >= Table[i].PhysicalBase + Table[i].Length);
    }
  }
  mMmuCount = i;
  *Base = NULL; *Size = 0; // no actual page tables/CPU state in this host test
  return RETURN_SUCCESS;
}

int main(void)
{
  CHECK(MemoryPeim(0, 0) == EFI_INVALID_PARAMETER);
  CHECK(mResourceCount == 0);
  CHECK(MemoryPeim(0xBD930000, 0x1A2D0000) == EFI_SUCCESS);
  CHECK(mResourceCount > 0 && mAllocations > 0 && mMmuCount > mResourceCount);
  printf("PASS: %lu non-overlapping MMU leaves, %lu resource HOBs, %lu allocation HOBs\n",
         (unsigned long)mMmuCount, (unsigned long)mResourceCount, (unsigned long)mAllocations);
  ARM_MEMORY_REGION_DESCRIPTOR_EX *regions = GetPlatformMemoryMap();
  EFI_PHYSICAL_ADDRESS saved = regions[1].Address;
  regions[1].Address = regions[0].Address; // injected overlap, rejected before HOB writes
  mResourceCount = 0;
  CHECK(MemoryPeim(0xBD930000, 0x1A2D0000) == EFI_INVALID_PARAMETER);
  CHECK(mResourceCount == 0);
  regions[1].Address = saved;
  CONFIGURATION_DESCRIPTOR_EX *config = GetPlatformConfigurationMap();
  unsigned disabled = 0;
  for (UINTN i = 0; strcmp(config[i].Name, "Terminator") != 0; i++) {
    CHECK(i < 100);
    if (!strcmp(config[i].Name, "EnableMultiThreading") ||
        !strcmp(config[i].Name, "EnableDisplayThread") ||
        !strcmp(config[i].Name, "EnableMultiCoreFvDecompression")) {
      CHECK(config[i].Value == 0); disabled++;
    }
  }
  CHECK(disabled == 3);
  puts("PASS: invalid arena/overlap rejection and bring-up threading configuration");
  return 0;
}
