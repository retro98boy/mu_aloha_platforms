"""Instruction-level recovery tests. Run: uv run --with unicorn python this-file.

MMIO is a model, NOT a hardware/ownership/charging-safety certification.
Clock services and memcpy are mocked; the stock firmware-loader body executes.
"""
from pathlib import Path
import runpy
import struct
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.arm64_const import *

ROOT = Path(__file__).resolve().parents[1]
mod = runpy.run_path(str(ROOT/'custom-build/prepare-elden-i2c.py'))
data = mod['patched']()
assert data[0xb800:0xc000].count(data[0x6e68:0x6ea0])==1, 'loader frame must match'

def trial(reason=0, *, base=0xa94000, active=0, protocol=3, dma=0, dma_active=0, gsi=0,
          clock_on=1, clock_off=1, mirror=True, gpi_zero=False, late_busy=False):
    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    for address, length in ((0,0x10000), (0x100000,0x10000),
                            (0x200000,0x10000), (0xa00000,0x30000),
                            (0xa90000,0x10000)):
        u.mem_map(address,length)
    u.mem_write(0,data)
    def put(address,value,fmt='<I'): u.mem_write(address,struct.pack(fmt,value))
    put(0x200000,0x201000,'<Q')
    put(0x200018,base,'<Q')
    put(0x201018,0xa00000,'<Q')
    put(0xa00004,0x40000000)
    put(base+0x40,active)
    put(base+0x64,1)
    put(base+0x68,(protocol<<8)|4)
    put(base+0x258,dma)
    put(base+0xe40,dma_active)
    put(base+0xe18,gsi)
    put(base+0xe28,16<<16)
    put(base+0x228,16<<16)
    for n in range(5):
        for off in (0,0x60,0x80,0xe0):
            put(0xa10000+n*0x4000+off,0 if gpi_zero else 0x12300000+n*0x100+off)
    u.reg_write(UC_ARM64_REG_SP,0x10f000)
    saved = {}
    for i in range(19,31):
        reg=globals()[f'UC_ARM64_REG_X{i}']
        saved[reg]=0x200000 if i==20 else 0x12340000+i
        u.reg_write(reg,saved[reg])
    writes, calls, terminal, gpi_reads = [], [], [], []
    def read(uc,access,address,size,value,extra):
        if 0xa00000 <= address < 0xaa0000:
            assert base==0xa94000 and clock_on==1 and 0x5c14 in calls
        if 0xa10000 <= address < 0xa30000:
            assert size==4 and address in {0xa10000+n*0x4000+off for n in range(5) for off in (0,0x60,0x80,0xe0)}
            gpi_reads.append(address)
            if late_busy and len(gpi_reads)==20: put(base+0x40,1)
    def memory(uc, access, address, size, value, extra):
        if 0xa00000 <= address < 0xaa0000:
            writes.append((address,size,value))
            if address==base+0x2008 and mirror: put(base+0x64,value&1)
            if address==base+0x1000: put(base+0x68,value)
    def code(uc,address,size,extra):
        if address in (0x59f8,0x59c8):
            terminal.append(address); uc.emu_stop(); return
        assert address!=0x14ac, 'diagnostic logger must not execute'
        if address in (0x5c14,0x5cfc,0x1e84):
            calls.append(address)
            if address==0x1e84:
                dst,src,n=[uc.reg_read(r) for r in (UC_ARM64_REG_X0,UC_ARM64_REG_X1,UC_ARM64_REG_X2)]
                assert dst==base+0x1010 and n==343*4
                uc.mem_write(dst,bytes(uc.mem_read(src,n)))
            uc.reg_write(UC_ARM64_REG_W0,clock_on if address==0x5c14 else clock_off)
            uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_LR))
    u.hook_add(UC_HOOK_CODE,code)
    u.hook_add(UC_HOOK_MEM_WRITE,memory)
    u.hook_add(UC_HOOK_MEM_READ,read)
    u.emu_start(0xb800,0xffff,count=20000)
    assert terminal==[0x59f8 if reason==0 else 0x59c8]
    assert u.reg_read(UC_ARM64_REG_SP)==0x10f000
    assert all(u.reg_read(reg)==value for reg,value in saved.items())
    if reason in range(1,7): assert not writes, (reason,writes)
    if reason in (0,8,9):
        assert (base+0x2008,4,0) in writes
        assert bytes(u.mem_read(base+0x1010,343*4))==data[0xc1a8+28:0xc1a8+28+343*4]
    assert calls.count(0x5cfc)==(0 if reason in (1,2) else 1)
    expected = bool(gsi) and base==0xa94000 and clock_on and not active and protocol==3 and not dma_active
    assert len(gpi_reads)==(20 if expected else 0)

trial()
trial(1,base=0xa90000)
trial(2,clock_on=0)
trial(3,active=1)
trial(3,active=1<<12)
trial(4,protocol=1)
trial(dma=1)
trial(5,dma_active=1)
trial(5,dma_active=2)
trial(5,dma=1,dma_active=3)
trial(6,gsi=15)
trial(6,gsi=15,gpi_zero=True,dma=1)
trial(6,gsi=15,gpi_zero=True,late_busy=True)
trial(6,gsi=16,gpi_zero=True)
trial(8,mirror=False)
trial(9,clock_off=0)
print('PASS: 16 instruction-model cases; zero-context takeover disabled, loader and ABI')
