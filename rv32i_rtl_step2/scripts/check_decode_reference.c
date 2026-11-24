/* Additional legality cross-check against the preserved C CPU, not a decoder
 * oracle for its control signals and not execution on behalf of RTL. */
#include "rv32i.h"
#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1],"r");
    if (f == NULL) { perror(argv[1]); return 2; }
    uint32_t word, immediate;
    uint64_t controls;
    unsigned rows=0;
    int fields;
    while ((fields=fscanf(f,"%" SCNx32 " %" SCNx64 " %" SCNx32,
                           &word,&controls,&immediate))==3) {
        uint8_t ram[64]={0};
        for (unsigned i=0;i<4u;++i) ram[i]=(uint8_t)(word>>(8u*i));
        RvCpu cpu; RvTrap trap;
        if (!rv_init(&cpu,ram,sizeof ram)) { fclose(f); return 2; }
        const RvStatus status=rv_step(&cpu,&trap);
        const bool expected_illegal=((controls>>32)&1u)!=0u;
        if ((status==RV_ILLEGAL_INSTRUCTION)!=expected_illegal) {
            fprintf(stderr,"FAIL C legality row=%u word=%08" PRIx32 " status=%s\n",
                    rows+1u,word,rv_status_name(status));
            fclose(f); return 1;
        }
        ++rows;
    }
    const bool bad=fields!=EOF || ferror(f)!=0;
    fclose(f);
    if (bad || rows==0u) return 2;
    printf("PASS C reference legality cross-check: %u words\n",rows);
    return 0;
}
