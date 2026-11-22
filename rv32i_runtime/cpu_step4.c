/* Milestone 4: the step-3 machine plus LW, SW, JAL and JALR.
 * Self-contained learning snapshot. Earlier sources are preserved and their
 * complete tests run separately in ./test.sh. See README for the call trace.
 * All checks precede commit: faults cannot change PC, registers or RAM.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RAM_SIZE 256u
typedef struct { uint32_t x[32], pc; uint8_t ram[RAM_SIZE]; } Machine;
typedef enum {
    OK, FETCH_ALIGN, FETCH_RANGE, UNSUPPORTED, DATA_ALIGN, DATA_RANGE, TARGET_ALIGN
} Status;

static uint32_t read_reg(const Machine *m, uint32_t r)
{ return r == 0u ? 0u : m->x[r]; }

/* Sign extension stays in unsigned arithmetic, including negative values. */
static uint32_t sext(uint32_t v, unsigned bits)
{
    const uint32_t sign = UINT32_C(1) << (bits - 1u);
    return (v ^ sign) - sign;
}

static uint32_t read_word(const Machine *m, uint32_t a)
{
    return (uint32_t)m->ram[a] | ((uint32_t)m->ram[a + 1u] << 8)
         | ((uint32_t)m->ram[a + 2u] << 16)
         | ((uint32_t)m->ram[a + 3u] << 24);
}

static void put_word(Machine *m, uint32_t a, uint32_t v)
{
    for (unsigned i = 0; i < 4u; ++i)
        m->ram[a + i] = (uint8_t)(v >> (8u * i));
}

static Status cpu_step(Machine *m)
{
    if ((m->pc & 3u) != 0u) return FETCH_ALIGN;
    if (m->pc > RAM_SIZE - 4u) return FETCH_RANGE;
    const uint32_t w = read_word(m, m->pc);
    const uint32_t op = w & 127u, f3 = (w >> 12) & 7u;
    const uint32_t rd = (w >> 7) & 31u;
    const uint32_t a = read_reg(m, (w >> 15) & 31u);
    const uint32_t b = read_reg(m, (w >> 20) & 31u);
    const uint32_t ii = sext(w >> 20, 12);
    const uint32_t si = sext(((w >> 25) << 5) | ((w >> 7) & 31u), 12);
    uint32_t next = m->pc + 4u, value = 0u, addr = 0u, width = 0u;
    bool write_reg = false, store = false, jump = false;

    if (op == 0x13u && f3 == 0u) {             /* ADDI */
        value = a + ii; write_reg = true;
    } else if (op == 0x03u && (f3 == 4u || f3 == 2u)) { /* LBU/LW */
        addr = a + ii; width = f3 == 4u ? 1u : 4u; write_reg = true;
    } else if (op == 0x23u && (f3 == 0u || f3 == 2u)) { /* SB/SW */
        addr = a + si; width = f3 == 0u ? 1u : 4u; store = true;
    } else if (op == 0x63u && f3 == 1u) {      /* BNE */
        const uint32_t bi = ((w >> 31) << 12) | (((w >> 7) & 1u) << 11)
                          | (((w >> 25) & 63u) << 5) | (((w >> 8) & 15u) << 1);
        if (a != b) { next = m->pc + sext(bi, 13); jump = true; }
    } else if (op == 0x6fu) {                 /* JAL */
        const uint32_t ji = ((w >> 31) << 20) | (w & 0xff000u)
                          | (((w >> 20) & 1u) << 11) | (((w >> 21) & 1023u) << 1);
        next = m->pc + sext(ji, 21); value = m->pc + 4u;
        write_reg = true; jump = true;
    } else if (op == 0x67u && f3 == 0u) {      /* JALR: read old rs1 first */
        next = (a + ii) & ~UINT32_C(1); value = m->pc + 4u;
        write_reg = true; jump = true;
    } else {
        return UNSUPPORTED;
    }

    if (jump && (next & 3u) != 0u) return TARGET_ALIGN;
    if (width != 0u) {
        if ((addr & (width - 1u)) != 0u) return DATA_ALIGN;
        if (addr > RAM_SIZE - width) return DATA_RANGE;
        if (!store) value = width == 1u ? m->ram[addr] : read_word(m, addr);
    }
    /* Commit only after every instruction, alignment and range check. */
    if (store) {
        if (width == 1u) m->ram[addr] = (uint8_t)b;
        else put_word(m, addr, b);
    }
    if (write_reg && rd != 0u) m->x[rd] = value;
    m->pc = next;
    return OK;
}

static uint32_t enc_i(uint32_t op, uint32_t f3, uint32_t rd,
                      uint32_t rs1, int32_t imm)
{
    return (((uint32_t)imm & 4095u) << 20) | (rs1 << 15)
         | (f3 << 12) | (rd << 7) | op;
}
static uint32_t enc_s(uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    const uint32_t u = (uint32_t)imm & 4095u;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15)
         | (f3 << 12) | ((u & 31u) << 7) | 0x23u;
}
static uint32_t enc_j(uint32_t rd, int32_t imm)
{
    const uint32_t u = (uint32_t)imm & 0x1fffffu;
    return (((u >> 20) & 1u) << 31) | (((u >> 1) & 1023u) << 21)
         | (((u >> 11) & 1u) << 20) | (u & 0xff000u) | (rd << 7) | 0x6fu;
}

static unsigned failures;
#define CHECK(c) do { if (!(c)) { ++failures; \
    fprintf(stderr, "step4 FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void reject(Machine *m, Status expected)
{
    const Machine before = *m;
    CHECK(cpu_step(m) == expected);
    CHECK(m->pc == before.pc);
    CHECK(memcmp(m->x, before.x, sizeof m->x) == 0);
    CHECK(memcmp(m->ram, before.ram, sizeof m->ram) == 0);
}

static void nested_calls(void)
{
    Machine m = {0};
    /* main -> A -> B -> A -> main. A saves its old ra on a 16-byte frame. */
    put_word(&m, 0,   0x10000113u); /* addi sp,zero,256 */
    put_word(&m, 4,   0x00500513u); /* addi a0,zero,5 */
    put_word(&m, 8,   0x038000efu); /* jal ra,64 */
    put_word(&m, 12,  0x00050293u); /* addi t0,a0,0 */
    put_word(&m, 64,  0xff010113u); /* A: addi sp,sp,-16 */
    put_word(&m, 68,  0x00112623u); /* sw ra,12(sp) */
    put_word(&m, 72,  0x038000efu); /* jal ra,128 */
    put_word(&m, 76,  0x00c12083u); /* lw ra,12(sp) */
    put_word(&m, 80,  0x01010113u); /* addi sp,sp,16 */
    put_word(&m, 84,  0x00008067u); /* ret */
    put_word(&m, 128, 0x00150513u); /* B: addi a0,a0,1 */
    put_word(&m, 132, 0x00008067u); /* ret */
    m.ram[251] = 0xa5u;
    const uint32_t pcs[] = {0,4,8,64,68,72,128,132,76,80,84,12};
    for (unsigned i = 0; i < sizeof pcs / sizeof pcs[0]; ++i) {
        CHECK(m.pc == pcs[i]);
        CHECK(cpu_step(&m) == OK);
    }
    CHECK(m.pc == 16u && m.x[10] == 6u && m.x[5] == 6u);
    CHECK(m.x[2] == 256u && m.x[1] == 12u && m.x[0] == 0u);
    CHECK(read_word(&m, 252) == 12u && m.ram[251] == 0xa5u);
    printf("step4 nested calls: 12 instructions; a0=6 t0=6 sp=256 ra=12 pc=16\n");
}

static void edge_cases(void)
{
    Machine m = {0};
    CHECK(enc_j(1,56) == 0x038000efu);
    CHECK(enc_s(2,2,1,12) == 0x00112623u);
    CHECK(enc_i(0x03,2,1,2,12) == 0x00c12083u);
    put_word(&m, 64, enc_j(5,-4)); m.pc = 64;
    CHECK(cpu_step(&m) == OK && m.pc == 60u && m.x[5] == 68u);
    m = (Machine){0}; put_word(&m,0,enc_j(0,8));
    CHECK(cpu_step(&m) == OK && m.pc == 8u && m.x[0] == 0u);
    m = (Machine){0}; m.x[1] = 13;
    put_word(&m,0,enc_i(0x67,0,1,1,0)); /* rd == rs1, LSB cleared */
    CHECK(cpu_step(&m) == OK && m.pc == 12u && m.x[1] == 4u);
    m = (Machine){0}; put_word(&m,0,enc_j(1,2)); reject(&m,TARGET_ALIGN);
    m = (Machine){0}; m.x[1] = 3; put_word(&m,0,enc_i(0x67,0,1,1,0));
    reject(&m,TARGET_ALIGN);
    m = (Machine){0}; put_word(&m,0,enc_i(0x67,1,1,0,0)); reject(&m,UNSUPPORTED);
    m = (Machine){0}; put_word(&m,0,enc_j(1,256));
    CHECK(cpu_step(&m) == OK && m.pc == 256u && m.x[1] == 4u);
    reject(&m,FETCH_RANGE);
    m = (Machine){0}; m.x[2] = 256; m.x[3] = 0x89abcdefu;
    put_word(&m,0,enc_s(2,2,3,-4));
    CHECK(cpu_step(&m) == OK && read_word(&m,252) == 0x89abcdefu);
    CHECK(m.ram[252] == 0xefu && m.ram[255] == 0x89u);
    put_word(&m,4,enc_i(0x03,2,3,2,-4)); m.x[3] = 0;
    CHECK(cpu_step(&m) == OK && m.x[3] == 0x89abcdefu);
    const uint32_t bad[] = {253,256,0xfffffffcu};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        m = (Machine){0}; m.x[2] = bad[i];
        put_word(&m,0,enc_s(2,2,3,0));
        reject(&m, i == 0u ? DATA_ALIGN : DATA_RANGE);
        m = (Machine){0}; m.x[2] = bad[i];
        put_word(&m,0,enc_i(0x03,2,0,2,0)); /* even a load to x0 faults */
        reject(&m, i == 0u ? DATA_ALIGN : DATA_RANGE);
    }
    m = (Machine){0}; put_word(&m,0,enc_i(0x03,2,0,0,100)); put_word(&m,100,42);
    CHECK(cpu_step(&m) == OK && m.x[0] == 0u);
}

int main(void)
{
    nested_calls(); edge_cases();
    if (failures != 0u) { fprintf(stderr,"step4: %u failures\n",failures); return 1; }
    puts("step4: all nested-call and edge checks passed");
    return 0;
}
