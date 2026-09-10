"""Execute the disabled redriver path with RAM mocks; not hardware proof."""
from pathlib import Path
import runpy
import struct
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import *
root=Path(__file__).resolve().parents[1]
data=runpy.run_path(str(root/'custom-build/prepare-elden-usb-probe.py'))['patched']()
for words in ((1,16,400,0,0,0),(1,5,0x4f,0xffffffff,1,2),(0,0,0,0,0,0)):
    u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
    u.mem_map(0,0x20000); u.mem_write(0,data)
    u.mem_map(0x200000,0x10000)
    u.mem_write(0x200018,struct.pack('<Q',0x201000))
    u.mem_write(0x201000,struct.pack('<6I',*words))
    saved={}
    for n in range(19,31):
        reg=globals()[f'UC_ARM64_REG_X{n}']; saved[reg]=0x200000 if n==20 else n
        u.reg_write(reg,saved[reg])
    u.reg_write(UC_ARM64_REG_SP,0x20f000)
    end=[]
    def code(uc,address,size,extra):
        assert address!=0xab58, 'I2C initialization must not execute'
        assert address!=0x22a8, 'diagnostic logger must not execute'
        if address==0xae94:
            end.append(address); uc.emu_stop()
    def write(*args): raise AssertionError('probe must not write memory')
    u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write)
    u.emu_start(0xae7c,0x13000,count=100)
    assert end==[0xae94]
    assert u.reg_read(UC_ARM64_REG_SP)==0x20f000
    assert all(u.reg_read(r)==v for r,v in saved.items() if r!=UC_ARM64_REG_X30)
print('PASS: USB probe has no log or I2C call, no writes, existing failure branch')

# Exercise the actual caller-visible feature gate, including its FALSE return.
# Both original flag values must avoid redriver init; no stack/MMIO writes.
for flag in (0, 1, 255):
    for mode in range(5):
        u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        u.mem_map(0,0x20000); u.mem_write(0,data)
        u.mem_write(0x14608,bytes([flag]))
        u.reg_write(UC_ARM64_REG_X0,0)
        u.reg_write(UC_ARM64_REG_X1,mode)
        u.reg_write(UC_ARM64_REG_LR,0x18000)
        u.reg_write(UC_ARM64_REG_SP,0x19000)
        seen=[]
        def gate_code(uc,address,size,extra):
            seen.append(address)
            assert address not in (0xaec0,0xad6c,0xab58,0xe288)
        u.hook_add(UC_HOOK_CODE,gate_code)
        u.hook_add(UC_HOOK_MEM_WRITE,write)
        u.emu_start(0xa1b4,0x18000,count=20)
        assert u.reg_read(UC_ARM64_REG_PC)==0x18000
        assert u.reg_read(UC_ARM64_REG_X0)==0
        assert u.reg_read(UC_ARM64_REG_SP)==0x19000
        assert seen==[0xa1b4,0xa1b8,0xa1bc,0xa1c4,0xa1c8]
print('PASS: 15 redriver feature-gate cases return FALSE with no device access')
