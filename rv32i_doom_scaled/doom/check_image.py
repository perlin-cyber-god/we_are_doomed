#!/usr/bin/env python3
"""Audit actual executable words, not only merged ELF ISA attributes."""
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
assert data[:7] == b"\x7fELF\x01\x01\x01"
h = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
assert h[1] == 243 and h[3] == 0 and h[6] == 0
allowed = {3, 15, 19, 23, 35, 51, 55, 99, 103, 111, 115}
count = 0
sections = [struct.unpack_from("<10I", data, h[5] + i * h[10]) for i in range(h[11])]
symbols = {}
for section in sections:
    if section[1] != 2:
        continue
    strings = sections[section[6]]
    names = data[strings[4]:strings[4] + strings[5]]
    for off in range(section[4], section[4] + section[5], section[9]):
        name, value, _, _, _, _ = struct.unpack_from("<IIIBBH", data, off)
        symbols[names[name:names.find(b"\0", name)].decode()] = value
tls = [struct.unpack_from("<8I", data, h[4] + i * h[8]) for i in range(h[9])]
tls = [p for p in tls if p[0] == 7]
assert len(tls) == 1 and tls[0][2] == symbols["__tls_base"], "TLS base differs from tp target"
assert symbols["__bss_start"] >= tls[0][2] + tls[0][4]
assert symbols["__tls_zero_end"] == tls[0][2] + tls[0][5]
for section in sections:
    if section[1] == 8 and section[2] & 0x400 == 0:
        assert section[3] >= symbols["__tls_zero_end"], "ordinary BSS overlaps TLS"
for i in range(h[11]):
    section = struct.unpack_from("<10I", data, h[5] + i * h[10])
    if section[2] & 4 == 0:
        continue
    assert section[5] % 4 == 0
    for offset in range(section[4], section[4] + section[5], 4):
        w = struct.unpack_from("<I", data, offset)[0]
        if w == 0:
            continue
        assert w & 3 == 3 and w & 127 in allowed, f"non-base width/opcode {w:08x}"
        assert not (w & 127 == 51 and w >> 25 == 1), "M instruction"
        assert w & 127 != 115 or w in (0x73, 0x100073), "CSR/privileged SYSTEM instruction"
        count += 1
print(f"PASS Doom executable scan: {count} code words; no M, C, CSR or floating-point opcodes")
