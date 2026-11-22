#include "../rv_run.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool load(const char *name,uint8_t *destination,size_t capacity,size_t *size)
{
    FILE *f=fopen(name,"rb"); if (f==NULL) { perror(name); return false; }
    *size=fread(destination,1,capacity,f);
    const int extra=fgetc(f);
    const bool ok=ferror(f)==0 && extra==EOF && *size!=0u;
    const int closed=fclose(f);
    return ok && closed==0;
}
static void word(uint8_t *ram,uint32_t address,uint32_t value)
{ for (unsigned i=0;i<4u;++i) ram[address+i]=(uint8_t)(value>>(8u*i)); }
int main(int argc,char **argv)
{
    if (argc!=3) { fprintf(stderr,"usage: %s IMAGE.bin FIXTURE.wad\n",argv[0]); return 2; }
    uint8_t *ram=malloc(0x80000); if (ram==NULL) return 2;
    memset(ram,0xa5,0x80000);
    size_t image_size=0,asset_size=0;
    if (!load(argv[1],ram,0x10000,&image_size) || !load(argv[2],ram+0x20000,0x1000,&asset_size)) {
        fprintf(stderr,"input empty, oversized or unreadable\n"); free(ram); return 2;
    }
    memset(ram+0x10000,0,24);
    word(ram,0x10080,0x20000); word(ram,0x10084,(uint32_t)asset_size);
    RvCpu cpu; if (!rv_init(&cpu,ram,0x80000)) { free(ram); return 2; }
    const RvRunResult r=rv_run_signature(&cpu,0x10000,10000000);
    uint32_t values[5]={0};
    for (unsigned i=0;i<5u;++i) (void)rv_peek_u32(&cpu,0x10004u+4u*i,&values[i]);
    printf("runtime: image=%zu asset=%zu steps=%" PRIu64 " run_status=%u\n",
           image_size,asset_size,r.steps,(unsigned)r.status);
    printf("checks=%" PRIu32 " failures=%" PRIu32 " asset_sum=%" PRIu32
           " heap_free=%" PRIu32 " diagnosed_invalid_frees=%" PRIu32 "\n",
           values[0],values[1],values[2],values[3],values[4]);
    if (r.status==RV_RUN_TRAP) fprintf(stderr,"trap %s pc=0x%08" PRIx32 " word=0x%08" PRIx32 "\n",
        rv_status_name(r.trap.status),r.trap.pc,r.trap.word);
    const bool ok=r.status==RV_RUN_COMPLETE && values[0]>=100u && values[1]==0u &&
                  values[2]==24u && values[3]==0x40000u-16u && values[4]==2u;
    puts(ok ? "PASS interpreted runtime guest" : "FAIL interpreted runtime guest");
    free(ram); return ok ? 0:1;
}
