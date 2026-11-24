#!/usr/bin/env python3
"""Construct instructions from ISA cases; expected immediates are the ORIGINAL
constants, not values decoded back out using the RTL's extraction expressions.
"""
import argparse
import collections
import json
import pathlib
import random

ROOT = pathlib.Path(__file__).resolve().parent.parent
MASK = (1 << 32) - 1
R_OPS = {(0, 0): ("ADD", 0), (32, 0): ("SUB", 1),
         (0, 1): ("SLL", 5), (0, 2): ("SLT", 8), (0, 3): ("SLTU", 9),
         (0, 4): ("XOR", 4), (0, 5): ("SRL", 6), (32, 5): ("SRA", 7),
         (0, 6): ("OR", 3), (0, 7): ("AND", 2)}
I_OPS = {0: ("ADDI", 0), 2: ("SLTI", 8), 3: ("SLTIU", 9),
         4: ("XORI", 4), 6: ("ORI", 3), 7: ("ANDI", 2)}


def enc_i(op, rd, f3, rs1, imm):
    return ((imm & 4095) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op


def enc_r(rd, f3, rs1, rs2, f7):
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33


def enc_s(f3, rs1, rs2, imm):
    bits = imm & 4095
    return ((bits >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((bits & 31) << 7) | 0x23


def enc_b(f3, rs1, rs2, imm):
    bits = imm & 8191
    return (((bits >> 12) & 1) << 31) | (((bits >> 5) & 63) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (((bits >> 1) & 15) << 8) | (((bits >> 11) & 1) << 7) | 0x63


def enc_j(rd, imm):
    bits = imm & ((1 << 21) - 1)
    return (((bits >> 20) & 1) << 31) | (((bits >> 1) & 1023) << 21) | (((bits >> 11) & 1) << 20) | (((bits >> 12) & 255) << 12) | (rd << 7) | 0x6f


def build():
    records = []
    counts = collections.Counter()
    names = set()

    def emit(word, kind=0, *, imm=0, fmt=0, alu=0, ai=0, size=0, unsigned=0, branch=0, name="ILLEGAL"):
        # See SPEC.md for the packed checker layout. Raw register fields are
        # always visible; semantic controls for illegal words are all inactive.
        rs1, rs2, rd = (word >> 15) & 31, (word >> 20) & 31, (word >> 7) & 31
        use1 = int(kind in (1, 2, 3, 4, 6))
        use2 = int(kind in (3, 4) or (kind == 1 and (word & 127) == 0x33))
        write = int(kind in (1, 2, 5, 6, 7, 8))
        fields = [(kind, 4), (int(kind == 0), 1), (write, 1), (use1, 1),
                  (use2, 1), (ai, 1), (alu, 4), (size, 2), (unsigned, 1),
                  (branch, 3), (fmt, 3), (rs1, 5), (rs2, 5), (rd, 5)]
        packed = 0
        for value, width in fields:
            assert 0 <= value < (1 << width)
            packed = (packed << width) | value
        records.append(f"{word:08x} {packed:010x} {imm & MASK:08x}\n")
        counts[name] += 1
        if kind: names.add(name)

    # Literal anchors prevent a completely self-consistent broken encoder test.
    assert enc_i(0x13, 6, 0, 0, 40) == 0x02800313
    assert enc_i(0x13, 6, 0, 6, -1) == 0xfff30313
    assert enc_s(0, 2, 3, 0) == 0x00310023
    assert enc_b(1, 4, 0, -20) == 0xfe0216e3
    assert enc_j(1, 0) == 0x000000ef
    operands = [(0, 0, 0), (31, 31, 31), (6, 6, 6), (5, 31, 0), (0, 5, 7), (31, 1, 30)]
    edges = [-2048, -1024, -1, 0, 1, 1023, 2047]
    for (f7, f3), (name, alu) in R_OPS.items():
        for rd, rs1, rs2 in operands:
            emit(enc_r(rd, f3, rs1, rs2, f7), 1, alu=alu, name=name)
    for f3, (name, alu) in I_OPS.items():
        for value in edges:
            for rd, rs1, _ in operands:
                emit(enc_i(0x13, rd, f3, rs1, value), 1, imm=value, fmt=1, alu=alu, ai=1, name=name)
    for name, f3, f7, alu in [("SLLI", 1, 0, 5), ("SRLI", 5, 0, 6), ("SRAI", 5, 32, 7)]:
        for shift in range(32):
            for rd, rs1, _ in operands:
                emit(enc_i(0x13, rd, f3, rs1, (f7 << 5) | shift), 1,
                     imm=shift, fmt=6, alu=alu, ai=1, name=name)
    for f3, name, size, unsigned in [(0,"LB",0,0),(1,"LH",1,0),(2,"LW",2,0),(4,"LBU",0,1),(5,"LHU",1,1)]:
        for value in edges:
            for rd, rs1, _ in operands:
                emit(enc_i(3,rd,f3,rs1,value),2,imm=value,fmt=1,size=size,unsigned=unsigned,name=name)
    for f3, name in [(0,"SB"),(1,"SH"),(2,"SW")]:
        for value in edges:
            for _, rs1, rs2 in operands:
                emit(enc_s(f3,rs1,rs2,value),3,imm=value,fmt=2,size=f3,name=name)
    for f3, name in [(0,"BEQ"),(1,"BNE"),(4,"BLT"),(5,"BGE"),(6,"BLTU"),(7,"BGEU")]:
        for value in [-4096,-2048,-2,0,2,2048,4094]:
            for rs1, rs2 in [(0,0),(31,31),(6,0)]:
                emit(enc_b(f3,rs1,rs2,value),4,imm=value,fmt=3,branch=f3,name=name)
    # Every signed I/S immediate and every representable B offset.
    for value in range(-2048,2048):
        emit(enc_i(0x13,6,0,6,value),1,imm=value,fmt=1,ai=1,name="ADDI")
        emit(enc_s(2,5,6,value),3,imm=value,fmt=2,size=2,name="SW")
    for value in range(-4096,4096,2):
        emit(enc_b(1,6,0,value),4,imm=value,fmt=3,branch=1,name="BNE")
    # Bit-walking plus sign/range boundaries exercise U and J wiring.
    for upper in sorted({0,0x7ffff,0x80000,0xfffff}|{1<<i for i in range(20)}):
        for rd in (0,1,6,31):
            for opcode, kind, name in [(0x37,7,"LUI"),(0x17,8,"AUIPC")]:
                emit((upper<<12)|(rd<<7)|opcode,kind,imm=upper<<12,fmt=4,ai=1,name=name)
    offsets={-1048576,-1048574,-4096,-2048,-2,0,2,2048,4094,1048574}
    offsets |= {1<<i for i in range(1,20)} | {-(1<<i) for i in range(1,20)}
    for value in sorted(offsets):
        for rd in (0,1,6,31):emit(enc_j(rd,value),5,imm=value,fmt=5,name="JAL")
    for value in edges:
        for rd, rs1, _ in operands:
            emit(enc_i(0x67,rd,0,rs1,value),6,imm=value,fmt=1,name="JALR")
    # Exhaust funct7/funct3 validity for R words and shift-immediate upper bits.
    for f7 in range(128):
        for f3 in range(8):
            word=enc_r(6,f3,5,7,f7)
            if (f7,f3) in R_OPS:
                name,alu=R_OPS[f7,f3];emit(word,1,alu=alu,name=name)
            else:emit(word)
        for f3 in (1,5):
            for shift in range(32):
                word=enc_i(0x13,6,f3,5,(f7<<5)|shift)
                if (f3,f7) in [(1,0),(5,0),(5,32)]:
                    name,alu={(1,0):("SLLI",5),(5,0):("SRLI",6),(5,32):("SRAI",7)}[f3,f7]
                    emit(word,1,imm=shift,fmt=6,alu=alu,ai=1,name=name)
                else:emit(word)
    for f3 in (3,6,7): emit(enc_i(3,6,f3,5,-1))
    for f3 in range(3,8): emit(enc_s(f3,5,6,-1))
    for f3 in (2,3): emit(enc_b(f3,5,6,-2))
    for f3 in range(1,8): emit(enc_i(0x67,6,f3,5,-1))
    for imm in (0,0xff,0x800,0xfff):
        for rd,rs1,_ in operands:
            emit(enc_i(0x0f,rd,0,rs1,imm),9,name="FENCE")
    for f3 in range(1,8):emit(enc_i(0x0f,6,f3,5,0))
    emit(0x00000073,10,name="ECALL");emit(0x00100073,11,name="EBREAK")
    for word in [0x10500073,0x30200073,0x00001073,0xc0002073,0x001000f3,0x000000f3]:emit(word)
    for f3 in range(1,8):emit(enc_i(0x73,0,f3,0,0))
    rng=random.Random(0xDEC032)
    known={0x13,0x33,3,0x23,0x63,0x6f,0x67,0x37,0x17,0x0f,0x73}
    for opcode in range(128):
        if opcode not in known:
            for _ in range(4):emit((rng.getrandbits(25)<<7)|opcode)
    assert len(names)==40, names
    return "".join(records), {"rows":len(records),"instruction_names":sorted(names),"counts":dict(sorted(counts.items()))}


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--check",action="store_true");args=parser.parse_args()
    text,summary=build()
    files={ROOT/"vectors/decode_vectors.txt":text,
           ROOT/"vectors/decode_summary.json":json.dumps(summary,indent=2)+"\n"}
    for path,payload in files.items():
        if args.check:
            if path.read_text()!=payload:raise SystemExit(f"FAIL stale vector file {path}")
        else:path.write_text(payload)
    print(f"PASS {summary['rows']} decoder vectors; all 40 base instruction names covered")


if __name__=="__main__":main()
