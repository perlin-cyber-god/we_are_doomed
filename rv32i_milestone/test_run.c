#include "rv_run.h"
#include "guest/protocol.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { ++failures; \
    fprintf(stderr,"run FAIL line %d: %s\n",__LINE__,#c); } } while (0)
static void word(uint8_t *ram, unsigned a, uint32_t value)
{ for (unsigned i=0;i<4u;++i) ram[a+i]=(uint8_t)(value>>(8u*i)); }

int main(void)
{
    uint8_t ram[256]={0}; RvCpu cpu;
    CHECK(rv_init(&cpu,ram,sizeof ram));
    /* Real decoded instructions publish DONE at RAM[192]. This is a runner
     * fixture, NOT the cross-compiled C guest. No host result is injected.
     */
    word(ram,0,0x444f50b7u);  /* lui x1,0x444f5: round high bits before -443 */
    word(ram,4,0xe4508093u);  /* addi x1,x1,-443 -> DONE */
    word(ram,8,0x0c102023u);  /* sw x1,192(x0) */
    RvRunResult r=rv_run_signature(&cpu,192,2);
    CHECK(r.status==RV_RUN_BUDGET && r.steps==2u && cpu.pc==8u);
    r=rv_run_signature(&cpu,192,1);
    CHECK(r.status==RV_RUN_COMPLETE && r.steps==1u && cpu.pc==12u);
    CHECK(rv_init(&cpu,ram,sizeof ram)); memset(ram+192,0,SIG_USED_BYTES);
    r=rv_run_signature(&cpu,192,3);
    CHECK(r.status==RV_RUN_COMPLETE && r.steps==3u);
    CHECK(rv_init(&cpu,ram,sizeof ram)); memset(ram,0,sizeof ram);
    word(ram,0,0x0000006fu); /* jal x0,0 */
    r=rv_run_signature(&cpu,192,50);
    CHECK(r.status==RV_RUN_BUDGET && r.steps==50u && cpu.pc==0u);
    r=rv_run_signature(&cpu,192,0); CHECK(r.status==RV_RUN_BUDGET && r.steps==0u);
    word(ram,0,0x00000073u); r=rv_run_signature(&cpu,192,50);
    CHECK(r.status==RV_RUN_TRAP && r.trap.status==RV_ECALL && r.steps==0u && cpu.pc==0u);
    word(ram,192,GUEST_FAILED); r=rv_run_signature(&cpu,192,50);
    CHECK(r.status==RV_RUN_GUEST_FAILED);
    word(ram,192,123); r=rv_run_signature(&cpu,192,50);
    CHECK(r.status==RV_RUN_BAD_SIGNATURE);
    r=rv_run_signature(&cpu,193,50); CHECK(r.status==RV_RUN_INVALID_SIGNATURE);
    r=rv_run_signature(&cpu,240,50); CHECK(r.status==RV_RUN_INVALID_SIGNATURE);
    puts(failures==0u ? "PASS signature completion, exact budgets, infinite loop and traps"
                      : "FAIL signature runner");
    return failures==0u ? 0 : 1;
}
