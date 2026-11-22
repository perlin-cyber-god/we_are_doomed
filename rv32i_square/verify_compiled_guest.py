#!/usr/bin/env python3
"""Run the actual compiled guest plus budget and data/startup fault checks.

Uses only Python's standard library. ELF inspection is for test symbol lookup;
the processor runner still loads a flat binary and interprets every opcode.
"""
import pathlib
import struct
import subprocess
import sys
import tempfile


def symbols(elf):
    data = elf.read_bytes()
    assert data[:7] == b"\x7fELF\x01\x01\x01", "expected ELF32 little-endian"
    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    assert header[1] == 243 and header[3] == 0, "RISC-V entry must be zero"
    assert header[6] == 0, "unexpected compressed or floating-point ABI flags"
    offset, stride, count = header[5], header[10], header[11]
    sections = [struct.unpack_from("<10I", data, offset + i * stride) for i in range(count)]
    result = {}
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for pos in range(section[4], section[4] + section[5], section[9]):
            name, value, _, _, _, _ = struct.unpack_from("<IIIBBH", data, pos)
            label = names[name:names.find(b"\0", name)].decode()
            result[label] = value
    assert any(s[1] == 8 and s[5] >= 32 for s in sections), "missing NOBITS BSS"
    return result


def run(image, budget=1000000):
    return subprocess.run(["./build/rv32-run", str(image), "65536", str(budget)],
                          text=True, capture_output=True)


def main():
    image = pathlib.Path(sys.argv[1])
    elf = pathlib.Path(sys.argv[2])
    sym = symbols(elf)
    for name in ("guest_main", "outer", "inner", "initialized_global"):
        assert name in sym, f"missing actual linked function/global: {name}"
    normal = run(image)
    assert normal.returncode == 0, normal.stdout + normal.stderr
    assert "checksum=90 count=6 initialized=7 BSS_before=0 nested_calls=6" in normal.stdout
    print(normal.stdout, end="")
    # Read the actual retired count, rather than assuming it is always 565.
    retired = int(normal.stdout.split("retired=", 1)[1].split()[0])
    exact = run(image, retired)
    short = run(image, retired - 1)
    assert exact.returncode == 0 and short.returncode == 1
    assert "run status 4" in short.stderr, short.stderr  # RV_RUN_BUDGET
    print(f"PASS real guest exact budget={retired}; budget={retired - 1} fails")

    raw = image.read_bytes()
    with tempfile.TemporaryDirectory(dir="build", prefix="guest-negative-") as tmp:
        modified = bytearray(raw)
        address = sym["initialized_global"]
        assert struct.unpack_from("<I", modified, address)[0] == 7
        struct.pack_into("<I", modified, address, 9)
        other = pathlib.Path(tmp) / "changed-data.bin"
        other.write_bytes(modified)
        changed = run(other)
        assert changed.returncode == 1
        assert "checksum=102 count=6 initialized=9 BSS_before=0 nested_calls=6" in changed.stdout
        assert "FAIL compiled guest signature" in changed.stdout
        print("PASS changing guest data 7 -> 9 computes checksum 102; host rejects mismatch")

        # Skip the real startup BSS store but retain its pointer/loop control.
        modified = bytearray(raw)
        stores = [i for i in range(0, sym["inner"], 4)
                  if struct.unpack_from("<I", modified, i)[0] == 0x0002a023]
        assert len(stores) == 1, "expected exactly one sw zero,0(t0) in startup"
        struct.pack_into("<I", modified, stores[0], 0x00000013)  # ADDI x0,x0,0
        other = pathlib.Path(tmp) / "missing-bss-clear.bin"
        other.write_bytes(modified)
        broken = run(other)
        assert broken.returncode == 1 and "run status 1" in broken.stderr
        print("PASS missing guest BSS clear produces guest FAIL with poisoned host RAM")


if __name__ == "__main__":
    main()
