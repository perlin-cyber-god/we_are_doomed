#!/usr/bin/env python3
"""Deterministic golden ALU vectors, using Python integer arithmetic.

No HDL implementation is imported. Checked-in vectors also let Vivado GUI
users run without Python. --check verifies reproducibility without edits.
"""
import argparse
import pathlib
import random

MASK = (1 << 32) - 1
ROOT = pathlib.Path(__file__).resolve().parent.parent


def signed(value):
    return value if value < (1 << 31) else value - (1 << 32)


def expected(op, a, b):
    shift = b % 32
    operations = (
        lambda: a + b, lambda: a - b,
        lambda: a & b, lambda: a | b, lambda: a ^ b,
        lambda: a * (1 << shift), lambda: a // (1 << shift),
        lambda: signed(a) // (1 << shift),
        lambda: int(signed(a) < signed(b)), lambda: int(a < b),
    )
    value = operations[op]() & MASK if op < 10 else 0
    return value, int(op < 10), int(a == b), int(signed(a) < signed(b)), int(a < b)


def vectors():
    edges = (0, 1, 31, 32, 0x7fffffff, 0x80000000, 0xfffffffe, MASK)
    for a in edges:
        for b in edges:
            for op in range(10):
                yield op, a, b
    for a in edges:
        for shift in range(64):
            for op in (5, 6, 7):
                yield op, a, shift
    rng = random.Random(0x52563332)
    for _ in range(512):
        a, b = rng.getrandbits(32), rng.getrandbits(32)
        for op in range(10):
            yield op, a, b
    for op in range(10, 16):
        for a in edges:
            yield op, a, 0x80000001


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    rows = []
    for op, a, b in vectors():
        y, valid, eq, lts, ltu = expected(op, a, b)
        rows.append(f"{op:x} {a:08x} {b:08x} {y:08x} {valid:x} {eq:x} {lts:x} {ltu:x}\n")
    path = ROOT / "vectors" / "alu_vectors.txt"
    payload = "".join(rows)
    if args.check:
        if path.read_text() != payload:
            raise SystemExit("FAIL checked-in ALU vectors differ from their generator")
    else:
        path.write_text(payload)
    print(f"PASS {len(rows)} deterministic ALU vectors {'verified' if args.check else 'generated'}")


if __name__ == "__main__":
    main()
