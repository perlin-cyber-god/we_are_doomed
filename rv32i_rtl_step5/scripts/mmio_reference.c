/* Post-run replay: timer/status responses are external inputs recorded by RTL.
 * All ordinary instructions execute in the unchanged reference/rv32i.c.
 * This adapter supplies only the platform's load/store/fetch MMIO policy.
 */
#include "rv32i.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAPACITY=16384 };
#define TX UINT32_C(0x10000000)
#define STATUS UINT32_C(0x10000004)
#define TIMER UINT32_C(0x10000010)
static FILE *events;
static unsigned event_count;

static void die(const char *message)
{
    fprintf(stderr,"reference MMIO replay: %s (event %u)\n",message,event_count);exit(1);
}
static uint32_t sign12(uint32_t n)
{
    return (n&2048u)!=0u?n|UINT32_C(0xfffff000):n;
}
static uint32_t event(uint32_t address,uint32_t rm,uint32_t wm,
                      uint32_t data,bool fetch,bool expected_error)
{
    uint32_t a,r,w,d,response,err,insn;
    char tag;
    if(fscanf(events," %c %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32
              " %" SCNx32 " %" SCNx32 " %" SCNx32,
              &tag,&a,&r,&w,&d,&response,&err,&insn)!=8 || tag!='I') die("missing/bad event");
    ++event_count;
    if(a!=address || r!=rm || w!=wm || d!=data || insn!=(fetch?1u:0u) || err!=(expected_error?1u:0u))
        die("request/order/error differs from architectural expectation");
    if(expected_error && response!=0u) die("failed access returned nonzero data");
    if(wm!=0u && response!=0u) die("store returned nonzero read data");
    if(address==STATUS && !expected_error && response>1u) die("bad status reserved bits");
    return response;
}
static RvStatus report(RvCpu *c,RvTrap *t,RvStatus code,uint32_t word,uint32_t address)
{
    *t=(RvTrap){code,c->pc,word,address};return code;
}
static RvStatus platform_step(RvCpu *c,RvTrap *t)
{
    if((c->pc&3u)!=0u) return rv_step(c,t);
    if(c->pc>=CAPACITY) {
        (void)event(c->pc,15,0,0,true,true);
        return rv_step(c,t);
    }
    uint32_t word;
    if(!rv_peek_u32(c,c->pc,&word)) return rv_step(c,t);
    const uint32_t op=word&127u,f3=(word>>12)&7u;
    const bool store=op==0x23u;
    if(op!=3u && !store) return rv_step(c,t);
    const bool valid=store?f3<=2u:(f3==0u || f3==1u || f3==2u || f3==4u || f3==5u);
    if(!valid) return rv_step(c,t);
    const uint32_t width=1u<<(f3&3u);
    const uint32_t raw=store?((word>>25)<<5)|((word>>7)&31u):word>>20;
    const uint32_t address=c->x[(word>>15)&31u]+sign12(raw);
    if((address&(width-1u))!=0u || address<CAPACITY) return rv_step(c,t);
    const uint32_t lanes=((1u<<width)-1u)<<(address&3u);
    const uint32_t data=store?c->x[(word>>20)&31u]<<((address&3u)*8u):0u;
    const bool tx=store && address==TX && width==1u;
    const bool read=!store && width==4u && (address==STATUS || address==TIMER);
    const bool error=!tx && !read;
    const uint32_t response=event(address&UINT32_C(0xfffffffc),store?0u:lanes,
                                  store?lanes:0u,data,false,error);
    if(error) return report(c,t,store?RV_STORE_ACCESS:RV_LOAD_ACCESS,word,address);
    if(!store) {
        const uint32_t rd=(word>>7)&31u;
        if(rd!=0u) c->x[rd]=response; // Only full-word MMIO reads are legal here.
    }
    const uint32_t old=c->pc;c->pc+=4u;
    *t=(RvTrap){RV_OK,old,word,0};return RV_OK;
}
static void state(FILE *out,const RvCpu *c)
{
    fputs(" 00000000",out); // Format marker, no per-instruction RAM hash.
    for(unsigned i=0;i<32;++i) fprintf(out," %08" PRIx32,c->x[i]);
    fputc('\n',out);
}
static void memory(FILE *out,const uint8_t *ram)
{
    fputs("M",out);
    for(size_t i=0;i<CAPACITY;++i) fprintf(out," %02x",(unsigned)ram[i]);
    fputc('\n',out);
}
int main(int argc,char **argv)
{
    if(argc!=5) {fprintf(stderr,"usage: mmio-reference image.bin events.txt steps trace.txt\n");return 2;}
    const unsigned long budget=strtoul(argv[3],NULL,0);
    if(budget<1 || budget>20000) return 2;
    uint8_t ram[CAPACITY],before[CAPACITY];
    FILE *in=fopen(argv[1],"rb");if(!in) return 2;
    const size_t got=fread(ram,1,sizeof ram,in);const int extra=fgetc(in);fclose(in);
    if(got!=sizeof ram || extra!=EOF) return 2;
    events=fopen(argv[2],"r");if(!events) return 2;
    FILE *out=fopen(argv[4],"w");if(!out) {fclose(events);return 2;}
    RvCpu cpu;if(!rv_init(&cpu,ram,CAPACITY)) return 2;
    bool stopped=false;
    for(unsigned long step=0;step<budget;++step) {
        const uint32_t pc=cpu.pc;
        uint32_t word=0;(void)rv_peek_u32(&cpu,pc,&word);
        uint32_t regs[32];memcpy(regs,cpu.x,sizeof regs);memcpy(before,ram,sizeof ram);
        const uint32_t op=word&127u,f3=(word>>12)&7u,rd=(word>>7)&31u;
        uint32_t address=0,rm=0,wm=0,data=0;
        if(op==3u || op==0x23u) {
            const uint32_t raw=op==3u?word>>20:((word>>25)<<5)|((word>>7)&31u);
            address=regs[(word>>15)&31u]+sign12(raw);
            const unsigned width=1u<<(f3&3u);
            const uint32_t mask=((1u<<width)-1u)<<(address&3u);
            if(op==3u) rm=mask;else {wm=mask;data=regs[(word>>20)&31u]<<((address&3u)*8u);}
        }
        RvTrap trap;const RvStatus status=platform_step(&cpu,&trap);
        if(status!=RV_OK) {
            if(cpu.pc!=pc || memcmp(regs,cpu.x,sizeof regs)!=0 || memcmp(before,ram,sizeof ram)!=0)
                die("fault changed architectural state");
            fprintf(out,"T %x %08" PRIx32 " %08" PRIx32 " %08" PRIx32,
                    (unsigned)status,trap.pc,trap.word,trap.address);
            state(out,&cpu);stopped=true;break;
        }
        const bool writes=op==0x13u || op==0x33u || op==0x37u || op==0x17u || op==3u || op==0x6fu || op==0x67u;
        const uint32_t dest=writes?rd:0;
        fprintf(out,"R %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %x %08" PRIx32
                    " %08" PRIx32 " %x %x %08" PRIx32,
                pc,word,cpu.pc,dest,dest?cpu.x[dest]:0,address,rm,wm,data);
        state(out,&cpu);
    }
    if(!stopped) {fprintf(out,"B %08" PRIx32,cpu.pc);state(out,&cpu);}
    memory(out,ram);
    int ch;
    do {ch=fgetc(events);} while(ch!=EOF && isspace((unsigned char)ch));
    if(ch!=EOF || ferror(events)) die("unconsumed events");
    fclose(events);if(fclose(out)!=0) return 2;
    printf("PASS C_MMIO_REPLAY events=%u terminal=%s PC=%08" PRIx32 "\n",event_count,stopped?"trap":"budget",cpu.pc);
    return 0;
}
