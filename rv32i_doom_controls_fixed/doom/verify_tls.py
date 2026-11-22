#!/usr/bin/env python3
"""Regression for the real startup bug: omitted tp setup corrupted code at 0."""
import pathlib
import struct
import tempfile
from machine import Machine


def inspect(machine, expect_corruption):
    try:
        for _ in range(1000):
            machine.run(0, 0, 100000)
            actual = machine.lib.dh_boot_word(machine.pointer)
            if actual != machine.original_boot_word:
                assert expect_corruption, "correct startup overwrote its boot instruction"
                return
        assert not expect_corruption, "negative test did not exercise an errno write: " + machine.log[-200:]
    finally:
        machine.close()


def main():
    inspect(Machine(), False)
    raw = bytearray(pathlib.Path("build/doom/doom.bin").read_bytes())
    # The first three startup instructions pairs set sp, gp, tp. Check before
    # modifying the tp pair so a future startup rewrite fails this test clearly.
    a, b = struct.unpack_from("<II", raw, 16)
    assert a & 127 == 0x17 and (a >> 7) & 31 == 4
    assert b & 127 == 0x13 and (b >> 7) & 31 == 4
    struct.pack_into("<II", raw, 16, 0x13, 0x13)
    with tempfile.TemporaryDirectory(dir="build", prefix="missing-tp-") as tmp:
        path = pathlib.Path(tmp) / "guest.bin"
        path.write_bytes(raw)
        inspect(Machine(image=path), True)
    print("PASS TLS regression: correct tp preserves boot code; missing tp exposes errno corruption")


if __name__ == "__main__":
    main()
