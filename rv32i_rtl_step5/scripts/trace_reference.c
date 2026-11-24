/* Execute the preserved C interpreter, emitting retirement/trap observations.
 * This runs the instruction image, never the guest source as a native program.
 */
#include "rv32i.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAPACITY = 2048 };
static uint32_t hash_ram(const uint8_t *ram)
{
    uint32_t h = UINT32_C(2166136261);
    for (size_t i = 0; i < CAPACITY; ++i) h = (h ^ ram[i]) * UINT32_C(16777619);
    return h;
}
static uint32_t sign12(uint32_t n)
{
    return (n & 2048u) != 0u ? n | UINT32_C(0xfffff000) : n;
}
static void state(FILE *out, const RvCpu *c, const uint8_t *ram)
{
    fprintf(out," %08" PRIx32,hash_ram(ram));
    for (unsigned i=0; i<32; ++i) fprintf(out," %08" PRIx32,c->x[i]);
    fputc('\n',out);
}
static void final_memory(FILE *out,const uint8_t *ram)
{
    fputs("M",out);
    for(size_t i=0;i<CAPACITY;++i) fprintf(out," %02x",(unsigned)ram[i]);
    fputc('\n',out);
}
int main(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr,"usage: trace-reference image.bin ram-bytes steps trace.txt reset-pc\n");
        return 2;
    }
    const unsigned long size=strtoul(argv[2],NULL,0), budget=strtoul(argv[3],NULL,0);
    const unsigned long reset_pc=strtoul(argv[5],NULL,0);
    if (size<4 || size>CAPACITY || budget==0 || budget>10000 || reset_pc>UINT32_MAX) return 2;
    uint8_t ram[CAPACITY],before[CAPACITY];
    FILE *in=fopen(argv[1],"rb");
    if (!in) return 2;
    const size_t got=fread(ram,1,sizeof ram,in);
    const int extra=fgetc(in);
    const int bad=ferror(in);
    fclose(in);
    if (got!=sizeof ram || extra!=EOF || bad) return 2;
    FILE *out=fopen(argv[4],"w");
    if (!out) return 2;
    RvCpu cpu;
    if (!rv_init(&cpu,ram,(size_t)size)) { fclose(out); return 2; }
    cpu.pc=(uint32_t)reset_pc;
    for (unsigned long step=0; step<budget; ++step) {
        const uint32_t pc=cpu.pc;
        uint32_t w=0;
        (void)rv_peek_u32(&cpu,pc,&w);
        uint32_t regs[32];
        memcpy(regs,cpu.x,sizeof regs);
        memcpy(before,ram,sizeof ram);
        const uint32_t op=w&127u,rd=(w>>7)&31u,f3=(w>>12)&7u;
        const uint32_t a=regs[(w>>15)&31u],b=regs[(w>>20)&31u];
        uint32_t address=0,rmask=0,wmask=0,data=0;
        if (op==3u || op==0x23u) {
            const uint32_t offset=op==3u ? w>>20 : ((w>>25)<<5)|((w>>7)&31u);
            address=a+sign12(offset);
            const unsigned width=1u<<(f3&3u);
            const uint32_t mask=((1u<<width)-1u)<<(address&3u);
            if (op==3u) rmask=mask;
            else { wmask=mask; data=b<<((address&3u)*8u); }
        }
        RvTrap trap;
        const RvStatus status=rv_step(&cpu,&trap);
        if (status!=RV_OK) {
            if (cpu.pc!=pc || memcmp(cpu.x,regs,sizeof regs)!=0 || memcmp(ram,before,sizeof ram)!=0) {
                fprintf(stderr,"C reference changed state on trap\n");fclose(out);return 1;
            }
            fprintf(out,"T %x %08" PRIx32 " %08" PRIx32 " %08" PRIx32,
                    (unsigned)status,trap.pc,trap.word,trap.address);
            state(out,&cpu,ram);
            final_memory(out,ram);
            if (fclose(out)!=0) return 2;
            printf("reference retired=%lu trap=%u pc=%08" PRIx32 "\n",step,(unsigned)status,cpu.pc);
            return 0;
        }
        const bool write=op==0x13u || op==0x33u || op==0x37u || op==0x17u || op==3u || op==0x6fu || op==0x67u;
        const uint32_t dest=write?rd:0u;
        fprintf(out,"R %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %x %08" PRIx32
                    " %08" PRIx32 " %x %x %08" PRIx32,
                pc,w,cpu.pc,dest,dest!=0?cpu.x[dest]:0,address,rmask,wmask,data);
        state(out,&cpu,ram);
    }
    fprintf(out,"B %08" PRIx32,cpu.pc);
    state(out,&cpu,ram);
    final_memory(out,ram);
    if (fclose(out)!=0) return 2;
    printf("reference budget-prefix=%lu pc=%08" PRIx32 " (not guest HALT)\n",budget,cpu.pc);
    return 0;
}
