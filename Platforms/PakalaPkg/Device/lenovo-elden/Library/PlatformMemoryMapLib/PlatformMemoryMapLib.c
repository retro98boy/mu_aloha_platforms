// SPDX-License-Identifier: BSD-2-Clause-Patent
// Elden V1. Source: xbl_config.elf b36b854e..., FDT at 0x80dc.
// Stock attributes translated using local ArmLib and reference Aloha maps.
// Must be paired with the device-local MemoryInitPeiLib, not the common one.
#include <Library/PlatformMemoryMapLib.h>

static ARM_MEMORY_REGION_DESCRIPTOR_EX mRegions[] = {
  { "NOMAP", 0x80000000, 0x01960000, NoHob, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "RSRV0", 0x81960000, 0x000A0000, AddMem, SYS_MEM, 0x703C07, Reserv, WRITE_BACK },
  { "XBL_DT", 0x81A00000, 0x00040000, AddMem, MEM_RES, 0x703C07, Reserv, WRITE_BACK },
  { "AOP", 0x81C00000, 0x000A0000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "UEFI Log", 0x81CE4000, 0x00010000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "SMEM", 0x81D00000, 0x00200000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "PDP_NS", 0x81F00000, 0x00100000, AddMem, MEM_RES, 0x400, Reserv, NS_DEVICE },
  { "PvmFw", 0x824A0000, 0x00100000, AddMem, SYS_MEM, 0x703C07, Reserv, WRITE_BACK },
  { "PIL Reserved", 0x89E00000, 0x19700000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "Global_SYNC", 0x82600000, 0x00100000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "Display_Demura", 0xA3500000, 0x02C80000, AddMem, MEM_RES, 0x703C07, Reserv, WRITE_THROUGH },
  { "FD Reserved I", 0xA7000000, 0x00400000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "FD Reserved II", 0xA7400000, 0x00200000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "CPU Vectors", 0xA7600000, 0x00001000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "Info Blk", 0xA7601000, 0x00001000, AddMem, SYS_MEM, 0x703C07, RtData, WRITE_BACK },
  { "MMU PageTables", 0xA7602000, 0x00003000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "Log Buffer", 0xA7605000, 0x00008000, AddMem, SYS_MEM, 0x703C07, RtData, WRITE_BACK },
  { "Stock UEFI Stack", 0xA760D000, 0x00040000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "SEC Heap", 0xA764D000, 0x0008C000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "Sched Heap", 0xA76D9000, 0x00400000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "FV Region", 0xA7AD9000, 0x00400000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "UEFI RESV", 0xA7ED9000, 0x00127000, AddMem, SYS_MEM, 0x703C07, BsData, WRITE_BACK },
  { "Kernel", 0xA8000000, 0x10000000, AddMem, SYS_MEM, 0x703C07, Reserv, WRITE_BACK },
  { "XBL_Ramdump", 0xB8000000, 0x001C0000, AddMem, MEM_RES, 0x400, Reserv, UNCACHED_UNBUFFERED },
  { "DBI_Dump", 0xB81C0000, 0x05770000, NoHob, MMAP_IO, 0x2, Conv, UNCACHED_UNBUFFERED },
  { "Display Reserved", 0xFC800000, 0x02B00000, AddMem, MEM_RES, 0x703C07, Reserv, WRITE_THROUGH },
  { "IMEM Base", 0x14680000, 0x0002A000, NoHob, MMAP_IO, 0x2, Conv, NS_DEVICE },
  { "IMEM Cookie Base", 0x146AA000, 0x00016000, AddDev, MMAP_IO, 0x2, Conv, NS_DEVICE },
  { "IPC_ROUTER_TOP", 0x00400000, 0x00100000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  // SM8750 GCC DT aperture 0x100000+0x1f4200, rounded to a page.
  // Stock UFSDxe full initialization directly accesses GCC_UFS_PHY_BCR.
  { "GCC", 0x00100000, 0x001F5000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "SECURITY_CONTROL", 0x00780000, 0x00007000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "QUP", 0x00800000, 0x00300000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "PRNG_CFG_PRNG", 0x010C0000, 0x0000C000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "CRYPTO0_CRYPTO", 0x01DC0000, 0x00040000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  // 9380: DAL sets attributes on the whole 256 KiB parent, not HCI alone.
  // Keep one contiguous GCD/MMU device range; probes still read four words only.
  { "UFS HWIO Parent", 0x01D80000, 0x00040000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "CLK_TCSR_TCSR_REGS", 0x01F00000, 0x00100000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "PERIPH_SS", 0x08800000, 0x00100000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "USB", 0x0A600000, 0x00200000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "AOSS", 0x0B000000, 0x04000000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "TLMM", 0x0F000000, 0x01000000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "SMMU", 0x15000000, 0x00200000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  { "APSS_HM", 0x16000000, 0x06000000, AddDev, MMAP_IO, 0x400, MmIO, NS_DEVICE },
  // Logical container for PrePi lookup ONLY; the Elden MemoryInitPeiLib skips it.
  { "DXE Heap", 0xBD930000, 0x1A2D0000, NoHob, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK },
  { "DXE Free", 0xBD930000, 0x1A290000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK },
  { "UEFI Stack", 0xD7BC0000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK },
  { "UEFI FD", 0xD7C00000, 0x00400000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK },
  // High-DRAM source: the user-provided ABL Linux capture
  // `abl-memory-20260923-125606.tar.gz`:
  // - `abl-final.dts`: the live /memory `reg` tuples provide bank boundaries.
  // - `system-memory.txt`: exported /proc/iomem and debugfs memblock maps
  //   cross-check the RAM banks and the reserved regions at the top of DRAM.
  //   MemTotal (15,249,636 kB) is a sanity check, not a boundary source.
  // That Linux boot is not EFI-backed (/sys/firmware/efi is absent); it
  // validates the physical map, not an ABL EFI memory map. Aloha boots Linux
  // through EFI, where usable RAM must be advertised in the UEFI HOB/map and
  // cannot be recovered from the DT /memory nodes alone. Keep physical holes
  // absent.
  { "RAM High Bank 0", 0x880000000, 0x02F700000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN },
  { "RAM High Bank 1", 0x8B0000000, 0x005500000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN },
  { "RAM High Bank 2", 0x8C0000000, 0x140000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN },
  // The final 0x132C0000 bytes are reserved by the ABL memory map. Reserve
  // the full contiguous tail conservatively, including its small gaps.
  { "RAM High Bank 3", 0xA00000000, 0x1ECD40000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN },
  { "DRAM Top Reserved", 0xBECD40000, 0x0132C0000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN },
  { "Terminator", 0, 0, 0, 0, 0, 0, 0 }
};

ARM_MEMORY_REGION_DESCRIPTOR_EX *GetPlatformMemoryMap (VOID)
{
  return mRegions;
}
