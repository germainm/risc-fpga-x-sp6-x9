#!/usr/bin/env python3
"""bin -> hex 32 bits little-endian pour $readmemh (ROM 16 Ko)."""
import sys
data = open(sys.argv[1], 'rb').read()
size = int(sys.argv[3]) if len(sys.argv) > 3 else 16384
assert len(data) <= size, f"binaire trop gros: {len(data)} > {size}"
data += bytes(size - len(data))
with open(sys.argv[2], 'w') as f:
    for i in range(0, size, 4):
        w = data[i] | data[i+1] << 8 | data[i+2] << 16 | data[i+3] << 24
        f.write(f"{w:08x}\n")
