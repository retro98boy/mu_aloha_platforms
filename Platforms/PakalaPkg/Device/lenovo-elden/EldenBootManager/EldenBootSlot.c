/** @file
  Acknowledge a successful Aloha boot to the Lenovo Elden Android bootloader.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Uefi/UefiGpt.h>
#include <Protocol/BlockIo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

// Qualcomm A/B attributes from QcomModulePkg/Include/Library/PartitionTableUpdate.h.
#define SLOT_ACTIVE       BIT50
#define SLOT_SUCCESSFUL   BIT54
#define SLOT_UNBOOTABLE   BIT55
#define GPT_BUFFER_ALIGN  65536

STATIC EFI_BLOCK_IO_PROTOCOL *mBootIo;
STATIC EFI_PARTITION_TABLE_HEADER mPrimary, mBackup;
STATIC UINTN mBootIndex;
STATIC UINTN mBootDisks;
STATIC BOOLEAN mAttempted;
STATIC UINT32 mMediaId;
BOOLEAN EldenFixedMediaGptIsValid (VOID);

// Called only after the existing read-only validator checked both GPT copies,
// their CRCs, matching entries, bounds and non-overlapping partition extents.
VOID
EldenRememberBootSlot (EFI_HANDLE Handle, EFI_BLOCK_IO_PROTOCOL *Io,
                      EFI_PARTITION_TABLE_HEADER *Primary,
                      EFI_PARTITION_TABLE_HEADER *Backup,
                      EFI_PARTITION_ENTRY *Entries)
{
  EFI_DEVICE_PATH_PROTOCOL *Path, *Node;
  UINTN I, A = MAX_UINTN, B = MAX_UINTN;
  BOOLEAN ActiveA, ActiveB;
  EFI_GUID ZeroGuid = {0};

  Path = DevicePathFromHandle(Handle);
  if (Path == NULL || !IsDevicePathValid(Path, 4096)) return;
  for (Node = Path; !IsDevicePathEnd(Node); Node = NextDevicePathNode(Node)) {
    // Vendor UFS paths may omit MSG_UFS_DP. Never accept external USB/SD
    // media just because a non-removable device contains Android GPT names.
    if (DevicePathType(Node) == MESSAGING_DEVICE_PATH &&
        (DevicePathSubType(Node) == MSG_USB_DP ||
         DevicePathSubType(Node) == MSG_USB_CLASS_DP ||
         DevicePathSubType(Node) == MSG_USB_WWID_DP ||
         DevicePathSubType(Node) == MSG_SD_DP)) return;
  }
  for (I = 0; I < Primary->NumberOfPartitionEntries; I++) {
    if (CompareMem(Entries[I].PartitionName, L"boot_a", sizeof(L"boot_a")) == 0) {
      if (A != MAX_UINTN) { mBootDisks = MAX_UINTN; return; }
      A = I;
    }
    if (CompareMem(Entries[I].PartitionName, L"boot_b", sizeof(L"boot_b")) == 0) {
      if (B != MAX_UINTN) { mBootDisks = MAX_UINTN; return; }
      B = I;
    }
  }
  if (A == MAX_UINTN && B == MAX_UINTN) return;
  if (mBootDisks == MAX_UINTN) return;
  mBootDisks++;
  if (A == MAX_UINTN || B == MAX_UINTN || mBootDisks != 1) return;
  if (CompareMem(&Entries[A].PartitionTypeGUID, &ZeroGuid, sizeof(ZeroGuid)) == 0 ||
      CompareMem(&Entries[B].PartitionTypeGUID, &ZeroGuid, sizeof(ZeroGuid)) == 0) return;
  ActiveA = (Entries[A].Attributes & SLOT_ACTIVE) != 0;
  ActiveB = (Entries[B].Attributes & SLOT_ACTIVE) != 0;
  if (ActiveA == ActiveB) return;
  I = ActiveA ? A : B;
  if ((Entries[I].Attributes & SLOT_UNBOOTABLE) != 0 ||
      Entries[I].StartingLBA < Primary->FirstUsableLBA ||
      Entries[I].EndingLBA > Primary->LastUsableLBA ||
      Entries[I].StartingLBA > Entries[I].EndingLBA) return;
  mBootIndex = I;
  mBootIo = Io;
  mMediaId = Io->Media->MediaId;
  CopyMem(&mPrimary, Primary, sizeof(mPrimary));
  CopyMem(&mBackup, Backup, sizeof(mBackup));
}

STATIC EFI_STATUS
WriteAndVerify (EFI_BLOCK_IO_PROTOCOL *Io, UINT32 MediaId, EFI_LBA Lba,
                UINTN Bytes, VOID *Data, VOID *Verify)
{
  EFI_STATUS Status;
  Status = Io->WriteBlocks(Io, MediaId, Lba, Bytes, Data);
  if (EFI_ERROR(Status)) return Status;
  Status = Io->FlushBlocks(Io);
  if (EFI_ERROR(Status)) return Status;
  Status = Io->ReadBlocks(Io, MediaId, Lba, Bytes, Verify);
  if (EFI_ERROR(Status)) return Status;
  return CompareMem(Data, Verify, Bytes) == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

VOID
EldenMarkBootSuccessful (VOID)
{
  EFI_BLOCK_IO_PROTOCOL *Io = mBootIo;
  VOID *Allocation = NULL;
  UINT8 *PrimaryBlock, *BackupBlock, *Entries, *OtherEntries, *Verify;
  EFI_PARTITION_TABLE_HEADER *Primary, *Backup;
  EFI_PARTITION_ENTRY *Slot;
  UINTN Bytes, AlignedBytes;
  UINT32 Crc, MediaId;
  EFI_STATUS Status;

  if (mAttempted) return;
  mAttempted = TRUE;
  if (!EldenFixedMediaGptIsValid() || mBootDisks != 1 || Io == NULL) {
    DEBUG((DEBUG_ERROR, "Lenovo Elden: no unambiguous validated boot slot; not acknowledging boot\n"));
    return;
  }
  if (!Io->Media->MediaPresent || Io->Media->MediaId != mMediaId ||
      Io->Media->BlockSize != 4096 || Io->Media->ReadOnly ||
      Io->WriteBlocks == NULL || Io->FlushBlocks == NULL) {
    DEBUG((DEBUG_ERROR, "Lenovo Elden: boot-slot metadata is not writable\n"));
    return;
  }
  Status = gBS->AllocatePool(EfiBootServicesData, 6 * GPT_BUFFER_ALIGN, &Allocation);
  if (EFI_ERROR(Status)) return;
  PrimaryBlock = (UINT8 *)(((UINTN)Allocation + GPT_BUFFER_ALIGN - 1) & ~(UINTN)(GPT_BUFFER_ALIGN - 1));
  BackupBlock = PrimaryBlock + GPT_BUFFER_ALIGN;
  Entries = BackupBlock + GPT_BUFFER_ALIGN;
  OtherEntries = Entries + GPT_BUFFER_ALIGN;
  Verify = OtherEntries + GPT_BUFFER_ALIGN;
  Primary = (EFI_PARTITION_TABLE_HEADER *)PrimaryBlock;
  Backup = (EFI_PARTITION_TABLE_HEADER *)BackupBlock;
  Bytes = mPrimary.NumberOfPartitionEntries * sizeof(EFI_PARTITION_ENTRY);
  AlignedBytes = (Bytes + 4095) & ~(UINTN)4095;
  MediaId = Io->Media->MediaId;

  // Re-read the validated snapshot before writing. Preserve full header
  // sectors and table padding, and refuse metadata changed since validation.
  Status = Io->ReadBlocks(Io, MediaId, mPrimary.MyLBA, 4096, PrimaryBlock);
  if (EFI_ERROR(Status)) goto Done;
  Status = Io->ReadBlocks(Io, MediaId, mBackup.MyLBA, 4096, BackupBlock);
  if (EFI_ERROR(Status)) goto Done;
  Status = EFI_COMPROMISED_DATA;
  if (CompareMem(Primary, &mPrimary, sizeof(mPrimary)) != 0 ||
      CompareMem(Backup, &mBackup, sizeof(mBackup)) != 0) goto Done;
  Status = Io->ReadBlocks(Io, MediaId, Primary->PartitionEntryLBA, AlignedBytes, Entries);
  if (EFI_ERROR(Status)) goto Done;
  Status = Io->ReadBlocks(Io, MediaId, Backup->PartitionEntryLBA, AlignedBytes, OtherEntries);
  if (EFI_ERROR(Status)) goto Done;
  Status = gBS->CalculateCrc32(Entries, Bytes, &Crc);
  if (EFI_ERROR(Status)) goto Done;
  Status = EFI_COMPROMISED_DATA;
  if (Crc != Primary->PartitionEntryArrayCRC32 || CompareMem(Entries, OtherEntries, Bytes) != 0) goto Done;
  Slot = &((EFI_PARTITION_ENTRY *)Entries)[mBootIndex];
  if ((Slot->Attributes & SLOT_SUCCESSFUL) != 0) {
    Status = EFI_SUCCESS;
    goto Done;
  }

  // ABL decrements retry_count while successful is clear, even when Aloha
  // and Linux boot normally. After seven attempts it rejects this intact
  // boot image and falls back to fastboot. Aloha is the Android boot payload:
  // reaching normal BDS acknowledges it, independently of which EFI OS is
  // selected afterwards. Do not reset retry_count or change the other slot.
  Slot->Attributes |= SLOT_SUCCESSFUL;
  ((EFI_PARTITION_ENTRY *)OtherEntries)[mBootIndex].Attributes = Slot->Attributes;
  Status = gBS->CalculateCrc32(Entries, Bytes, &Crc);
  if (EFI_ERROR(Status)) goto Done;
  Primary->PartitionEntryArrayCRC32 = Backup->PartitionEntryArrayCRC32 = Crc;
  Primary->Header.CRC32 = Backup->Header.CRC32 = 0;
  Status = gBS->CalculateCrc32(Primary, Primary->Header.HeaderSize, &Primary->Header.CRC32);
  if (EFI_ERROR(Status)) goto Done;
  Status = gBS->CalculateCrc32(Backup, Backup->Header.HeaderSize, &Backup->Header.CRC32);
  if (EFI_ERROR(Status)) goto Done;

  // Keep the primary intact until the new backup is flushed and verified.
  // GPT has no atomic two-copy update; stop on error, never rebuild a table
  // or touch partition contents in an attempt to repair it here.
  Status = WriteAndVerify(Io, MediaId, Backup->PartitionEntryLBA, AlignedBytes, OtherEntries, Verify);
  if (EFI_ERROR(Status)) goto Done;
  Status = WriteAndVerify(Io, MediaId, Backup->MyLBA, 4096, BackupBlock, Verify);
  if (EFI_ERROR(Status)) goto Done;
  Status = WriteAndVerify(Io, MediaId, Primary->PartitionEntryLBA, AlignedBytes, Entries, Verify);
  if (EFI_ERROR(Status)) goto Done;
  Status = WriteAndVerify(Io, MediaId, Primary->MyLBA, 4096, PrimaryBlock, Verify);
Done:
  if (EFI_ERROR(Status))
    DEBUG((DEBUG_ERROR, "Lenovo Elden: boot-slot acknowledgement failed: %r\n", Status));
  gBS->FreePool(Allocation);
}
