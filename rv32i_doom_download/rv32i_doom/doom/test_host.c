#include "host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t wad[12]={'I','W','A','D',0,0,0,0,12,0,0,0};
static void word(uint8_t *p,unsigned a,uint32_t w)
{ for(unsigned i=0;i<4u;++i)p[a+i]=(uint8_t)(w>>(8u*i)); }
int main(void)
{
    uint8_t code[64]={0};
    word(code,0,0x0000006fu); /* bounded infinite loop */
    DoomMachine *m=dh_create(code,sizeof code,wad,sizeof wad); assert(m!=NULL);
    assert(dh_run(m,0,0,100)==DH_YIELD && dh_steps(m)==100u);
    assert(dh_run(m,0,0,0)==DH_YIELD && dh_steps(m)==100u);dh_destroy(m);
    /* Request an input event, then exit with its encoded value. */
    memset(code,0,sizeof code);
    word(code,0,0x00200893u);word(code,4,0x00000073u);
    word(code,8,0x00500893u);word(code,12,0x00000073u);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,4,0,100)==DH_EXIT && dh_exit_code(m)==0x101adu);dh_destroy(m);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,1u<<11,0,100)==DH_EXIT && dh_exit_code(m)==0x10131u);dh_destroy(m);
    /* A release event is queued as well as a press. */
    word(code,8,0x00000073u);word(code,12,0x00500893u);word(code,16,0x00000073u);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,4,0,0)==DH_YIELD);
    assert(dh_run(m,0,0,0)==DH_YIELD);
    assert(dh_run(m,0,0,100)==DH_EXIT && dh_exit_code(m)==0x100adu);dh_destroy(m);
    /* Unknown ECALL and invalid framebuffer become explicit host errors. */
    memset(code,0,sizeof code);word(code,0,0x06300893u);word(code,4,0x00000073u);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,0,0,100)==DH_ERROR && strstr(dh_error(m),"unsupported ECALL")!=NULL);
    assert(dh_run(m,0,0,100)==DH_ERROR);dh_destroy(m);
    word(code,0,0x00300893u);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,0,0,100)==DH_ERROR && strstr(dh_error(m),"framebuffer")!=NULL);dh_destroy(m);
    /* Sleep across the millisecond wrap without executing while suspended. */
    memset(code,0,sizeof code);word(code,0,0x01000513u);word(code,4,0x00400893u);
    word(code,8,0x00000073u);word(code,12,0x00500893u);word(code,16,0x00000073u);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,0,UINT32_MAX-7u,100)==DH_SLEEP);
    const uint64_t steps=dh_steps(m);
    assert(dh_run(m,0,7,100)==DH_SLEEP && dh_steps(m)==steps);
    assert(dh_run(m,0,8,100)==DH_EXIT);dh_destroy(m);
    word(code,0,0x3e900513u); /* invalid 1001 ms sleep */
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    assert(dh_run(m,0,0,100)==DH_ERROR && strstr(dh_error(m),"sleep")!=NULL);dh_destroy(m);
    memset(code,0,sizeof code);word(code,0,0x0000006fu);
    m=dh_create(code,sizeof code,wad,sizeof wad);assert(m!=NULL);
    for(unsigned i=0;i<63u;++i)assert(dh_run(m,(i&1u)==0u ? 4u:0u,0,0)==DH_YIELD);
    assert(dh_run(m,0,0,0)==DH_ERROR && strstr(dh_error(m),"queue")!=NULL);dh_destroy(m);
    uint8_t bad[12];memcpy(bad,wad,sizeof bad);bad[0]='X';
    assert(dh_create(code,sizeof code,bad,sizeof bad)==NULL);
    memcpy(bad,wad,sizeof bad);memset(bad+4,0xff,4);
    assert(dh_create(code,sizeof code,bad,sizeof bad)==NULL);
    memcpy(bad,wad,sizeof bad);memset(bad+8,0xff,4);
    assert(dh_create(code,sizeof code,bad,sizeof bad)==NULL);
    assert(dh_create(code,0,wad,sizeof wad)==NULL);
    assert(dh_create(code,0x180001u,wad,sizeof wad)==NULL);
    puts("PASS Doom host loader bounds, input encoding, ECALL errors, timer wrap and bounded execution");
    return 0;
}
