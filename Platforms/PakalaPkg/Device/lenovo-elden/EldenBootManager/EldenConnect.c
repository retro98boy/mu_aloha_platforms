/** @file
  Connect initialized storage and display controllers during BDS.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Uefi/UefiGpt.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
extern EFI_GUID gEldenUfsReadyProtocolGuid;

STATIC BOOLEAN mEldenFixedMediaGptValidated = FALSE;
VOID EldenRememberBootSlot (EFI_HANDLE Handle, EFI_BLOCK_IO_PROTOCOL *Io,
                           EFI_PARTITION_TABLE_HEADER *Primary,
                           EFI_PARTITION_TABLE_HEADER *Backup,
                           EFI_PARTITION_ENTRY *Entries);

BOOLEAN
EldenFixedMediaGptIsValid (VOID)
{
  return mEldenFixedMediaGptValidated;
}

// Connect the SD parent and its partition children without probing files.
STATIC VOID
EldenConnectSdFilesystems (EFI_HANDLE Parent)
{
  EFI_GUID PathGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
  EFI_GUID BlockGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
  EFI_DEVICE_PATH_PROTOCOL *Path, *Candidate;
  EFI_HANDLE *List = NULL;
  EFI_STATUS Status;
  UINTN Prefix, Count = 0, I, Matches = 0;

  Status = gBS->HandleProtocol(Parent, &PathGuid, (VOID **)&Path);
  if (EFI_ERROR(Status) || !IsDevicePathValid(Path, 4096)) return;
  Prefix = GetDevicePathSize(Path);
  if (Prefix <= END_DEVICE_PATH_LENGTH) return;
  Prefix -= END_DEVICE_PATH_LENGTH;
  gBS->ConnectController(Parent, NULL, NULL, FALSE);
  Status = gBS->LocateHandleBuffer(ByProtocol, &BlockGuid, NULL, &Count, &List);
  if (!EFI_ERROR(Status) && Count <= 512) {
    for (I = 0; I < Count; I++) {
      if (List[I] == Parent) continue;
      Status = gBS->HandleProtocol(List[I], &PathGuid, (VOID **)&Candidate);
      if (EFI_ERROR(Status) || !IsDevicePathValid(Candidate, 4096) ||
          GetDevicePathSize(Candidate) < Prefix + END_DEVICE_PATH_LENGTH ||
          CompareMem(Path, Candidate, Prefix) != 0) continue;
      if (++Matches > 32) break;
      gBS->ConnectController(List[I], NULL, NULL, FALSE);
    }
  }
  if (List != NULL) gBS->FreePool(List);
}

STATIC VOID
EldenPrepareUi (VOID)
{
  EFI_STATUS S;
  EFI_HANDLE *Displays = NULL;
  UINTN Count = 0, I;

  // GopOverrideDxe owns protocol replacement. Only connect its handles here;
  // RenderingEngineDxe publishes the standard GOP used by the console.
  gDS->Dispatch();
  S = gBS->LocateHandleBuffer(ByProtocol,
        (EFI_GUID *)PcdGetPtr(PcdMsGopOverrideProtocolGuid), NULL, &Count, &Displays);
  if (!EFI_ERROR(S) && Count <= 4) {
    for (I = 0; I < Count; I++)
      gBS->ConnectController(Displays[I], NULL, NULL, FALSE);
  }
  if (Displays != NULL) gBS->FreePool(Displays);
  {
    // WindowManager was published by the late GOP connection, after the
    // previous dispatch pass. Re-evaluate real DEPEX; do not bypass it by
    // manually loading DisplayEngine. USB power entry remains locked.
    EFI_GUID Engine = {0x9bbe29e9,0xfda1,0x41ec,{0xad,0x52,0x45,0x22,0x13,0x74,0x2d,0x2e}};
    EFI_GUID Osk = {0x3c4ca20d,0xc95a,0x4b8b,{0x81,0xaf,0x94,0xa9,0x83,9,0x23,0xe2}};
    VOID *Interface;
    EFI_HANDLE *OskHandles = NULL;
    UINTN OskCount = 0, J;
    S = gDS->Dispatch();
    S = gBS->LocateProtocol(&Engine, NULL, &Interface);
    if (EFI_ERROR(S)) return;
    // OSK publishes its control interface before DriverBindingStart. Connect
    // only those software handles, not all hardware controllers.
    S = gBS->LocateHandleBuffer(ByProtocol, &Osk, NULL, &OskCount, &OskHandles);
    if (!EFI_ERROR(S) && OskCount <= 4) {
      for (J = 0; J < OskCount; J++) {
        S = gBS->ConnectController(OskHandles[J], NULL, NULL, FALSE);
      }
    }
    if (OskHandles != NULL) gBS->FreePool(OskHandles);
  }
}

STATIC UINTN
EldenSelectParents (EFI_HANDLE *All, UINTN Count, EFI_GUID *Guid, EFI_HANDLE *Parents)
{
  UINTN I, N = 0;
  EFI_BLOCK_IO_PROTOCOL *Io;
  if (Count > 512) return 0;
  for (I = 0; I < Count; I++) {
    Io = NULL;
    if (EFI_ERROR(gBS->HandleProtocol(All[I], Guid, (VOID **)&Io)) || Io == NULL || Io->Media == NULL) return 0;
    if (Io->Media->LogicalPartition || Io->Media->RemovableMedia || !Io->Media->MediaPresent) continue;
    if (N == 16) return 0;
    Parents[N++] = All[I];
  }
  return N;
}

STATIC VOID
EldenPartitionConnect (EFI_HANDLE *Handles, UINTN Count, EFI_GUID *BlockGuid)
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *Io;
  UINTN I;

  if (!mEldenFixedMediaGptValidated || Count > 16) return;
  for (I = 0; I < Count; I++) {
    Io = NULL;
    Status = gBS->HandleProtocol(Handles[I], BlockGuid, (VOID **)&Io);
    if (EFI_ERROR(Status) || Io == NULL || Io->Media == NULL || Io->Media->LogicalPartition) continue;
    gBS->ConnectController(Handles[I], NULL, NULL, FALSE);
  }
}

// Validate GPT metadata in memory without repairing or writing the medium.
STATIC BOOLEAN
EldenValidateGptHeader (VOID *Buffer, EFI_LBA Here, EFI_LBA Other,
                EFI_PARTITION_TABLE_HEADER *Out)
{
  EFI_PARTITION_TABLE_HEADER *H = Buffer;
  UINT32 Saved, Crc;
  EFI_STATUS S;
  if (H->Header.Signature != EFI_PTAB_HEADER_ID || H->Header.Revision != 0x10000 ||
      H->Header.HeaderSize != sizeof(*H) || H->Header.Reserved != 0 ||
      H->MyLBA != Here || H->AlternateLBA != Other ||
      H->FirstUsableLBA > H->LastUsableLBA ||
      H->SizeOfPartitionEntry != sizeof(EFI_PARTITION_ENTRY) ||
      H->NumberOfPartitionEntries == 0 || H->NumberOfPartitionEntries > 256) return FALSE;
  Saved = H->Header.CRC32;
  H->Header.CRC32 = 0;
  S = gBS->CalculateCrc32(Buffer, H->Header.HeaderSize, &Crc);
  H->Header.CRC32 = Saved;
  if (EFI_ERROR(S) || Crc != Saved) return FALSE;
  CopyMem(Out, H, sizeof(*Out));
  return TRUE;
}

// Check fixed UFS media before connecting partition drivers. Unpartitioned
// LUNs are skipped; any invalid GPT or read failure keeps the gate closed.
// Partition names, contents and write support are irrelevant to this check.
STATIC VOID
EldenValidateFixedMediaGpt (EFI_HANDLE *Handles, UINTN Count, EFI_GUID *Guid)
{
  VOID *Allocation = NULL;
  UINT8 *HeaderBuffer, *PrimaryEntries, *BackupEntries;
  EFI_BLOCK_IO_PROTOCOL *Io;
  EFI_PARTITION_TABLE_HEADER PrimaryHeader, BackupHeader;
  EFI_PARTITION_ENTRY *Entries;
  EFI_GUID ZeroGuid = {0};
  UINT32 Crc;
  UINTN I, J, K, EntryBytes, AlignedEntryBytes, EntryBlocks;
  EFI_STATUS Status;
  mEldenFixedMediaGptValidated = FALSE;
  if (Handles == NULL || Count == 0 || Count > 16) return;
  Status = gBS->AllocatePool(EfiBootServicesData, 4 * 65536, &Allocation);
  if (EFI_ERROR(Status)) return;
  HeaderBuffer = (UINT8 *)(((UINTN)Allocation + 65535) & ~(UINTN)65535);
  PrimaryEntries = HeaderBuffer + 65536; BackupEntries = PrimaryEntries + 65536;
  for (I = 0; I < Count; I++) {
    Io = NULL;
    Status = gBS->HandleProtocol(Handles[I], Guid, (VOID **)&Io);
    if (EFI_ERROR(Status) || Io == NULL || Io->Media == NULL || Io->ReadBlocks == NULL) goto Reject;
    if (Io->Media->LogicalPartition || Io->Media->RemovableMedia || !Io->Media->MediaPresent) continue;
    if (Io->Media->BlockSize != 4096 || Io->Media->LastBlock < 16 ||
        Io->Media->IoAlign > 65536 ||
        (Io->Media->IoAlign > 1 && (Io->Media->IoAlign & (Io->Media->IoAlign - 1)) != 0)) goto Reject;
    Status = Io->ReadBlocks(Io, Io->Media->MediaId, 1, 4096, HeaderBuffer);
    if (EFI_ERROR(Status)) goto Reject;
    if (((EFI_PARTITION_TABLE_HEADER *)HeaderBuffer)->Header.Signature != EFI_PTAB_HEADER_ID) {
      continue;
    }
    if (!EldenValidateGptHeader(HeaderBuffer, 1, Io->Media->LastBlock, &PrimaryHeader)) goto Reject;
    EntryBytes = PrimaryHeader.NumberOfPartitionEntries * sizeof(EFI_PARTITION_ENTRY);
    AlignedEntryBytes = (EntryBytes + 4095) & ~(UINTN)4095; EntryBlocks = AlignedEntryBytes / 4096;
    if (PrimaryHeader.PartitionEntryLBA != 2 || PrimaryHeader.FirstUsableLBA < 2 + EntryBlocks ||
        PrimaryHeader.LastUsableLBA >= Io->Media->LastBlock - EntryBlocks) goto Reject;
    Status = Io->ReadBlocks(Io, Io->Media->MediaId, 2, AlignedEntryBytes, PrimaryEntries);
    if (EFI_ERROR(Status)) goto Reject;
    Status = gBS->CalculateCrc32(PrimaryEntries, EntryBytes, &Crc);
    if (EFI_ERROR(Status) || Crc != PrimaryHeader.PartitionEntryArrayCRC32) goto Reject;
    Status = Io->ReadBlocks(Io, Io->Media->MediaId, Io->Media->LastBlock, 4096, HeaderBuffer);
    if (EFI_ERROR(Status) || !EldenValidateGptHeader(HeaderBuffer, Io->Media->LastBlock, 1, &BackupHeader)) goto Reject;
    if (BackupHeader.FirstUsableLBA != PrimaryHeader.FirstUsableLBA || BackupHeader.LastUsableLBA != PrimaryHeader.LastUsableLBA ||
        CompareMem(&BackupHeader.DiskGUID, &PrimaryHeader.DiskGUID, sizeof(EFI_GUID)) != 0 ||
        BackupHeader.NumberOfPartitionEntries != PrimaryHeader.NumberOfPartitionEntries ||
        BackupHeader.PartitionEntryArrayCRC32 != PrimaryHeader.PartitionEntryArrayCRC32 ||
        BackupHeader.PartitionEntryLBA <= PrimaryHeader.LastUsableLBA ||
        BackupHeader.PartitionEntryLBA > Io->Media->LastBlock - EntryBlocks) goto Reject;
    Status = Io->ReadBlocks(Io, Io->Media->MediaId, BackupHeader.PartitionEntryLBA, AlignedEntryBytes, BackupEntries);
    if (EFI_ERROR(Status) || CompareMem(PrimaryEntries, BackupEntries, EntryBytes) != 0) goto Reject;
    Entries = (EFI_PARTITION_ENTRY *)PrimaryEntries;
    for (J = 0; J < PrimaryHeader.NumberOfPartitionEntries; J++) {
      if (CompareMem(&Entries[J].PartitionTypeGUID, &ZeroGuid, sizeof(ZeroGuid)) == 0) continue;
      if (Entries[J].StartingLBA < PrimaryHeader.FirstUsableLBA || Entries[J].EndingLBA > PrimaryHeader.LastUsableLBA ||
          Entries[J].StartingLBA > Entries[J].EndingLBA) goto Reject;
      for (K = 0; K < J; K++) {
        if (CompareMem(&Entries[K].PartitionTypeGUID, &ZeroGuid, sizeof(ZeroGuid)) == 0) continue;
        if (Entries[J].StartingLBA <= Entries[K].EndingLBA && Entries[K].StartingLBA <= Entries[J].EndingLBA) goto Reject;
      }
    }
    EldenRememberBootSlot(Handles[I], Io, &PrimaryHeader, &BackupHeader, Entries);
  }
  mEldenFixedMediaGptValidated = TRUE;
Reject:
  gBS->FreePool(Allocation);
}

VOID
EldenConnectDevices (VOID)
{
  EFI_GUID BlockIoGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
  EFI_HANDLE *Handles = NULL;
  UINTN HandleCount = 0;
  EFI_STATUS Status;
  VOID *Ready;
  Status = gBS->LocateHandleBuffer(ByProtocol, &BlockIoGuid, NULL, &HandleCount, &Handles);
  if (!EFI_ERROR(Status) && Handles != NULL) {
    EFI_HANDLE Parents[16];
    UINTN ParentCount = EldenSelectParents(Handles, HandleCount, &BlockIoGuid, Parents);
    mEldenFixedMediaGptValidated = FALSE;
    if (ParentCount != 0 &&
        !EFI_ERROR(gBS->LocateProtocol(&gEldenUfsReadyProtocolGuid, NULL, &Ready))) {
      EldenValidateFixedMediaGpt(Parents, ParentCount, &BlockIoGuid);
    }
    EldenPartitionConnect(Parents, ParentCount, &BlockIoGuid);
  } else {
    mEldenFixedMediaGptValidated = FALSE;
    EldenPartitionConnect(NULL, 0, &BlockIoGuid);
  }
  if (Handles != NULL) gBS->FreePool(Handles);

  {
    EFI_GUID Block = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_HANDLE *Handles = NULL;
    UINTN Count = 0, I;
    EFI_STATUS S;
    EFI_BLOCK_IO_PROTOCOL *Io;
    S = gBS->LocateHandleBuffer(ByProtocol, &Block, NULL, &Count, &Handles);
    if (!EFI_ERROR(S) && Count <= 512) {
      for (I = 0; I < Count; I++) {
        Io = NULL;
        if (EFI_ERROR(gBS->HandleProtocol(Handles[I], &Block, (VOID **)&Io)) ||
            Io == NULL || Io->Media == NULL || Io->Media->LogicalPartition ||
            !Io->Media->RemovableMedia) continue;
        if (Io->Media->MediaPresent) EldenConnectSdFilesystems(Handles[I]);
      }
    }
    if (Handles != NULL) gBS->FreePool(Handles);

  }
  EldenPrepareUi();
}
