"""Check target-specific conditional attach bypass; not hardware DMA proof."""
import runpy
import struct
from pathlib import Path

ns = runpy.run_path(str(Path(__file__).with_name('prepare-elden-ufs.py')))
data = ns['patched']()
assert data == ns['patched']()
word = struct.unpack_from('<I', data, 0x1b170)[0]
assert word & 0xff00001f == 0x34000000  # CBZ w0, not unconditional B
target = 0x1b170 + ((word >> 5) & 0x7ffff) * 4
assert target == 0x1b1d4
assert 0x1b170 + 4 == 0x1b174  # nonzero falls through to original error path
source = ns['SOURCE'].read_bytes()
assert data[0x1b174:0x1b18c] == source[0x1b174:0x1b18c]
assert data[0x1b1d4:0x1b1e0] == source[0x1b1d4:0x1b1e0]
assert len(data) == len(source)
assert b'.etrace' not in data
assert struct.unpack_from('<I', data, 0x10e48)[0] == 0xb82a6928
assert b'GATE_REQ_' not in data
assert b'SUBMIT_SYNC_' not in data
assert struct.unpack_from('<I', data, 0x137bc)[0] == 0xb6f80180
assert struct.unpack_from('<I', data, 0x137c0)[0] == 0x1400001b
assert data[0x137ec:0x13838] == source[0x137ec:0x13838]
print('PASS: conditional skip to success, configure failure preserved, deterministic PE')
