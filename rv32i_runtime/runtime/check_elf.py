#!/usr/bin/env python3
"""Static scan of executable ELF sections: no RVC widths or M opcodes.

The runtime integration still verifies dynamic execution separately.
All-zero alignment padding is permitted but is not an executable instruction.
"""
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
assert data[:7] == b"\x7fELF\x01\x01\x01"
h = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
assert h[1] == 243 and h[3] == 0 and h[6] == 0
allowed = {0x03, 0x0f, 0x13, 0x17, 0x23, 0x33, 0x37, 0x63, 0x67, 0x6f, 0x73}
words = 0
for i in range(h[11]):
    section = struct.unpack_from("<10I", data, h[5] + h[10] * i)
    if section[2] & 4 == 0:
        continue
    assert section[5] % 4 == 0
    for position in range(section[4], section[4] + section[5], 4):
        word = struct.unpack_from("<I", data, position)[0]
        if word == 0:
            continue
        assert word & 3 == 3, f"non-32-bit instruction at file offset {position}"
        assert word & 127 in allowed, f"unsupported instruction family {word:08x}"
        assert not (word & 127 == 0x33 and word >> 25 == 1), "M instruction detected"
        words += 1
print(f"PASS static ELF scan: {words} non-padding code words, base opcodes, no M/RVC")
