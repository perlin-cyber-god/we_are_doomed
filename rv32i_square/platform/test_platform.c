#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t pixel(const uint8_t *b,unsigned i)
{
    b+=4u*i;
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static void check_frame(Platform *p,unsigned x,unsigned y)
{
    const uint8_t *b=pl_frame(p);
    for (unsigned yy=0;yy<PL_HEIGHT;++yy) for (unsigned xx=0;xx<PL_WIDTH;++xx) {
        const uint32_t want=xx>=x && xx<x+8u && yy>=y && yy<y+8u ? 0x00ffcc00u : 0x00102030u;
        assert(pixel(b,yy*PL_WIDTH+xx)==want);
    }
}
static int next(Platform *p,uint32_t keys,uint32_t *now)
{
    for (unsigned tries=0;tries<1000u;++tries) {
        int e=pl_run(p,keys,*now,10000);
        if (e==PL_WAIT) *now+=16u;
        else if (e!=PL_YIELD) return e;
    }
    fprintf(stderr,"frame budget exhausted: %s\n",pl_error(p));
    abort();
}
static void dump(Platform *p)
{
    FILE *f=fopen("build/square.ppm","wb"); assert(f!=NULL);
    assert(fprintf(f,"P6\n%u %u\n255\n",PL_WIDTH,PL_HEIGHT)>0);
    for (unsigned i=0;i<PL_PIXELS;++i) {
        uint32_t v=pixel(pl_frame(p),i);
        assert(fputc((int)((v>>16)&255u),f)!=EOF);
        assert(fputc((int)((v>>8)&255u),f)!=EOF);
        assert(fputc((int)(v&255u),f)!=EOF);
    }
    assert(fclose(f)==0);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb"); assert(f!=NULL);
    uint8_t image[PL_APP_END];
    const size_t n=fread(image,1,sizeof image,f); assert(n>0 && ferror(f)==0);
    assert(fgetc(f)==EOF); assert(fclose(f)==0);
    Platform *p=pl_create(image,n); assert(p!=NULL);
    uint32_t now=0;
    assert(next(p,0,&now)==PL_FRAME); check_frame(p,76,46); dump(p);
    assert(pl_run(p,PL_RIGHT,now,100000)==PL_WAIT);
    uint64_t steps=pl_steps(p);
    assert(pl_run(p,PL_RIGHT,now+15u,100000)==PL_WAIT && pl_steps(p)==steps);
    now+=16u;
    assert(next(p,PL_RIGHT,&now)==PL_FRAME); check_frame(p,77,46);
    assert(next(p,0,&now)==PL_FRAME); check_frame(p,77,46);
    assert(next(p,PL_LEFT|PL_RIGHT,&now)==PL_FRAME); check_frame(p,77,46);
    for (unsigned i=0;i<170u;++i) assert(next(p,PL_LEFT|PL_UP,&now)==PL_FRAME);
    check_frame(p,0,0);
    for (unsigned i=0;i<170u;++i) assert(next(p,PL_RIGHT|PL_DOWN,&now)==PL_FRAME);
    check_frame(p,152,92);
    assert(next(p,PL_QUIT,&now)==PL_STOP && pl_exit_code(p)==0u);
    printf("PASS guest frames, movement, release, opposing keys, all edges, sleep, quit (%llu CPU instructions)\n",
           (unsigned long long)pl_steps(p));
    pl_destroy(p);
    p=pl_create(image,n); assert(p!=NULL); now=UINT32_MAX-7u;
    assert(next(p,0,&now)==PL_FRAME);
    assert(pl_run(p,0,now,100000)==PL_WAIT);
    steps=pl_steps(p);
    assert(pl_run(p,PL_RIGHT,7,100000)==PL_WAIT && pl_steps(p)==steps);
    now=8; assert(next(p,PL_RIGHT,&now)==PL_FRAME); check_frame(p,77,46);
    pl_destroy(p);
    const uint8_t loop[]={0x6f,0,0,0}; p=pl_create(loop,sizeof loop); assert(p!=NULL);
    assert(pl_run(p,0,0,0)==PL_YIELD && pl_steps(p)==0u);
    assert(pl_run(p,0,0,100)==PL_YIELD && pl_steps(p)==100u); pl_destroy(p);
    const uint8_t illegal_call[]={0x13,0x05,0x30,0x06, /* addi a0,zero,99 */
        0x93,0x08,0x90,0x00, /* addi a7,zero,9 */ 0x73,0,0,0};
    p=pl_create(illegal_call,sizeof illegal_call); assert(p!=NULL);
    assert(pl_run(p,0,0,100)==PL_ERROR);
    assert(pl_pc(p)==8u && pl_register(p,10)==99u && pl_steps(p)==2u);
    assert(pl_run(p,0,0,100)==PL_ERROR && pl_pc(p)==8u); pl_destroy(p);
    const uint8_t bad_sleep[]={0x13,0x05,0x90,0x3e, /* addi a0,zero,1001 */
        0x93,0x08,0x40,0x00,0x73,0,0,0};
    p=pl_create(bad_sleep,sizeof bad_sleep); assert(p!=NULL);
    assert(pl_run(p,0,0,100)==PL_ERROR && pl_pc(p)==8u && pl_register(p,10)==1001u);
    pl_destroy(p);
    const uint8_t bad[]={0,0,0,0}; p=pl_create(bad,sizeof bad); assert(p!=NULL);
    assert(pl_run(p,0,0,100)==PL_ERROR && pl_pc(p)==0u); pl_destroy(p);
    assert(pl_create(image,PL_APP_END+1u)==NULL);
    puts("PASS wrapping clock, bounded slices, unknown ECALL, illegal instruction and loader bounds");
    return 0;
}
