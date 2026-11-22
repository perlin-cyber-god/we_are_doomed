#include "host.h"
#include "engine/doomkeys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RAM_SIZE 0x4000000u
#define ASSET 0x2000000u
#define ASSET_CAPACITY 0x1f00000u
#define FRAME_BYTES (320u*200u*4u)
struct DoomMachine {
    RvCpu cpu;
    uint8_t *ram;
    uint64_t steps;
    uint32_t frames,frame_address,exit_code,keys,queue[64],read_index,write_index;
    uint32_t sleep_start,sleep_duration;
    bool sleeping,exited,failed;
    char error[256],log[65536];
    size_t log_length;
};
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void word(uint8_t *p,uint32_t v)
{ for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i)); }
static bool wad_valid(const uint8_t *p,size_t n)
{
    if (p==NULL || n<12u || n>ASSET_CAPACITY || memcmp(p,"IWAD",4)!=0) return false;
    const uint32_t count=le32(p+4),directory=le32(p+8);
    if (directory>n || count>(n-directory)/16u) return false;
    for(uint32_t i=0;i<count;++i) {
        const uint8_t *entry=p+directory+16u*i;
        const uint32_t a=le32(entry),size=le32(entry+4);
        if(a>n || size>n-a) return false;
    }
    return true;
}
DoomMachine *dh_create(const uint8_t *image,size_t size,const uint8_t *wad,size_t wad_size)
{
    if(image==NULL || size==0u || size>0x180000u || !wad_valid(wad,wad_size)) return NULL;
    DoomMachine *m=calloc(1,sizeof *m); if(m==NULL)return NULL;
    m->ram=malloc(RAM_SIZE); if(m->ram==NULL){free(m);return NULL;}
    memset(m->ram,0xa5,RAM_SIZE);
    memcpy(m->ram,image,size); memcpy(m->ram+ASSET,wad,wad_size);
    word(m->ram+0x1f0000u,ASSET); word(m->ram+0x1f0004u,(uint32_t)wad_size);
    if(!rv_init(&m->cpu,m->ram,RAM_SIZE)){free(m->ram);free(m);return NULL;}
    return m;
}
void dh_destroy(DoomMachine *m) { if(m!=NULL){free(m->ram);free(m);} }
const uint8_t *dh_frame(const DoomMachine *m) { return m->ram+m->frame_address; }
uint64_t dh_steps(const DoomMachine *m) { return m->steps; }
uint32_t dh_frames(const DoomMachine *m) { return m->frames; }
uint32_t dh_exit_code(const DoomMachine *m) { return m->exit_code; }
const char *dh_error(const DoomMachine *m) { return m->error; }
const char *dh_log(const DoomMachine *m) { return m->log; }
uint32_t dh_report(const DoomMachine *m,unsigned index)
{ return index<4u ? le32(m->ram+0x1f0010u+4u*index):0u; }
uint32_t dh_boot_word(const DoomMachine *m) { return le32(m->ram); }
static int failure(DoomMachine *m,const char *why,uint32_t word_value)
{
    m->failed=true;
    (void)snprintf(m->error,sizeof m->error,"%s PC=0x%08x word=0x%08x",why,(unsigned)m->cpu.pc,(unsigned)word_value);
    return DH_ERROR;
}
int dh_run(DoomMachine *m,uint32_t keys,uint32_t now,uint32_t budget)
{
    if(m->failed)return DH_ERROR;
    if(m->exited)return DH_EXIT;
    static const uint32_t keycodes[]={KEY_LEFTARROW,KEY_RIGHTARROW,KEY_UPARROW,KEY_DOWNARROW,
                                      KEY_FIRE,KEY_USE,KEY_ENTER,KEY_ESCAPE,KEY_TAB,KEY_RSHIFT,KEY_RALT,
                                      '1','2','3','4','5','6','7','y','n',0x7f};
    const uint32_t changes=(keys ^ m->keys)&0x1fffffu;
    for(unsigned bit=0;bit<21u;++bit)if((changes & (1u<<bit))!=0u) {
        const uint32_t next=(m->write_index+1u)&63u;
        if(next==m->read_index)return failure(m,"input queue overflow",0);
        m->queue[m->write_index]=0x10000u|(((keys>>bit)&1u)<<8)|keycodes[bit];
        m->write_index=next;
    }
    m->keys=keys;
    if(m->sleeping) {
        if((uint32_t)(now-m->sleep_start)<m->sleep_duration)return DH_SLEEP;
        m->sleeping=false;
    }
    for(uint32_t i=0;i<budget;++i) {
        RvTrap trap; const RvStatus s=rv_step(&m->cpu,&trap);
        if(s==RV_OK){++m->steps;continue;}
        if(s!=RV_ECALL)return failure(m,rv_status_name(s),trap.word);
        const uint32_t service=m->cpu.x[17],a=m->cpu.x[10];
        uint32_t result=0; int event=DH_YIELD;
        switch(service) {
        case 1:result=now;break;
        case 2:
            if(m->read_index!=m->write_index){result=m->queue[m->read_index];m->read_index=(m->read_index+1u)&63u;}
            break;
        case 3:
            if(m->cpu.x[11]!=320u || m->cpu.x[12]!=200u || (a&3u)!=0u || a>RAM_SIZE-FRAME_BYTES)
                return failure(m,"invalid framebuffer",trap.word);
            m->frame_address=a;++m->frames;event=DH_FRAME;break;
        case 4:
            if(a>1000u)return failure(m,"invalid sleep",trap.word);
            m->sleep_start=now;m->sleep_duration=a;m->sleeping=true;event=DH_SLEEP;break;
        case 5:m->exit_code=a;m->exited=true;event=DH_EXIT;break;
        case 6:
            if(m->log_length+1u<sizeof m->log){m->log[m->log_length++]=(char)(a&255u);m->log[m->log_length]='\0';}
            break;
        default:return failure(m,"unsupported ECALL",trap.word);
        }
        m->cpu.x[10]=result;m->cpu.pc+=4u;
        if(event!=DH_YIELD)return event;
    }
    return DH_YIELD;
}
