"""Read-only ELF/SEFW bounds inspection; never loads or flashes firmware."""
import argparse
import hashlib
from pathlib import Path
import struct

def inspect(data):
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('expected ELF32 little-endian')
    offset = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    if size != 32 or not count or offset + size * count > len(data):
        raise ValueError('invalid program headers')
    segments = []
    for index in range(count):
        kind, start, _, _, length, _, flags, _ = struct.unpack_from('<8I', data, offset + index * size)
        if start + length > len(data):
            raise ValueError('truncated ELF segment')
        blob = data[start:start + length]
        if kind != 1 or not blob.startswith(b'SEFW'):
            continue
        if length < 28:
            raise ValueError('truncated SEFW header')
        _, version, core, protocol, fw, cfg, words, code, pairs, indices, values = struct.unpack_from('<III8H', blob)
        if version != 1:
            raise ValueError('unsupported SEFW version')
        for pos, amount in ((code, words * 4), (indices, pairs), (values, pairs * 4)):
            if pos < 28 or pos + amount > length:
                raise ValueError('SEFW array out of bounds')
        segments.append((protocol, core, fw, cfg, words, hashlib.sha256(blob).hexdigest()))
    if not segments:
        raise ValueError('no SEFW segments')
    return segments

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--compare', type=Path)
    args = parser.parse_args()
    data = args.image.read_bytes()
    segments = inspect(data)
    print('image SHA256', hashlib.sha256(data).hexdigest())
    for protocol, core, fw, cfg, words, digest in segments:
        print(f'protocol={protocol} core={core:#x} fw={fw:#x} cfg={cfg} words={words} sha256={digest}')
    if args.compare:
        print('SEFW segments identical:', segments == inspect(args.compare.read_bytes()))
