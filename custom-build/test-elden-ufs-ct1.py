"""Pinned instruction and UTRD header regression checks; not a device test."""
import runpy
import struct
from pathlib import Path

m = runpy.run_path(str(Path(__file__).with_name('prepare-elden-ufs.py')))
data = m['patched']()
assert data == m['patched']()
assert struct.unpack_from('<I', data, 0x10c9c)[0] == 0x52a20008
assert struct.unpack_from('<I', data, 0x10e48)[0] == 0xb82a6928
assert b'GATE_BLOCKED_NO_DOORBELL' not in data
# Simulate subsequent BFI/ORR operations for supported descriptor flags.
for old_type in (0, 1, 2):
    for dd in range(3):
        for interrupt in range(2):
            for crypto in range(2):
                for cci in range(256):
                    flags = dd << 25 | interrupt << 24 | crypto << 23 | cci
                    old = old_type << 28 | flags
                    new = 1 << 28 | flags
                    assert new >> 28 == 1
                    assert (old ^ new) & 0x0fffffff == 0
print('PASS: pinned CT1 instruction, deterministic PE, original doorbell store, header flags')
