#!/usr/bin/env python3
"""Verify the binary guest and its handling of changed/malformed game data."""
import pathlib
import struct
import subprocess
import tempfile


def fixture():
    greeting = b"HELLO!\0"
    numbers = bytes((3, 5, 7, 9))
    directory = 12 + len(greeting) + len(numbers)
    return (b"PWAD" + struct.pack("<II", 2, directory) + greeting + numbers
            + struct.pack("<II8s", 12, len(greeting), b"GREETING")
            + struct.pack("<II8s", 12 + len(greeting), len(numbers), b"NUMBERS"))


def run(path):
    return subprocess.run(["./build/runtime-run", "build/runtime.bin", str(path)],
                          capture_output=True, text=True, timeout=30)


def main():
    raw = fixture()
    path = pathlib.Path("runtime/fixture.wad")
    assert path.read_bytes() == raw, "fixture/source mismatch"
    normal = run(path)
    print(normal.stdout, end="")
    assert normal.returncode == 0, normal.stderr + normal.stdout
    assert "failures=0 asset_sum=24 heap_free=262128 diagnosed_invalid_frees=2" in normal.stdout
    with tempfile.TemporaryDirectory(dir="build", prefix="runtime-assets-") as tmp:
        for name, data in (("wrong-magic", b"XXXX" + raw[4:]),
                           ("truncated", raw[:9]),
                           ("bad-directory", raw[:8] + b"\xff" * 4 + raw[12:]),
                           ("changed-numbers", raw[:22] + bytes((10,)) + raw[23:])):
            bad = pathlib.Path(tmp) / name
            bad.write_bytes(data)
            result = run(bad)
            assert result.returncode == 1, result.stdout + result.stderr
            assert "run_status=1" in result.stdout, "guest should report FAIL, not CPU-fault"
            assert "trap " not in result.stderr
            if name == "changed-numbers":
                assert "asset_sum=25" in result.stdout
            print(f"PASS interpreted guest rejects {name}")


if __name__ == "__main__":
    main()
