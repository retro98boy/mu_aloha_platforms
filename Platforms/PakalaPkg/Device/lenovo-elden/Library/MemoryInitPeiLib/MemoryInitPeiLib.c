// SPDX-License-Identifier: BSD-2-Clause-Patent
// Elden-only HOB policy. Do not bind this library to another device's map.
#include <PiPei.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformMemoryMapLib.h>

EFI_STATUS EFIAPI MemoryPeim (
  IN EFI_PHYSICAL_ADDRESS UefiMemoryBase,
  IN UINT64 UefiMemorySize
  )
{
  ARM_MEMORY_REGION_DESCRIPTOR_EX *Regions = GetPlatformMemoryMap ();
  ARM_MEMORY_REGION_DESCRIPTOR Table[MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT];
  UINTN Count = 0;
  UINTN Index;
  UINTN Prior;
  VOID *TranslationTableBase;
  UINTN TranslationTableSize;
  RETURN_STATUS Status;

  // PrePi constructs its HOB arena with the stack excluded at its upper end.
  if (UefiMemoryBase != 0xBD930000 || UefiMemorySize != 0x1A2D0000) {
    return EFI_INVALID_PARAMETER;
  }

  // Validate and construct MMU leaves before publishing any resource HOBs.
  for (Index = 0; Index < MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT; Index++) {
    ARM_MEMORY_REGION_DESCRIPTOR_EX *Region = &Regions[Index];
    if (Region->Length == 0) {
      break;
    }
    if (AsciiStrCmp (Region->Name, "DXE Heap") == 0) {
      // Lookup-only container; its free and stack children describe resources.
      if (Region->Address != UefiMemoryBase || Region->Length != UefiMemorySize) {
        return EFI_INVALID_PARAMETER;
      }
      continue;
    }
    if (Count >= MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT - 1 ||
        (Region->Address & 0xFFF) != 0 || (Region->Length & 0xFFF) != 0 ||
        Region->Address > MAX_UINT64 - Region->Length) {
      return EFI_INVALID_PARAMETER;
    }
    for (Prior = 0; Prior < Count; Prior++) {
      if (Region->Address < Table[Prior].PhysicalBase + Table[Prior].Length &&
          Table[Prior].PhysicalBase < Region->Address + Region->Length) {
        return EFI_INVALID_PARAMETER;
      }
    }
    Table[Count].PhysicalBase = Region->Address;
    Table[Count].VirtualBase = Region->Address;
    Table[Count].Length = Region->Length;
    Table[Count].Attributes = Region->ArmAttributes;
    Count++;
  }
  if (Index == MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT) {
    return EFI_INVALID_PARAMETER;
  }
  Table[Count].PhysicalBase = 0;
  Table[Count].VirtualBase = 0;
  Table[Count].Length = 0;
  Table[Count].Attributes = 0;

  for (Index = 0; Regions[Index].Length != 0; Index++) {
    ARM_MEMORY_REGION_DESCRIPTOR_EX *Region = &Regions[Index];
    if (Region->HobOption == NoHob) {
      continue;
    }
    BuildResourceDescriptorHob (Region->ResourceType, Region->ResourceAttribute,
                                Region->Address, Region->Length);
    // Conventional RAM is available via its resource HOB. Stack allocation
    // is emitted exactly once by the caller's BuildStackHob after MemoryPeim.
    if (Region->MemoryType != EfiConventionalMemory &&
        AsciiStrCmp (Region->Name, "UEFI Stack") != 0 &&
        (Region->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY ||
         Region->MemoryType == EfiRuntimeServicesData)) {
      BuildMemoryAllocationHob (Region->Address, Region->Length, Region->MemoryType);
    }
  }
  Status = ArmConfigureMmu (Table, &TranslationTableBase, &TranslationTableSize);
  if (RETURN_ERROR (Status)) {
    // The common PrePi caller only ASSERTs; do not continue with MMU failure
    // in builds where assertions have been compiled out.
    CpuDeadLoop ();
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
