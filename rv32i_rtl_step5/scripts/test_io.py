#!/usr/bin/env python3
"""Run actual RTL first, then replay external MMIO inputs through the C oracle.
Timer correctness is checked independently on every simulated clock edge.
"""
import argparse
import json
import os
import pathlib
import re
import struct
import sys
import test_guest as common
from make_decode_vectors import enc_i as I,enc_s as S

ROOT=pathlib.Path(__file__).resolve().parent.parent
BUILD=ROOT/'build/io'
CAPACITY=16384
TEXT=b'Hello from our RV32I CPU!\n'
RTL=[*common.RTL,ROOT/'rtl/rv32_mmio.sv']
TOPS=['tb_mmio_unit','tb_mmio_unit_wait','tb_mmio_cpu','tb_mmio_cpu_wait']


def build_guest(prefix,name,opt):
    dest=BUILD/name;dest.mkdir(parents=True,exist_ok=True)
    common.run([prefix+'gcc',*common.FLAGS,opt,'-std=c11','-c','io_guest/guest.c','-o',dest/'guest.o'],dest/'compile-c.log')
    common.run([prefix+'gcc','-march=rv32i','-mabi=ilp32','-mno-relax','-c','io_guest/start.S','-o',dest/'start.o'],dest/'compile-start.log')
    common.run([prefix+'gcc','-march=rv32i','-mabi=ilp32','-nostdlib','-nostartfiles','-static',
                '-Wl,-T,io_guest/linker.ld','-Wl,--no-relax','-Wl,--build-id=none',
                f'-Wl,-Map,{dest}/guest.map',dest/'start.o',dest/'guest.o','-o',dest/'guest.elf'],dest/'link.log')
    common.run([prefix+'objcopy','-O','binary',dest/'guest.elf',dest/'guest.bin'],dest/'objcopy.log')
    output=common.run([prefix+'nm','-n',dest/'guest.elf'],dest/'symbols.log')
    symbols={m[2]:int(m[0],16) for line in output.splitlines() if len(m:=line.split())==3}
    required=['_start','_guest_stop','guest_main','console_putc','timer_read','bytes_sent','byte_sum',
              '__bss_start','__bss_end','__data_start','__data_end','__stack_top']
    assert all(n in symbols for n in required)
    dis=common.run([prefix+'objdump','-d','-S','-M','no-aliases',dest/'guest.elf'],dest/'disassembly.log')
    (dest/'guest.dis').write_text(dis)
    words=common.inspect_elf(dest/'guest.elf')
    (dest/'metadata.json').write_text(json.dumps(dict(symbols=symbols,code_words=words,opt=opt),indent=2)+'\n')
    image=bytearray([0xa5]*CAPACITY);binary=(dest/'guest.bin').read_bytes()
    assert 0<len(binary)<=symbols['__bss_start']<0x1800
    image[:len(binary)]=binary
    print(f'BUILT MMIO {name}: {words} RV32I code words; CPU unchanged',flush=True)
    return bytes(image),symbols


def compare_traces(actual,expected):
    a=actual.read_text().splitlines();b=expected.read_text().splitlines()
    assert len(a)==len(b),f'trace line count {len(a)} != {len(b)}'
    checks=0
    for line,(left,right) in enumerate(zip(a,b),1):
        aa,bb=left.split(),right.split()
        assert aa[0]==bb[0] and len(aa)==len(bb),f'trace shape at line{line}'
        for field,(x,y) in enumerate(zip(aa[1:],bb[1:]),1):
            checks+=1
            assert int(x,16)==int(y,16),f'line{line} field{field}: RTL {x} != C {y}'
    terminal=a[-2].split();memory=bytes(int(x,16) for x in a[-1].split()[1:])
    assert len(memory)==CAPACITY
    return checks,terminal,memory


def check_application(memory,terminal,symbols,initial,events,console):
    assert terminal[:2]==['T','9'] and int(terminal[2],16)==symbols['_guest_stop']
    sig=list(struct.unpack_from('<8I',memory,0x2000))
    assert sig[:3]==[common.DONE,len(TEXT),sum(TEXT)] and sig[7]==1,sig
    assert sig[6]==((sig[5]-sig[4])&0xffffffff) and 64<=sig[6]<10000,sig
    assert console==TEXT,(console,TEXT)
    timer=[int(f[5],16) for line in events.read_text().splitlines()
           if (f:=line.split())[1]=='10000010' and int(f[6],16)==0]
    assert len(timer)>=3 and [sig[3],sig[4],sig[5]]==[timer[0],timer[1],timer[-1]]
    assert struct.unpack_from('<I',memory,symbols['bytes_sent'])[0]==len(TEXT)
    assert struct.unpack_from('<I',memory,symbols['byte_sum'])[0]==sum(TEXT)
    assert int(terminal[8],16)==0x4000,'sp not restored' # T + 4 fields + marker + x0,x1,x2
    allowed=[(symbols['__data_start'],symbols['__data_end']),
             (symbols['__bss_start'],symbols['__bss_end']),(0x2000,0x2020),(0x3000,0x4000)]
    for i,(x,y) in enumerate(zip(memory,initial)):
        assert x==y or any(lo<=i<hi for lo,hi in allowed),f'guard/code changed at {i:x}'
    return sig


def main():
    parser=argparse.ArgumentParser();parser.add_argument('backend',choices=['iverilog','vivado']);args=parser.parse_args()
    BUILD.mkdir(parents=True,exist_ok=True)
    prefix=common.compiler_prefix()
    common.run([prefix+'gcc','--version'],BUILD/'cross-version.log')
    common.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
                '-Wconversion','-Wshadow','-O2','-Ireference','reference/rv32i.c','scripts/mmio_reference.c',
                '-o',BUILD/'mmio-reference'],BUILD/'compile-reference.log')
    normal=[(name,*build_guest(prefix,name,opt)) for name,opt in [('O0','-O0'),('O2','-O2')]]
    if args.backend=='iverilog':
        cmd=[os.environ.get('IVERILOG','iverilog')]
        if os.environ.get('IVERILOG_BASE'):cmd+=['-B',os.environ['IVERILOG_BASE']]
        for top in TOPS:
            tb='tb_mmio_unit.sv' if 'unit' in top else 'tb_mmio_cpu.sv'
            common.run([*cmd,'-g2012','-Wall','-s',top,'-o',BUILD/f'{top}.vvp',*RTL,ROOT/'tb'/tb],BUILD/f'{top}-compile.log')
        def simulate(top,options,log):
            return common.run([os.environ.get('VVP','vvp'),BUILD/f'{top}.vvp',*[f'+{x}' for x in options]],log)
    else:
        dest=ROOT/'build/vivado_io';dest.mkdir(exist_ok=True)
        common.run(['xvlog','-sv',*RTL,ROOT/'tb/tb_mmio_unit.sv',ROOT/'tb/tb_mmio_cpu.sv'],BUILD/'compile-rtl.log',dest)
        for top in TOPS:common.run(['xelab',f'work.{top}','-s',top,'-debug','typical'],BUILD/f'{top}-elaborate.log',dest)
        def simulate(top,options,log):
            cmd=['xsim',top,'-runall']
            for option in options:cmd+=['-testplusarg',option]
            return common.run(cmd,log,dest)
    for top in TOPS[:2]:
        print('RUN '+top,flush=True)
        out=simulate(top,[],BUILD/f'{top}.log');assert 'PASS MMIO_UNIT ' in out
        print(out.split('PASS MMIO_UNIT ',1)[1].splitlines()[0],flush=True)
    cases=[]
    for name,image,symbols in normal:
        for wait in (0,3):cases.append(dict(name=name,image=image,symbols=symbols,wait=wait,budget=10000,kind='normal',trap=9))
    base=[0x100002b7,I(0x13,6,0,0,65)] # LUI x5,0x10000; ADDI x6,x0,65.
    assert S(0,5,6,0)==0x00628023
    specs=[('tx_word',S(2,5,6,0),7),('tx_read',I(3,6,4,5,0),5),
           ('status_write',S(0,5,6,4),7),('timer_half',I(3,6,5,5,16),5),
           ('timer_write',S(2,5,6,16),7),('unknown_load',I(3,6,2,5,12),5),
           ('unknown_store',S(0,5,6,32),7),('fetch_io',I(0x67,0,0,5,0),2),
           ('misaligned_store',S(1,5,6,1),6),('load_x0_bad',I(3,0,4,5,0),5),
           ('load_x0_timer',I(3,0,2,5,16),9)]
    for name,word,trap in specs:
        image=bytearray([0xa5]*CAPACITY)
        words=base+[word,0x00100073]
        for i,w in enumerate(words):struct.pack_into('<I',image,i*4,w)
        cases.append(dict(name=name,image=bytes(image),wait=3,budget=10000,kind='fault',trap=trap))
    for name,value in [('ram_limit',0x4000),('high_address',0xfffffffc)]:
        image=bytearray([0xa5]*CAPACITY)
        words=([0x000042b7] if value==0x4000 else [I(0x13,5,0,0,-4)])+[I(3,6,2,5,0),0x100073]
        for i,w in enumerate(words):struct.pack_into('<I',image,i*4,w)
        cases.append(dict(name=name,image=bytes(image),wait=3,budget=10000,kind='fault',trap=5))
    cases.append(dict(name='budget_prefix',image=normal[0][1],wait=3,budget=20,kind='budget',trap=None))
    results=[]
    for case in cases:
        name=f"{case['name']}-wait{case['wait']}"
        (BUILD/f'{name}.load.bin').write_bytes(case['image'])
        (BUILD/f'{name}.mem').write_text(''.join(f'{b:02x}\n' for b in case['image']))
        print(f"RUN {name}: {case['kind']}",flush=True)
        top='tb_mmio_cpu' if case['wait']==0 else 'tb_mmio_cpu_wait'
        actual=BUILD/f'{name}.actual.trace';expected=BUILD/f'{name}.expected.trace';events=BUILD/f'{name}.events'
        console_path=BUILD/f'{name}.console'
        output=simulate(top,[f'IMAGE={BUILD}/{name}.mem',f'TRACE={actual}',f'EVENTS={events}',
                             f'CONSOLE={console_path}',f"MAX_STEPS={case['budget']}"],BUILD/f'{name}.log')
        m=re.search(r'PASS MMIO_CPU retired=(\d+) cycles=(\d+) tx=(\d+) io=(\d+) wait=(\d+)',output)
        assert m,'missing RTL run marker'
        common.run([BUILD/'mmio-reference',BUILD/f'{name}.load.bin',events,case['budget'],expected],BUILD/f'{name}-replay.log')
        checks,terminal,memory=compare_traces(actual,expected)
        console=console_path.read_bytes()
        signature=None
        if case['kind']=='normal':
            signature=check_application(memory,terminal,case['symbols'],case['image'],events,console)
            print('CONSOLE: '+console.decode('ascii').rstrip(),flush=True)
        elif case['kind']=='fault':
            assert terminal[0]=='T' and int(terminal[1],16)==case['trap'],terminal
            assert not console and memory==case['image'],'fault caused output/RAM side effect'
            if case['name']=='misaligned_store':assert events.read_text()=='','alignment fault reached device'
            if case['name']=='load_x0_timer':assert len(events.read_text().splitlines())==1,'x0 load was skipped'
        else:
            assert terminal[0]=='B' and 'BUDGET_PREFIX' in output
        result=dict(name=name,checks=checks,signature=signature,
                    **dict(zip(['retired','cycles','tx','io','wait'],map(int,m.groups()))))
        results.append(result)
        print(f'PASS {name}: C replay, registers, memory events and final RAM agree',flush=True)
    summary=dict(backend=args.backend,cpu_runs=len(results),unit_runs=2,
                 checks=sum(r['checks'] for r in results),results=results)
    (BUILD/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f"PASS RTL_MMIO: {len(results)} CPU runs + 2 device unit runs; {summary['checks']} replay fields; console/timer/faults/reset/wrap",flush=True)


if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:
        print('INTERRUPTED: no complete MMIO-suite success claimed.',file=sys.stderr);sys.exit(130)
    except (AssertionError,RuntimeError,ValueError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);sys.exit(1)
