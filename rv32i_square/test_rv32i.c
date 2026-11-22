#include "rv32i.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; \
    fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); } } while (0)

typedef struct { RvCpu cpu; uint8_t ram[258]; } Fixture;

static uint32_t ei(uint32_t op, uint32_t f3, uint32_t rd, uint32_t rs1, int32_t imm)
{
    return (((uint32_t)imm & 4095u) << 20) | (rs1 << 15)
         | (f3 << 12) | (rd << 7) | op;
}
static uint32_t er(uint32_t f7, uint32_t f3, uint32_t rd, uint32_t rs1, uint32_t rs2)
{ return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33u; }
static uint32_t es(uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    const uint32_t u = (uint32_t)imm & 4095u;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12)
         | ((u & 31u) << 7) | 0x23u;
}
static uint32_t eb(uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    const uint32_t u = (uint32_t)imm & 8191u;
    return (((u >> 12) & 1u) << 31) | (((u >> 5) & 63u) << 25)
         | (rs2 << 20) | (rs1 << 15) | (f3 << 12)
         | (((u >> 1) & 15u) << 8) | (((u >> 11) & 1u) << 7) | 0x63u;
}
static uint32_t ej(uint32_t rd, int32_t imm)
{
    const uint32_t u = (uint32_t)imm & 0x1fffffu;
    return (((u >> 20) & 1u) << 31) | (((u >> 1) & 1023u) << 21)
         | (((u >> 11) & 1u) << 20) | (u & 0xff000u) | (rd << 7) | 0x6fu;
}
static void word(Fixture *f, uint32_t a, uint32_t w)
{
    for (unsigned i = 0; i < 4u; ++i) f->ram[a + i] = (uint8_t)(w >> (8u * i));
}
static void fresh(Fixture *f, size_t size, uint32_t w)
{
    memset(f->ram,0,sizeof f->ram);
    CHECK(rv_init(&f->cpu,f->ram,size));
    word(f,0,w);
}
static void reject(Fixture *f, RvStatus expected)
{
    const RvCpu before = f->cpu;
    uint8_t bytes[sizeof f->ram]; memcpy(bytes,f->ram,sizeof bytes);
    RvTrap trap;
    CHECK(rv_step(&f->cpu,&trap) == expected);
    CHECK(trap.status == expected && trap.pc == before.pc);
    CHECK(f->cpu.pc == before.pc && f->cpu.ram == before.ram);
    CHECK(f->cpu.ram_size == before.ram_size);
    CHECK(memcmp(f->cpu.x,before.x,sizeof before.x) == 0);
    CHECK(memcmp(f->ram,bytes,sizeof bytes) == 0);
}

typedef struct { const char *name; uint32_t w,a,b,expected; } AluCase;
static void alu_case(const AluCase *t)
{
    Fixture f; fresh(&f,256,t->w);
    f.cpu.x[1] = t->a; f.cpu.x[2] = t->b;
    uint8_t before[sizeof f.ram]; memcpy(before,f.ram,sizeof before);
    CHECK(rv_step(&f.cpu,NULL) == RV_OK);
    if (f.cpu.x[3] != t->expected) {
        fprintf(stderr,"%s got 0x%08" PRIx32 " expected 0x%08" PRIx32 "\n",
                t->name,f.cpu.x[3],t->expected);
    }
    CHECK(f.cpu.x[3] == t->expected && f.cpu.pc == 4u);
    CHECK(f.cpu.x[1] == t->a && f.cpu.x[2] == t->b && f.cpu.x[0] == 0u);
    CHECK(memcmp(before,f.ram,sizeof before) == 0);
    /* Every computational family must also discard a write to x0. */
    fresh(&f,256,t->w & ~UINT32_C(0xf80));
    f.cpu.x[1] = t->a; f.cpu.x[2] = t->b;
    CHECK(rv_step(&f.cpu,NULL) == RV_OK && f.cpu.x[0] == 0u);
}

static void test_alu(void)
{
    const AluCase cases[] = {
        {"ADDI wrap",ei(0x13,0,3,1,1),UINT32_MAX,0,0},
        {"ADDI min immediate",ei(0x13,0,3,1,-2048),0,0,0xfffff800u},
        {"ADDI max immediate",ei(0x13,0,3,1,2047),1,0,2048},
        {"ADD sign boundary",er(0,0,3,1,2),0x7fffffffu,1,0x80000000u},
        {"ADD wrap",er(0,0,3,1,2),UINT32_MAX,1,0},
        {"SUB underflow",er(32,0,3,1,2),0,1,UINT32_MAX},
        {"SUB sign boundary",er(32,0,3,1,2),0x80000000u,1,0x7fffffffu},
        {"SLTI negative",ei(0x13,2,3,1,-1),0x80000000u,0,1},
        {"SLTI equal",ei(0x13,2,3,1,-1),UINT32_MAX,0,0},
        {"SLTIU sign extended immediate",ei(0x13,3,3,1,-1),0x80000000u,0,1},
        {"SLTIU equal",ei(0x13,3,3,1,-1),UINT32_MAX,0,0},
        {"SLT signed",er(0,2,3,1,2),UINT32_MAX,0,1},
        {"SLTU unsigned",er(0,3,3,1,2),UINT32_MAX,0,0},
        {"XOR",er(0,4,3,1,2),0xf0f0u,0xff00u,0x0ff0u},
        {"OR",er(0,6,3,1,2),0xf0f0u,0xff00u,0xfff0u},
        {"AND",er(0,7,3,1,2),0xf0f0u,0xff00u,0xf000u},
        {"XORI",ei(0x13,4,3,1,-1),0x12345678u,0,0xedcba987u},
        {"ORI",ei(0x13,6,3,1,-2048),0x123u,0,0xfffff923u},
        {"ANDI",ei(0x13,7,3,1,-2048),0x12345678u,0,0x12345000u},
        {"LUI",0xabcde1b7u,0,0,0xabcde000u},
        {"AUIPC",0xfffff197u,0,0,0xfffff000u}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) alu_case(&cases[i]);
    /* All six shift forms, every legal count. rs2 high bits must be ignored.
     * A signed 64-bit division reference checks SRA rounding toward -infinity.
     */
    for (unsigned n = 0; n < 32u; ++n) {
        const uint32_t a = 0x80000001u;
        const uint32_t left = (uint32_t)((uint64_t)a << n);
        const uint32_t right = a >> n;
        const int64_t signed_a = (int64_t)a - INT64_C(0x100000000);
        const int64_t divisor = INT64_C(1) << n;
        const uint32_t aright = (uint32_t)((signed_a - (divisor - 1)) / divisor);
        const AluCase shifts[] = {
            {"SLL",er(0,1,3,1,2),a,32u+n,left},
            {"SRL",er(0,5,3,1,2),a,32u+n,right},
            {"SRA",er(32,5,3,1,2),a,32u+n,aright},
            {"SLLI",ei(0x13,1,3,1,(int32_t)n),a,0,left},
            {"SRLI",ei(0x13,5,3,1,(int32_t)n),a,0,right},
            {"SRAI",ei(0x13,5,3,1,(int32_t)(1024u+n)),a,0,aright},
            {"SRA positive",er(32,5,3,1,2),0x7fffffffu,n,0x7fffffffu>>n}
        };
        for (unsigned k = 0; k < sizeof shifts / sizeof shifts[0]; ++k) alu_case(&shifts[k]);
    }
    Fixture f; fresh(&f,256,0); word(&f,64,0xfffff197u); f.cpu.pc=64;
    CHECK(rv_step(&f.cpu,NULL) == RV_OK && f.cpu.x[3] == 0xfffff040u);
    fresh(&f,256,ei(0x13,0,1,1,-1)); f.cpu.x[1]=0;
    CHECK(rv_step(&f.cpu,NULL) == RV_OK && f.cpu.x[1] == UINT32_MAX);
    fresh(&f,256,er(0,0,3,0,2)); f.cpu.x[2]=55;
    CHECK(rv_step(&f.cpu,NULL) == RV_OK && f.cpu.x[3] == 55u);
    puts("PASS arithmetic, logic, signedness, upper immediates, all shift counts, x0");
}

static int64_t signed_value(uint32_t x)
{ return x <= INT32_MAX ? (int64_t)x : (int64_t)x - INT64_C(0x100000000); }

static void test_control(void)
{
    const uint32_t values[] = {0,1,UINT32_MAX,0x80000000u,0x7fffffffu};
    const uint32_t kinds[] = {0,1,4,5,6,7};
    Fixture f;
    CHECK(eb(1,4,0,-20) == 0xfe0216e3u);
    CHECK(ej(1,56) == 0x038000efu);
    CHECK(ej(1,-1048576) == 0x800000efu);
    for (unsigned k=0; k<6u; ++k) for (unsigned i=0; i<5u; ++i)
    for (unsigned j=0; j<5u; ++j) {
        const uint32_t a=values[i], b=values[j], kind=kinds[k];
        const bool taken[] = {a==b,a!=b,signed_value(a)<signed_value(b),
                              signed_value(a)>=signed_value(b),a<b,a>=b};
        fresh(&f,256,eb(kind,1,2,8)); f.cpu.x[1]=a; f.cpu.x[2]=b;
        uint32_t regs[32]; memcpy(regs,f.cpu.x,sizeof regs);
        uint8_t bytes[sizeof f.ram]; memcpy(bytes,f.ram,sizeof bytes);
        CHECK(rv_step(&f.cpu,NULL) == RV_OK);
        CHECK(f.cpu.pc == (taken[k] ? 8u : 4u));
        CHECK(memcmp(regs,f.cpu.x,sizeof regs) == 0);
        CHECK(memcmp(bytes,f.ram,sizeof bytes) == 0);
        fresh(&f,256,eb(kind,1,2,2)); f.cpu.x[1]=a; f.cpu.x[2]=b;
        if (taken[k]) reject(&f,RV_INSTRUCTION_MISALIGNED);
        else CHECK(rv_step(&f.cpu,NULL) == RV_OK && f.cpu.pc==4u);
    }
    const int32_t boff[] = {-4096,-2048,-4,0,4,2048,4092};
    for (unsigned i=0; i<sizeof boff/sizeof boff[0]; ++i) {
        fresh(&f,256,eb(0,0,0,boff[i]));
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==(uint32_t)boff[i]);
    }
    /* Literal encodings exercise scattered immediate bits independently of
     * the test encoders, including a target reported on an alignment trap. */
    const uint32_t golden_b[]={0x000000e3u,0x80000063u};
    const uint32_t golden_target[]={2048,0xfffff000u};
    for (unsigned i=0;i<2u;++i) {
        fresh(&f,256,golden_b[i]);
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==golden_target[i]);
    }
    fresh(&f,256,0x7e000fe3u);
    RvTrap alignment;
    CHECK(rv_step(&f.cpu,&alignment)==RV_INSTRUCTION_MISALIGNED);
    CHECK(alignment.pc==0u && alignment.address==4094u && alignment.word==0x7e000fe3u);
    CHECK(f.cpu.pc==0u);
    fresh(&f,256,0x001000efu); /* JAL ra,+2048: imm[11] is instruction bit 20 */
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==2048u && f.cpu.x[1]==4u);
    fresh(&f,256,0x000800efu); /* JAL ra,+524288: upper immediate bits */
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==524288u);
    const int32_t joff[] = {-1048576,-2048,-4,0,4,2048,524288,1048572};
    for (unsigned i=0; i<sizeof joff/sizeof joff[0]; ++i) {
        fresh(&f,256,ej(1,joff[i]));
        CHECK(rv_step(&f.cpu,NULL)==RV_OK);
        CHECK(f.cpu.pc==(uint32_t)joff[i] && f.cpu.x[1]==4u);
    }
    fresh(&f,256,ej(1,1048574)); reject(&f,RV_INSTRUCTION_MISALIGNED);
    fresh(&f,256,ej(0,8)); CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[0]==0u);
    fresh(&f,256,ei(0x67,0,1,1,-4)); f.cpu.x[1]=17;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==12u && f.cpu.x[1]==4u);
    fresh(&f,256,ei(0x67,0,0,1,0)); f.cpu.x[1]=3;
    reject(&f,RV_INSTRUCTION_MISALIGNED);
    fresh(&f,256,ei(0x67,0,0,1,0)); f.cpu.x[1]=13;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==12u && f.cpu.x[0]==0u);
    fresh(&f,256,ei(0x67,0,1,1,1)); f.cpu.x[1]=UINT32_MAX;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==0u && f.cpu.x[1]==4u);
    /* Aligned out-of-RAM targets commit; the following fetch faults. */
    for (unsigned k=0; k<3u; ++k) {
        fresh(&f,256,k==0u ? ej(1,256) : k==1u ? eb(0,0,0,256) : ei(0x67,0,1,0,256));
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==256u);
        reject(&f,RV_INSTRUCTION_ACCESS);
    }
    fresh(&f,256,eb(0,0,0,-4)); CHECK(rv_step(&f.cpu,NULL)==RV_OK);
    CHECK(f.cpu.pc==0xfffffffcu); reject(&f,RV_INSTRUCTION_ACCESS);
    fresh(&f,256,0); f.cpu.pc=2; reject(&f,RV_INSTRUCTION_MISALIGNED);
    fresh(&f,256,0); f.cpu.pc=0xfffffffcu; reject(&f,RV_INSTRUCTION_ACCESS);
    puts("PASS all branches, JAL/JALR, link aliasing, immediate boundaries, fetch faults");
}

static void test_memory(void)
{
    const unsigned kinds[] = {0,1,2,4,5}, widths[] = {1,2,4,1,2};
    const uint32_t want[] = {0xffffff80u,0xffffff80u,0x8123ff80u,128,65408};
    Fixture f;
    for (unsigned k=0; k<5u; ++k) {
        fresh(&f,256,ei(0x03,kinds[k],3,1,-4)); f.cpu.x[1]=104;
        word(&f,100,0x8123ff80u);
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[3]==want[k]);
        fresh(&f,256,ei(0x03,kinds[k],0,1,0)); f.cpu.x[1]=100;
        word(&f,100,UINT32_MAX);
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[0]==0u);
        fresh(&f,256,ei(0x03,kinds[k],0,1,0)); f.cpu.x[1]=256;
        reject(&f,RV_LOAD_ACCESS);
        fresh(&f,256,ei(0x03,kinds[k],3,1,0)); f.cpu.x[1]=256u-widths[k];
        memset(f.ram+252,0x7f,4);
        CHECK(rv_step(&f.cpu,NULL)==RV_OK); /* last complete aligned element */
        if (widths[k]>1u) {
            fresh(&f,256,ei(0x03,kinds[k],3,1,0)); f.cpu.x[1]=101;
            reject(&f,RV_LOAD_MISALIGNED);
            /* Capacity not divisible by 4: aligned crossing access must fault. */
            fresh(&f,257,ei(0x03,kinds[k],3,1,0)); f.cpu.x[1]=256;
            reject(&f,RV_LOAD_ACCESS);
        }
    }
    fresh(&f,256,ei(0x03,0,3,0,100)); f.ram[100]=0x7f;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[3]==127u);
    fresh(&f,256,ei(0x03,1,3,0,100)); f.ram[100]=0xff; f.ram[101]=0x7f;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[3]==32767u);
    for (unsigned k=0; k<3u; ++k) {
        const unsigned width=1u<<k;
        fresh(&f,256,es(k,1,2,-4)); f.cpu.x[1]=104; f.cpu.x[2]=0x89abcdefu;
        memset(f.ram+99,0xa5,6);
        uint32_t regs[32]; memcpy(regs,f.cpu.x,sizeof regs);
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==4u);
        for (unsigned j=0; j<width; ++j) CHECK(f.ram[100u+j]==(uint8_t)(0x89abcdefu>>(8u*j)));
        CHECK(f.ram[99]==0xa5u && f.ram[100u+width]==0xa5u);
        CHECK(memcmp(regs,f.cpu.x,sizeof regs)==0);
        fresh(&f,256,es(k,1,2,0)); f.cpu.x[1]=256u-width;
        CHECK(rv_step(&f.cpu,NULL)==RV_OK);
        fresh(&f,256,es(k,1,2,0)); f.cpu.x[1]=256;
        reject(&f,RV_STORE_ACCESS);
        if (width>1u) {
            fresh(&f,256,es(k,1,2,0)); f.cpu.x[1]=101;
            reject(&f,RV_STORE_MISALIGNED);
            fresh(&f,257,es(k,1,2,0)); f.cpu.x[1]=256; f.cpu.x[2]=UINT32_MAX;
            reject(&f,RV_STORE_ACCESS);
        }
    }
    /* Offset extremes, address wrapping, and byte / word endianness. */
    fresh(&f,256,ei(0x03,4,3,1,-2048)); f.cpu.x[1]=2148; f.ram[100]=42;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[3]==42u);
    fresh(&f,256,es(0,1,2,2047)); f.cpu.x[1]=0xfffff865u; f.cpu.x[2]=91;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.ram[100]==91u);
    fresh(&f,256,ei(0x03,4,3,1,1)); f.cpu.x[1]=UINT32_MAX;
    CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.x[3]==0x83u);
    fresh(&f,256,ei(0x03,4,3,1,0)); f.cpu.x[1]=UINT32_MAX;
    reject(&f,RV_LOAD_ACCESS);
    fresh(&f,256,es(2,1,2,0)); f.cpu.x[1]=0xfffffffcu;
    reject(&f,RV_STORE_ACCESS);
    /* Host loader checks the complete image before copying. */
    fresh(&f,256,0); const uint8_t image[]={1,2,3,4};
    CHECK(rv_load_image(&f.cpu,252,image,4));
    uint32_t value=0; CHECK(rv_peek_u32(&f.cpu,252,&value) && value==0x04030201u);
    CHECK(!rv_load_image(&f.cpu,254,image,4) && f.ram[254]==3u);
    CHECK(!rv_load_image(&f.cpu,0xfffffffcu,image,4));
    CHECK(!rv_peek_u32(&f.cpu,254,&value) && value==0x04030201u);
    CHECK(!rv_init(&f.cpu,f.ram,3)); CHECK(!rv_init(&f.cpu,NULL,256));
    puts("PASS all load/store widths, signed extension, bounds, atomic faults, configurable RAM");
}

static void test_system_and_illegal(void)
{
    Fixture f;
    fresh(&f,256,0x00000073u); reject(&f,RV_ECALL);
    fresh(&f,256,0x00100073u); reject(&f,RV_EBREAK);
    /* Every fm mode, including reserved encodings, is a conservative FENCE.
     * Nonzero rd and rs1 must be ignored, not interpreted as data operands. */
    for (unsigned fm=0; fm<16u; ++fm) {
        fresh(&f,256,(fm<<28) | 0x0330808fu); f.cpu.x[1]=123;
        CHECK(rv_step(&f.cpu,NULL)==RV_OK && f.cpu.pc==4u && f.cpu.x[1]==123u);
    }
    fresh(&f,256,0x0100000fu); CHECK(rv_step(&f.cpu,NULL)==RV_OK); /* PAUSE hint */
    const uint32_t bad[] = {0,UINT32_MAX,0x00000001u,0x0000100fu, /* FENCE.I */
        0x300110f3u,0x300120f3u,0x300130f3u,0x300150f3u,0x300160f3u,0x300170f3u,
        0x10500073u,0x30200073u,0x000000f3u,0x00108073u,0x0000202fu,0x00000053u};
    for (unsigned i=0; i<sizeof bad/sizeof bad[0]; ++i) {
        fresh(&f,256,bad[i]); reject(&f,RV_ILLEGAL_INSTRUCTION);
    }
    for (unsigned f7=0; f7<128u; ++f7) for (unsigned f3=0; f3<8u; ++f3) {
        if (f7==0u || (f7==32u && (f3==0u || f3==5u))) continue;
        fresh(&f,256,er(f7,f3,0,1,2)); reject(&f,RV_ILLEGAL_INSTRUCTION);
    }
    for (unsigned f7=0; f7<128u; ++f7) {
        if (f7!=0u) { fresh(&f,256,ei(0x13,1,3,1,(int32_t)(f7<<5))); reject(&f,RV_ILLEGAL_INSTRUCTION); }
        if (f7!=0u && f7!=32u) {
            fresh(&f,256,ei(0x13,5,3,1,(int32_t)(f7<<5))); reject(&f,RV_ILLEGAL_INSTRUCTION);
        }
    }
    for (unsigned f3=0; f3<8u; ++f3) {
        if (f3==3u || f3>=6u) { fresh(&f,256,ei(0x03,f3,0,0,0)); reject(&f,RV_ILLEGAL_INSTRUCTION); }
        if (f3>=3u) { fresh(&f,256,es(f3,0,0,0)); reject(&f,RV_ILLEGAL_INSTRUCTION); }
        if (f3==2u || f3==3u) { fresh(&f,256,eb(f3,0,0,0)); reject(&f,RV_ILLEGAL_INSTRUCTION); }
        if (f3!=0u) {
            fresh(&f,256,ei(0x67,f3,1,0,0)); reject(&f,RV_ILLEGAL_INSTRUCTION);
            fresh(&f,256,ei(0x0f,f3,0,0,0)); reject(&f,RV_ILLEGAL_INSTRUCTION);
        }
    }
    puts("PASS ECALL/EBREAK traps, FENCE semantics, extension and reserved-field rejection");
}

static void test_regression_programs(void)
{
    Fixture f; fresh(&f,256,0);
    const uint32_t copy[]={0x06400093u,0x0c800113u,0x00300213u,0x0000c183u,
        0x00310023u,0x00108093u,0x00110113u,0xfff20213u,0xfe0216e3u};
    for (unsigned i=0;i<sizeof copy/sizeof copy[0];++i) word(&f,4u*i,copy[i]);
    f.ram[100]=5; f.ram[101]=7; f.ram[102]=9; f.ram[199]=0xa5; f.ram[203]=0xa5;
    unsigned steps=0, branches=0;
    while (f.cpu.pc!=36u && steps<100u) {
        if (f.cpu.pc==32u) ++branches;
        if (rv_step(&f.cpu,NULL)!=RV_OK) { CHECK(false); break; }
        ++steps;
    }
    CHECK(steps==21u && branches==3u && f.cpu.pc==36u);
    CHECK(f.cpu.x[1]==103u && f.cpu.x[2]==203u && f.cpu.x[4]==0u && f.cpu.x[3]==9u);
    CHECK(f.ram[200]==5u && f.ram[201]==7u && f.ram[202]==9u);
    CHECK(f.ram[199]==0xa5u && f.ram[203]==0xa5u && f.ram[100]==5u);

    fresh(&f,256,0);
    const uint32_t addr[]={0,4,8,12,64,68,72,76,80,84,128,132};
    const uint32_t code[]={0x10000113u,0x00500513u,0x038000efu,0x00050293u,
        0xff010113u,0x00112623u,0x038000efu,0x00c12083u,0x01010113u,
        0x00008067u,0x00150513u,0x00008067u};
    for (unsigned i=0;i<sizeof code/sizeof code[0];++i) word(&f,addr[i],code[i]);
    const uint32_t pcs[]={0,4,8,64,68,72,128,132,76,80,84,12};
    for (unsigned i=0;i<sizeof pcs/sizeof pcs[0];++i) {
        CHECK(f.cpu.pc==pcs[i]); CHECK(rv_step(&f.cpu,NULL)==RV_OK);
    }
    uint32_t saved=0; CHECK(rv_peek_u32(&f.cpu,252,&saved) && saved==12u);
    CHECK(f.cpu.pc==16u && f.cpu.x[2]==256u && f.cpu.x[1]==12u && f.cpu.x[10]==6u);
    CHECK(f.cpu.x[5]==6u && f.cpu.x[0]==0u);
    puts("PASS step3 byte-copy loop and step4 nested-call regressions on the new core");
}

int main(void)
{
    test_alu(); test_control(); test_memory(); test_system_and_illegal();
    test_regression_programs();
    printf("RV32I: %u checks, %u failures\n",checks,failures);
    return failures==0u ? 0 : 1;
}
