/* Execution environment, not part of the RV32I decoder. ECALL remains a trap
 * in rv32i.c. This opt-in host dispatcher services our documented demo ABI.
 */
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Platform {
    RvCpu cpu;
    uint8_t ram[PL_RAM];
    uint64_t steps;
    uint32_t sleep_start, sleep_duration, exit_code;
    bool sleeping, stopped, failed;
    char error[128];
};
Platform *pl_create(const uint8_t *image, size_t length)
{
    if (image==NULL || length==0u || length>PL_APP_END) return NULL;
    Platform *p=calloc(1,sizeof *p);
    if (p==NULL) return NULL;
    memset(p->ram,0xa5,sizeof p->ram);
    if (!rv_init(&p->cpu,p->ram,sizeof p->ram) || !rv_load_image(&p->cpu,0,image,length)) {
        free(p); return NULL;
    }
    return p;
}
void pl_destroy(Platform *p) { free(p); }
const uint8_t *pl_frame(const Platform *p) { return p->ram+PL_FB; }
uint64_t pl_steps(const Platform *p) { return p->steps; }
const char *pl_error(const Platform *p) { return p->error; }
uint32_t pl_exit_code(const Platform *p) { return p->exit_code; }
uint32_t pl_pc(const Platform *p) { return p->cpu.pc; }
uint32_t pl_register(const Platform *p, unsigned index)
{ return index<32u ? p->cpu.x[index] : 0u; }
static int fail(Platform *p, const char *message)
{
    p->failed=true;
    (void)snprintf(p->error,sizeof p->error,"%s at PC=0x%08x",message,(unsigned)p->cpu.pc);
    return PL_ERROR;
}
int pl_run(Platform *p, uint32_t keys, uint32_t now, uint32_t budget)
{
    if (p->failed) return PL_ERROR;
    if (p->stopped) return PL_STOP;
    if (p->sleeping) {
        if ((uint32_t)(now-p->sleep_start)<p->sleep_duration) return PL_WAIT;
        p->sleeping=false;
    }
    for (uint32_t i=0;i<budget;++i) {
        RvTrap trap;
        const RvStatus s=rv_step(&p->cpu,&trap);
        if (s==RV_OK) { ++p->steps; continue; }
        if (s!=RV_ECALL) return fail(p,rv_status_name(s));
        const uint32_t service=p->cpu.x[17], arg=p->cpu.x[10]; /* a7, a0 */
        uint32_t result=0;
        int event=PL_YIELD;
        switch (service) {
        case PL_TICKS: result=now; break;
        case PL_KEYS: result=keys & 31u; break;
        case PL_PRESENT: event=PL_FRAME; break;
        case PL_SLEEP:
            if (arg>1000u) return fail(p,"sleep exceeds ABI limit");
            p->sleep_start=now; p->sleep_duration=arg; p->sleeping=true;
            event=PL_WAIT; break;
        case PL_EXIT:
            p->exit_code=arg; p->stopped=true; event=PL_STOP; break;
        default: return fail(p,"unsupported ECALL service");
        }
        /* A successful host service resumes AFTER ECALL. Unknown/invalid
         * requests stop above without modifying guest PC/registers/RAM.
         * This environment leaves a1..a7 and other registers unchanged.
         */
        p->cpu.x[10]=result;
        p->cpu.pc+=4u;
        if (event!=PL_YIELD) return event;
    }
    return PL_YIELD;
}
