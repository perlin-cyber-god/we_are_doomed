#!/usr/bin/env python3
"""Deterministic machine-code programs, not expected execution results.
The unchanged C CPU creates the expected retirement traces separately.
"""
import json
import pathlib
import random
from make_decode_vectors import enc_i as I, enc_r as R, enc_s as S, enc_b as B, enc_j as J

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'build/core'
EBREAK = 0x00100073


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cases = []

    def emit(name, words, *, data=None, ram=2048, budget=1000, reset=0, expect=9):
        image = bytearray([0xa5] * 2048)
        for address, word in (words.items() if isinstance(words, dict) else enumerate(words)):
            if not isinstance(words, dict): address *= 4
            assert 0 <= address <= 2044 and address % 4 == 0
            image[address:address+4] = (word & 0xffffffff).to_bytes(4, 'little')
        for address, value in (data or {}).items():
            image[address:address+4] = value.to_bytes(4, 'little')
        (OUT / f'{name}.bin').write_bytes(image)
        (OUT / f'{name}.mem').write_text(''.join(f'{byte:02x}\n' for byte in image))
        cases.append(dict(name=name, ram=ram, budget=budget, reset=reset, expect=expect))

    assert I(0x13,6,0,0,40) == 0x02800313
    assert I(0x13,6,0,6,-1) == 0xfff30313
    assert S(2,5,6,0) == 0x0062a023
    emit('demo', [0x20000293,0x0002a303,0xfff30313,0x0062a023,EBREAK],data={512:40})
    emit('loop', [I(0x13,5,0,0,512),I(3,6,2,5,0),I(0x13,6,0,6,-1),S(2,5,6,0),B(1,6,0,-12),EBREAK],data={512:3})
    words=[I(0x13,1,0,0,1),I(0x13,2,0,0,-1),0x800001b7,I(0x13,4,0,0,63)]
    for f7,f3 in [(0,0),(32,0),(0,1),(0,2),(0,3),(0,4),(0,5),(32,5),(0,6),(0,7)]:
        for a,b in [(1,2),(2,1),(3,4)]: words.append(R(6,f3,a,b,f7))
    for f3 in [0,2,3,4,6,7]:
        for imm in [-2048,-1,0,2047]: words.append(I(0x13,7,f3,3,imm))
    for shift in range(32):
        for f3,top in [(1,0),(5,0),(5,1024)]: words.append(I(0x13,8,f3,3,top+shift))
    words += [0xfffff4b7,0x80000517,I(0x13,0,0,1,7),EBREAK]
    emit('arithmetic',words)
    rng=random.Random(0x32a3)
    words=[I(0x13,n,0,0,rng.randrange(-2048,2048)) for n in range(1,32)]
    for _ in range(200):
        rd,a,b=[rng.randrange(32) for _ in range(3)]
        if rng.randrange(2):
            f7,f3=rng.choice([(0,0),(32,0),(0,1),(0,2),(0,3),(0,4),(0,5),(32,5),(0,6),(0,7)])
            words.append(R(rd,f3,a,b,f7))
        else: words.append(I(0x13,rd,rng.choice([0,2,3,4,6,7]),a,rng.randrange(-2048,2048)))
    emit('random_arithmetic',words+[EBREAK])
    words=[I(0x13,5,0,0,1536),0x80ff8137,I(0x13,2,0,2,127),S(2,5,2,0)]
    for f3 in [0,4]:
        for offset in range(4): words.append(I(3,6,f3,5,offset))
    for f3 in [1,5]:
        for offset in [0,2]: words.append(I(3,7,f3,5,offset))
    words += [I(3,8,2,5,0),I(3,0,0,5,3)]
    for offset in range(4): words += [S(0,5,2,offset),I(3,9,2,5,0)]
    for offset in [0,2]: words += [S(1,5,2,offset),I(3,9,2,5,0)]
    words += [I(0x13,5,0,5,4),S(2,5,2,-4),I(3,10,2,5,-4),EBREAK]
    emit('memory_widths',words)
    words=[I(0x13,1,0,0,-1),I(0x13,2,0,0,1)]
    # Each condition is exercised true and false; skipped increments reveal errors.
    for f3,a,b,c,d in [(0,1,1,1,2),(1,1,2,1,1),(4,1,2,2,1),
                        (5,2,1,1,2),(6,2,1,1,2),(7,1,2,2,1)]:
        words += [B(f3,a,b,8),I(0x13,20,0,20,1),B(f3,c,d,8),I(0x13,21,0,21,1)]
    emit('branches',words+[EBREAK])
    emit('nested_calls',{0:I(0x13,2,0,0,2016),4:J(1,28),8:I(0x13,20,0,0,123),12:EBREAK,
         32:I(0x13,2,0,2,-16),36:S(2,2,1,12),40:I(0x13,10,0,0,40),44:J(1,36),
         48:I(0x13,10,0,10,1),52:I(3,1,2,2,12),56:I(0x13,2,0,2,16),60:I(0x67,0,0,1,0),
         80:I(0x13,10,0,10,1),84:I(0x67,0,0,1,0)})
    emit('jalr_alias',{0:I(0x13,1,0,0,17),4:I(0x67,1,0,1,0),16:I(0x13,2,0,1,0),20:EBREAK})
    emit('fences',[0x0000000f,0x0ff0000f,0xffff8f8f,EBREAK])
    for name,word in [('mul',0x02000033),('shift_reserved',0x02001013),('csr',0x30001073),
                      ('fence_i',0x0000100f),('system_reserved',0x00200073),('zero_word',0)]:
        emit(name,[word],expect=3)
    emit('ecall',[0x00000073],expect=8)
    emit('ebreak',[EBREAK])
    for name,word,expected in [('lw_misaligned',I(3,1,2,0,1),4),('lh_misaligned',I(3,1,1,0,1),4),
                                ('sw_misaligned',S(2,0,1,2),6),('sh_misaligned',S(1,0,1,1),6)]:
        emit(name,[I(0x13,1,0,0,99),word],expect=expected)
    emit('load_access',[I(0x13,5,0,0,-4),I(3,1,2,5,0)],expect=5)
    emit('load_x0_access',[I(0x13,5,0,0,-4),I(3,0,2,5,0)],expect=5)
    emit('store_access',[I(0x13,5,0,0,-4),S(2,5,1,0)],expect=7)
    emit('fetch_access',[J(1,2048)],expect=2)
    emit('jal_misaligned',[J(1,2)],expect=1)
    emit('jalr_misaligned',[I(0x13,1,0,0,3),I(0x67,1,0,1,0)],expect=1)
    emit('branch_misaligned',[B(0,0,0,2)],expect=1)
    emit('branch_untaken_misaligned',[B(1,0,0,2),EBREAK])
    for name,word,expected in [('partial_sh_fault',S(1,5,1,0),7),('partial_lh_fault',I(3,1,1,5,0),5),
                                ('partial_sb_ok',S(0,5,1,0),9),('partial_lb_ok',I(3,1,0,5,0),9)]:
        emit(name,[I(0x13,5,0,0,2046),I(0x13,1,0,0,71),word,EBREAK],ram=2047,expect=expected)
    emit('reset_pc_misaligned',[EBREAK],reset=2,expect=1)
    emit('fetch_partial',[EBREAK],ram=2047,reset=2044,expect=2)
    emit('infinite_prefix',[J(0,0)],budget=20,expect='budget')
    (OUT/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')
    print(f'Generated {len(cases)} bounded programs (including expected faults); seed=0x32a3')


if __name__=='__main__': main()
