#!/usr/bin/env python3
"""Cross-compile C, execute the resulting bytes in C-reference and RTL.
No native execution of guest.c, QEMU, GUI or Doom.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys

ROOT=pathlib.Path(__file__).resolve().parent.parent
BUILD=ROOT/'build/guest'
CAPACITY=0x4000
SIG=0x2000
DONE=0x444f4e45
FAIL=0x4641494c
RTL=[ROOT/'rtl'/p for p in ['rv32_alu.sv','rv32_regfile.sv','rv32_immediate.sv','rv32_decode.sv','rv32_core.sv']]
FLAGS=['-march=rv32i','-mabi=ilp32','-mno-relax','-mstrict-align','-msmall-data-limit=0',
       '-ffreestanding','-fno-builtin','-fno-pic','-fno-pie','-fno-stack-protector',
       '-fno-omit-frame-pointer','-fno-optimize-sibling-calls',
       '-fno-unwind-tables','-fno-asynchronous-unwind-tables','-g',
       '-Wall','-Wextra','-Wpedantic','-Werror','-Wconversion','-Wshadow']


def run(cmd,log,cwd=ROOT,timeout=60):
    log.parent.mkdir(parents=True,exist_ok=True)
    try:
        result=subprocess.run([str(x) for x in cmd],cwd=cwd,text=True,
                              stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        log.write_text('$ '+' '.join(map(str,cmd))+'\nTIMEOUT\n'+str(exc.output or ''))
        raise RuntimeError(f'command timed out: {log}') from exc
    log.write_text('$ '+' '.join(map(str,cmd))+'\n'+result.stdout)
    if result.returncode:
        print(result.stdout,file=sys.stderr)
        raise RuntimeError(f'exit {result.returncode}: {log}')
    return result.stdout


def compiler_prefix():
    prefix=os.environ.get('RISCV_PREFIX')
    if prefix is not None:
        if not shutil.which(prefix+'gcc'): raise RuntimeError('RISCV_PREFIX gcc not found')
        return prefix
    for prefix in ['riscv64-unknown-elf-','riscv32-unknown-elf-']:
        if shutil.which(prefix+'gcc'): return prefix
    raise RuntimeError('No RISC-V GCC found. Install your cross toolchain or set RISCV_PREFIX; no guest success claimed.')


def base_instruction(word):
    op,f3,f7=word&127,(word>>12)&7,word>>25
    if op==0x33: return f7==0 or (f7==32 and f3 in (0,5))
    if op==0x13: return f3 not in (1,5) or (f3==1 and f7==0) or (f3==5 and f7 in (0,32))
    if op==3: return f3 in (0,1,2,4,5)
    if op==0x23: return f3 in (0,1,2)
    if op==0x63: return f3 in (0,1,4,5,6,7)
    if op in (0x37,0x17,0x6f): return True
    if op==0x67: return f3==0
    if op==0xf: return f3==0
    return word in (0x73,0x100073)


def inspect_elf(path):
    data=path.read_bytes()
    assert data[:7]==b'\x7fELF\x01\x01\x01','ELF32 little-endian required'
    header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    assert header[2]==243 and header[4]==0 and header[7]&7==0,'RISC-V entry0, no RVC/float ABI required'
    count=0
    for index in range(header[12]):
        sec=struct.unpack_from('<10I',data,header[6]+index*header[11])
        if sec[2]&4:
            assert sec[3]%4==0 and sec[5]%4==0
            for offset in range(sec[4],sec[4]+sec[5],4):
                word=int.from_bytes(data[offset:offset+4],'little')
                assert base_instruction(word),f'unsupported executable word {word:08x}'
                count+=1
    assert count>0
    return count


def build(prefix,name,opt,skip_bss=False):
    dest=BUILD/name;dest.mkdir(parents=True,exist_ok=True)
    run([prefix+'gcc',*FLAGS,opt,'-std=c11','-c','guest/guest.c','-o',dest/'guest.o'],dest/'compile-c.log')
    assembly=[prefix+'gcc','-march=rv32i','-mabi=ilp32','-mno-relax']
    if skip_bss: assembly+=['-DSKIP_BSS_CLEAR=1']
    run([*assembly,'-c','guest/start.S','-o',dest/'start.o'],dest/'compile-start.log')
    run([prefix+'gcc','-march=rv32i','-mabi=ilp32','-nostdlib','-nostartfiles','-static',
         '-Wl,-T,guest/linker.ld','-Wl,--no-relax','-Wl,--build-id=none',f'-Wl,-Map,{dest}/guest.map',
         dest/'start.o',dest/'guest.o','-o',dest/'guest.elf'],dest/'link.log')
    run([prefix+'objcopy','-O','binary',dest/'guest.elf',dest/'guest.bin'],dest/'objcopy.log')
    nm=run([prefix+'nm','-n',dest/'guest.elf'],dest/'symbols.log')
    symbols={m[2]:int(m[0],16) for line in nm.splitlines() if (m:=line.split()) and len(m)==3}
    for n in ['_start','_guest_stop','inner','outer','guest_main','initialized_global',
              'zero_global','nested_count','values','__data_start','__data_end','__bss_start','__bss_end',
              '__signature_start','__stack_bottom','__stack_top']:
        assert n in symbols,n
    assert symbols['_start']==0 and symbols['__stack_top']==CAPACITY and symbols['__signature_start']==SIG
    disassembly=run([prefix+'objdump','-d','-S','-M','no-aliases',dest/'guest.elf'],dest/'disassembly.log')
    (dest/'guest.dis').write_text(disassembly)
    run([prefix+'readelf','-h','-l','-SW','-A',dest/'guest.elf'],dest/'elf-info.log')
    words=inspect_elf(dest/'guest.elf')
    (dest/'metadata.json').write_text(json.dumps(dict(symbols=symbols,code_words=words,opt=opt,skip_bss=skip_bss),indent=2)+'\n')
    print(f'BUILT {name}: {words} executable RV32I words; no M/RVC',flush=True)
    return symbols


def image_for(profile,name,symbols,changed=False):
    binary=(BUILD/profile/'guest.bin').read_bytes()
    assert 0<len(binary)<=symbols['__bss_start']<0x1800
    ram=bytearray([0xa5]*CAPACITY)
    ram[:len(binary)]=binary
    if changed:
        address=symbols['initialized_global'];ram[address:address+4]=(9).to_bytes(4,'little')
    assert ram[symbols['__bss_start']:symbols['__bss_end']]==bytes([0xa5])*(symbols['__bss_end']-symbols['__bss_start'])
    (BUILD/f'{name}.load.bin').write_bytes(ram)
    (BUILD/f'{name}.mem').write_text(''.join(f'{b:02x}\n' for b in ram))
    return bytes(ram)


def trace(name,budget):
    run([BUILD/'trace-guest',BUILD/f'{name}.load.bin',CAPACITY,budget,BUILD/f'{name}.trace',0],BUILD/f'{name}-reference.log')
    records=(BUILD/f'{name}.trace').read_text().splitlines()
    terminal=records[-2].split()
    retires=[line.split() for line in records if line.startswith('R ')]
    return records,terminal,retires


def verify_protocol(memory,terminal,symbols,initial,expected_initialized=7):
    if terminal[0]!='T' or int(terminal[1],16)!=9 or int(terminal[2],16)!=symbols['_guest_stop']:
        return False,'missing EBREAK at _guest_stop'
    signature=list(struct.unpack_from('<6I',memory,SIG))
    expected_values=[(v<<1)+expected_initialized+1 for v in range(1,7)]
    expected=[DONE,sum(expected_values),6,expected_initialized,0,6]
    if signature!=expected: return False,f'signature {signature} expected {expected}'
    values=list(struct.unpack_from('<6I',memory,symbols['values']))
    if values!=expected_values: return False,'array mismatch'
    writable=[(symbols['__data_start'],symbols['__data_end']),
              (symbols['__bss_start'],symbols['__bss_end']),(SIG,SIG+24),(0x3000,0x4000)]
    for i,(a,b) in enumerate(zip(memory,initial)):
        if a!=b and not any(lo<=i<hi for lo,hi in writable): return False,f'guard/code byte changed at {i:x}'
    return True,'signature, array, code/guards verified'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('backend',choices=['iverilog','vivado'])
    args=parser.parse_args();BUILD.mkdir(parents=True,exist_ok=True)
    prefix=compiler_prefix()
    run([prefix+'gcc','--version'],BUILD/'cross-version.log')
    host_flags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Wconversion','-Wshadow','-O2']
    run([os.environ.get('CC','gcc'),*host_flags,'-Ireference','reference/rv32i.c','scripts/trace_guest.c',
         '-o',BUILD/'trace-guest'],BUILD/'compile-reference.log')
    symbols0=build(prefix,'O0','-O0');symbols2=build(prefix,'O2','-O2')
    symbols_bad=build(prefix,'no_bss','-O0',True)
    if args.backend=='iverilog':
        cmd=[os.environ.get('IVERILOG','iverilog')]
        if os.environ.get('IVERILOG_BASE'): cmd+=['-B',os.environ['IVERILOG_BASE']]
        run([*cmd,'-g2012','-Wall','-s','tb_compiled_guest','-o',BUILD/'guest.vvp',*RTL,ROOT/'tb/tb_compiled_guest.sv'],BUILD/'compile-rtl.log')
        def simulate(name,stall,budget):
            return run([os.environ.get('VVP','vvp'),BUILD/'guest.vvp',
                        f'+IMAGE={BUILD}/{name}.mem',f'+TRACE={BUILD}/{name}.trace',
                        f'+DUMP={BUILD}/{name}-stall{stall}.rtl.mem','+RAM_BYTES=16384',
                        f'+STALL={stall}',f'+MAX_STEPS={budget}'],BUILD/f'{name}-stall{stall}.log')
    else:
        vivado=ROOT/'build/vivado_guest';vivado.mkdir(exist_ok=True)
        run(['xvlog','-sv',*RTL,ROOT/'tb/tb_compiled_guest.sv'],BUILD/'compile-rtl.log',vivado)
        run(['xelab','work.tb_compiled_guest','-s','rtl_compiled_guest','-debug','typical'],BUILD/'elaborate.log',vivado)
        def simulate(name,stall,budget):
            cmd=['xsim','rtl_compiled_guest','-runall']
            for arg in [f'IMAGE={BUILD}/{name}.mem',f'TRACE={BUILD}/{name}.trace',
                        f'DUMP={BUILD}/{name}-stall{stall}.rtl.mem','RAM_BYTES=16384',f'STALL={stall}',f'MAX_STEPS={budget}']:
                cmd+=['-testplusarg',arg]
            return run(cmd,BUILD/f'{name}-stall{stall}.log',vivado)
    results=[]
    configs=[('O0','O0',symbols0,False,True),('O2','O2',symbols2,False,True),
             ('O0','changed_data',symbols0,True,False),('no_bss','no_bss',symbols_bad,False,False)]
    saved_normal=None
    for profile,name,symbols,changed,should_accept in configs:
        initial=image_for(profile,name,symbols,changed)
        records,terminal,retires=trace(name,10000)
        assert terminal[:2]==['T','9'] and int(terminal[2],16)==symbols['_guest_stop']
        if should_accept:
            # N successful steps + one terminal trap attempt must also finish.
            original='\n'.join(records)+'\n';records,terminal,retires=trace(name,len(retires)+1)
            assert (BUILD/f'{name}.trace').read_text()==original
        budget=len(retires)+1
        if name=='O0': saved_normal=(initial,len(retires),symbols)
        # Actual PCs show calls entered both functions six times; stack writes occurred.
        if should_accept:
            pcs=[int(r[1],16) for r in retires]
            assert pcs.count(symbols['guest_main'])==1 and pcs.count(symbols['outer'])==6 and pcs.count(symbols['inner'])==6
            stack_writes=[r for r in retires if int(r[8],16)!=0 and 0x3000<=int(r[6],16)<0x4000]
            assert stack_writes,'guest did not use stack'
            sps=[int(r[13],16) for r in retires if int(r[13],16)!=0] # x2 after tag+9 fields+marker
            assert sps and min(sps)>=0x3000 and max(sps)<=0x4000
            assert int(retires[-1][13],16)==0x4000,'stack not restored after guest_main'
        for stall in (0,1):
            print(f'RUN {name} memory={"zero-wait" if stall==0 else "delayed"} budget={budget}',flush=True)
            output=simulate(name,stall,budget)
            m=re.search(r'PASS RTL_COMPILED_TRACE retired=(\d+) cycles=(\d+) transactions=(\d+) stores=(\d+) checks=(\d+)',output)
            assert m,'missing RTL completion marker'
            memory=bytes(int(x,16) for x in (BUILD/f'{name}-stall{stall}.rtl.mem').read_text().split())
            assert len(memory)==CAPACITY
            accepted,reason=verify_protocol(memory,terminal,symbols,initial)
            assert accepted==should_accept,(name,reason)
            if changed:
                assert struct.unpack_from('<I',memory,SIG+4)[0]==sum((v<<1)+9+1 for v in range(1,7))
                assert verify_protocol(memory,terminal,symbols,initial,9)[0]
            if name=='no_bss': assert struct.unpack_from('<I',memory,SIG)[0]==FAIL
            signature=list(struct.unpack_from('<6I',memory,SIG))
            results.append(dict(name=name,stall=stall,accepted=accepted,signature=signature,
                                **dict(zip(['retired','cycles','transactions','stores','checks'],map(int,m.groups())))))
            print(f'PASS {name}: RTL signature checksum={signature[1]} status={signature[0]:08x}; '+
                  ('accepted' if accepted else 'expected protocol rejection'),flush=True)
    initial,normal_steps,symbols=saved_normal
    # Stop one retirement before DONE publication; a prefix is not guest success.
    name='budget_prefix';(BUILD/f'{name}.load.bin').write_bytes(initial)
    (BUILD/f'{name}.mem').write_text(''.join(f'{b:02x}\n' for b in initial))
    _,terminal,_=trace(name,normal_steps-1);assert terminal[0]=='B'
    print('RUN budget_prefix: intentional insufficient host budget',flush=True)
    output=simulate(name,1,normal_steps-1)
    assert 'BUDGET_PREFIX (not guest HALT)' in output
    m=re.search(r'PASS RTL_COMPILED_TRACE retired=(\d+) cycles=(\d+) transactions=(\d+) stores=(\d+) checks=(\d+)',output)
    assert m,'missing bounded-prefix comparison marker'
    memory=bytes(int(x,16) for x in (BUILD/f'{name}-stall1.rtl.mem').read_text().split())
    assert not verify_protocol(memory,terminal,symbols,initial)[0]
    results.append(dict(name=name,stall=1,accepted=False,
                        signature=list(struct.unpack_from('<6I',memory,SIG)),
                        **dict(zip(['retired','cycles','transactions','stores','checks'],map(int,m.groups())))))
    print('PASS insufficient budget rejected as incomplete, not guest success',flush=True)
    summary=dict(backend=args.backend,runs=9,positive_runs=4,negative_runs=5,
                 differential_checks=sum(r['checks'] for r in results),results=results)
    (BUILD/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('PASS RTL_COMPILED_C: O0/O2 arrays, nested calls, initialized globals, BSS; changed data/BSS/budget negatives',flush=True)


if __name__=='__main__':
    try: main()
    except KeyboardInterrupt:
        print('INTERRUPTED: no full-suite success claimed.',file=sys.stderr);sys.exit(130)
    except (AssertionError,RuntimeError,subprocess.TimeoutExpired) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);sys.exit(1)
