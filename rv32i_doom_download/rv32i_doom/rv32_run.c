#include "rv_run.h"
#include "guest/protocol.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool number(const char *s, uint64_t *result)
{
    if (*s=='-' || *s=='+' || *s=='\0') return false;
    char *end=NULL; errno=0;
    const unsigned long long n=strtoull(s,&end,0);
    if (errno!=0 || end==s || *end!='\0') return false;
    *result=(uint64_t)n; return true;
}

int main(int argc, char **argv)
{
    uint64_t bytes=GUEST_RAM_BYTES, budget=1000000, sig=GUEST_SIGNATURE;
    if (argc<2 || argc>5 || (argc>2 && !number(argv[2],&bytes)) ||
        (argc>3 && !number(argv[3],&budget)) || (argc>4 && !number(argv[4],&sig)) ||
        bytes<4u || bytes>UINT64_C(0x100000000) || bytes>SIZE_MAX ||
        sig>UINT32_MAX || (sig & 3u)!=0u || sig+SIG_USED_BYTES>bytes) {
        fprintf(stderr,"usage: %s IMAGE.bin [RAM_BYTES [BUDGET [SIGNATURE_ADDRESS]]]\n",argv[0]);
        return 2;
    }
    uint8_t *ram=malloc((size_t)bytes);
    if (ram==NULL) { perror("RAM allocation"); return 2; }
    /* Poisoning proves BSS initialization must be performed by guest startup.
     * Code and initialized data come from the image. Only signature is cleared.
     */
    memset(ram,0xa5,(size_t)bytes);
    memset(ram+(size_t)sig,0,SIG_USED_BYTES);
    FILE *input=fopen(argv[1],"rb");
    if (input==NULL) { perror(argv[1]); free(ram); return 2; }
    /* Read only below signature. One extra byte detects overlap/oversize. */
    const size_t count=fread(ram,1,(size_t)sig,input);
    const int extra=fgetc(input);
    const bool bad=ferror(input)!=0 || extra!=EOF;
    const int close_status=fclose(input);
    if (bad || close_status!=0 || count==0u) {
        fprintf(stderr,"image empty, unreadable, or overlaps reserved signature region\n");
        free(ram); return 2;
    }
    RvCpu cpu;
    if (!rv_init(&cpu,ram,(size_t)bytes)) { free(ram); return 2; }
    const RvRunResult run=rv_run_signature(&cpu,(uint32_t)sig,budget);
    printf("loaded %zu bytes at 0; RAM=%" PRIu64 " budget=%" PRIu64
           " retired=%" PRIu64 " PC=0x%08" PRIx32 "\n",count,bytes,budget,run.steps,cpu.pc);
    if (run.status!=RV_RUN_COMPLETE) {
        fprintf(stderr,"guest did not complete: run status %u",(unsigned)run.status);
        if (run.status==RV_RUN_TRAP)
            fprintf(stderr,"; %s at PC=0x%08" PRIx32 " word=0x%08" PRIx32
                    " address=0x%08" PRIx32,rv_status_name(run.trap.status),
                    run.trap.pc,run.trap.word,run.trap.address);
        fputc('\n',stderr); free(ram); return 1;
    }
    uint32_t payload[5]={0};
    for (unsigned i=0; i<5u; ++i)
        (void)rv_peek_u32(&cpu,(uint32_t)sig+4u*(i+1u),&payload[i]);
    /* An independent host reference checks results read FROM guest RAM.
     * It never injects register values, stores, or completion into the CPU.
     */
    uint32_t expected=0;
    for (uint32_t i=1; i<=6u; ++i) expected += (i<<1) + 7u + 1u;
    const bool passed=payload[0]==expected && payload[1]==6u && payload[2]==7u &&
                      payload[3]==0u && payload[4]==6u;
    printf("signature: checksum=%" PRIu32 " count=%" PRIu32 " initialized=%" PRIu32
           " BSS_before=%" PRIu32 " nested_calls=%" PRIu32 "\n",
           payload[0],payload[1],payload[2],payload[3],payload[4]);
    puts(passed ? "PASS compiled guest signature" : "FAIL compiled guest signature");
    free(ram); return passed ? 0 : 1;
}
