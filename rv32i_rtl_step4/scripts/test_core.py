#!/usr/bin/env python3
"""One bounded headless regression; Icarus or Vivado command-line backend."""
import collections
import json
import os
import pathlib
import re
import subprocess
import sys
from make_decode_vectors import R_OPS, I_OPS

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = ROOT/'build/core'
RTL = [ROOT/'rtl'/n for n in ['rv32_alu.sv','rv32_regfile.sv','rv32_immediate.sv','rv32_decode.sv','rv32_core.sv']]
TOPS = ['tb_core_demo','tb_core_reset','tb_core','tb_core_reset2','tb_core_reset2044']


def run(cmd, log, cwd=ROOT):
    result=subprocess.run([str(x) for x in cmd],cwd=cwd,text=True,
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    log.write_text('$ '+' '.join(str(x) for x in cmd)+'\n'+result.stdout)
    if result.returncode:
        print(result.stdout,file=sys.stderr)
        raise RuntimeError(f'command exited {result.returncode}: see {log}')
    return result.stdout


def instruction_name(w):
    op,f3,f7=w&127,(w>>12)&7,w>>25
    if op==0x33: return R_OPS[f7,f3][0]
    if op==0x13:
        if f3==1: return 'SLLI'
        if f3==5: return 'SRAI' if f7==32 else 'SRLI'
        return I_OPS[f3][0]
    if op==3: return {0:'LB',1:'LH',2:'LW',4:'LBU',5:'LHU'}[f3]
    if op==0x23: return {0:'SB',1:'SH',2:'SW'}[f3]
    if op==0x63: return {0:'BEQ',1:'BNE',4:'BLT',5:'BGE',6:'BLTU',7:'BGEU'}[f3]
    return {0x37:'LUI',0x17:'AUIPC',0x6f:'JAL',0x67:'JALR',0xf:'FENCE'}[op]


def main():
    backend=sys.argv[1]
    BUILD.mkdir(parents=True,exist_ok=True)
    run([sys.executable,ROOT/'scripts/make_core_programs.py'],BUILD/'generate.log')
    cc=os.environ.get('CC','gcc')
    flags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Wconversion','-Wshadow','-O2']
    run([cc,*flags,'-Ireference','reference/rv32i.c','scripts/trace_reference.c','-o',BUILD/'trace-reference'],BUILD/'compile-reference.log')
    cases=json.loads((BUILD/'cases.json').read_text())
    coverage=collections.Counter()
    for case in cases:
        name=case['name']
        run([BUILD/'trace-reference',BUILD/f'{name}.bin',case['ram'],case['budget'],
             BUILD/f'{name}.trace',case['reset']],BUILD/f'{name}-reference.log')
        lines=(BUILD/f'{name}.trace').read_text().splitlines()
        terminal=lines[-2].split()
        if case['expect']=='budget': assert terminal[0]=='B'
        else: assert terminal[:2]==['T',f"{case['expect']:x}"],name
        for line in lines:
            fields=line.split()
            if fields[0]=='R': coverage[instruction_name(int(fields[2],16))]+=1
            elif fields[0]=='T' and fields[1] in ('8','9'):
                coverage['ECALL' if fields[1]=='8' else 'EBREAK']+=1
        # Independent, simple end-state anchors in addition to differential checks.
        if name in ('demo','loop','nested_calls','branches'):
            regs=[int(x,16) for x in terminal[6:]]
            memory=bytes(int(x,16) for x in lines[-1].split()[1:])
            if name=='demo': assert regs[6]==39 and int.from_bytes(memory[512:516],'little')==39
            if name=='loop': assert regs[6]==0 and int.from_bytes(memory[512:516],'little')==0
            if name=='nested_calls': assert regs[10]==42 and regs[2]==2016 and regs[20]==123
            if name=='branches': assert regs[20]==0 and regs[21]==6
    assert len(coverage)==40,coverage
    (BUILD/'instruction_coverage.json').write_text(json.dumps(coverage,indent=2)+'\n')
    if backend=='iverilog':
        compiler=[os.environ.get('IVERILOG','iverilog')]
        if os.environ.get('IVERILOG_BASE'): compiler+=['-B',os.environ['IVERILOG_BASE']]
        for top in TOPS:
            tb='tb_core.sv' if top.startswith('tb_core_reset') and top!='tb_core_reset' else f'{top}.sv'
            if top=='tb_core': tb='tb_core.sv'
            run([*compiler,'-g2012','-Wall','-s',top,'-o',BUILD/f'{top}.vvp',*RTL,ROOT/'tb'/tb],BUILD/f'{top}-compile.log')
        def simulate(top,args,log):
            return run([os.environ.get('VVP','vvp'),BUILD/f'{top}.vvp',*[f'+{a}' for a in args]],log)
    elif backend=='vivado':
        vivado=ROOT/'build/vivado_step3';vivado.mkdir(exist_ok=True)
        run(['xvlog','-sv',*RTL,*[ROOT/'tb'/p for p in ['tb_core.sv','tb_core_demo.sv','tb_core_reset.sv']]],BUILD/'vivado-compile.log',vivado)
        for top in TOPS:
            run(['xelab',f'work.{top}','-s',top,'-debug','typical'],BUILD/f'{top}-elaborate.log',vivado)
        def simulate(top,args,log):
            command=['xsim',top,'-runall']
            for arg in args: command+=['-testplusarg',arg]
            return run(command,log,vivado)
    else: raise ValueError('backend must be iverilog or vivado')
    demo=simulate('tb_core_demo',[f'VCD={BUILD}/tb_core_demo.vcd'],BUILD/'tb_core_demo.log')
    assert 'PASS RTL_CORE_DEMO:' in demo
    reset=simulate('tb_core_reset',[],BUILD/'tb_core_reset.log')
    assert 'PASS RTL_CORE_RESET:' in reset
    results=[]
    for case in cases:
        name=case['name'];top={0:'tb_core',2:'tb_core_reset2',2044:'tb_core_reset2044'}[case['reset']]
        for stall in [0,1]:
            output=simulate(top,[f'IMAGE={BUILD}/{name}.mem',f'TRACE={BUILD}/{name}.trace',
                                f"RAM_BYTES={case['ram']}",f'STALL={stall}'],BUILD/f'{name}-stall{stall}.log')
            match=re.search(r'PASS RTL_CORE retired=(\d+) cycles=(\d+) transactions=(\d+) stores=(\d+) checks=(\d+)',output)
            if not match: raise RuntimeError(f'missing PASS in {name} stall={stall}')
            results.append(dict(name=name,stall=stall,**dict(zip(['retired','cycles','transactions','stores','checks'],map(int,match.groups())))))
        print(f'PASS {name}: zero-wait and delayed memory')
    summary=dict(backend=backend,programs=len(cases),runs=len(results),instruction_names=len(coverage),
                 checks=sum(r['checks'] for r in results),retired=sum(r['retired'] for r in results),
                 cycles=sum(r['cycles'] for r in results),results=results)
    (BUILD/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(f"PASS RTL Step 3: {len(results)} differential runs; {summary['checks']} checks; all 40 base instruction names exercised (ECALL/EBREAK trap).")
    print('PASS pending fetch/store reset cancellation; tiny self-fetching demo. No GUI, Doom or synthesis run.')


if __name__=='__main__':
    try: main()
    except (AssertionError,RuntimeError,subprocess.TimeoutExpired) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);sys.exit(1)
