/*
 * cpu_step1.c -- Milestone 1 of a 32-bit RISC-V (RV32) educational model.
 *
 * SCOPE: this milestone supports ONLY the ADDI instruction.
 *
 * ---------------------------------------------------------------------
 * DESIGN (read this before the code)
 * ---------------------------------------------------------------------
 * The guest machine is plain data inside one C struct:
 *
 *   - 32 integer registers, each a uint32_t (x0..x31).
 *   - one 32-bit program counter (PC).
 *   - 256 bytes of RAM, a uint8_t array.
 *
 * A guest address is just an INDEX into that RAM array. We never turn a
 * guest address into a native host pointer; every access goes through a
 * helper that checks the range first.
 *
 * One call to cpu_step() executes exactly one instruction:
 *
 *   1. FETCH      read 4 real bytes from RAM at PC (little-endian)
 *   2. RECONSTRUCT build the 32-bit instruction word from those bytes
 *   3. VALIDATE   opcode must be 0x13 and funct3 must be 0 (ADDI)
 *   4. EXTRACT    rd, rs1 and the signed 12-bit immediate
 *   5. READ       source register rs1 (x0 always reads as 0)
 *   6. COMPUTE    32-bit wrap-around add, done in uint32_t
 *   7. WRITEBACK  store into rd, unless rd is x0
 *   8. ADVANCE    PC = PC + 4
 *
 * If any step fails, cpu_step() returns an error code BEFORE changing
 * any guest state, so a rejected instruction leaves the machine intact.
 *
 * ADDI layout (I-type), bit 31 on the left:
 *
 *   31          20 19   15 14  12 11    7 6      0
 *  +--------------+-------+------+-------+--------+
 *  |  imm[11:0]   |  rs1  |funct3|  rd   | opcode |
 *  +--------------+-------+------+-------+--------+
 *       12 bits     5 bits 3 bits  5 bits  7 bits
 *   ADDI: opcode = 0x13 (0010011), funct3 = 0
 *
 * HOST-SIDE TEST LIMIT: the demo stops after exactly two instructions.
 * That is just a counter in main(). It is NOT a guest HALT instruction;
 * the guest CPU has no way to stop itself yet.
 *
 * Build:  gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step1 cpu_step1.c
 * Run:    ./cpu_step1
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RAM_SIZE      256u
#define NUM_REGS      32u
#define OPCODE_OP_IMM 0x13u /* the opcode group ADDI belongs to */
#define FUNCT3_ADDI   0x0u

/* ------------------------------------------------------------------ */
/* Machine state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t x[NUM_REGS]; /* guest registers; x[0] is kept at 0 */
    uint32_t pc;          /* guest program counter */
    uint8_t  ram[RAM_SIZE];
} Machine;

typedef enum {
    STEP_OK = 0,
    STEP_FETCH_MISALIGNED,
    STEP_FETCH_OUT_OF_RANGE,
    STEP_UNSUPPORTED
} StepStatus;

/* Everything one step learned; filled in for the trace. */
typedef struct {
    uint32_t pc;
    uint32_t word;
    uint32_t rd;
    uint32_t rs1;
    int32_t  imm;
    uint32_t src;
    uint32_t result;
    uint32_t next_pc;
} StepInfo;

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/* Registers: x0 always reads as zero. The index is only 5 bits wide
 * when it comes from an instruction, but we still guard it. */
static uint32_t reg_read(const Machine *m, uint32_t index)
{
    if (index == 0u || index >= NUM_REGS) {
        return 0u;
    }
    return m->x[index];
}

/* Writes to x0 are silently discarded: that is what the architecture
 * says. Writes to a non-existent register are discarded too. */
static void reg_write(Machine *m, uint32_t index, uint32_t value)
{
    if (index == 0u || index >= NUM_REGS) {
        return;
    }
    m->x[index] = value;
}

/* FETCHING: read four actual bytes at guest address `addr` and rebuild
 * the 32-bit word. RISC-V is little-endian: the byte at the LOWEST
 * address holds the LOWEST 8 bits of the word.
 *
 * Checks, in order:
 *   - addr must be a multiple of 4 (no compressed instructions here)
 *   - all four bytes must lie inside RAM. We compare against
 *     RAM_SIZE - 4 instead of computing addr + 4, so a huge address
 *     such as 0xFFFFFFFC cannot wrap around and sneak past the check. */
static StepStatus fetch_word(const Machine *m, uint32_t addr, uint32_t *word)
{
    if ((addr & 3u) != 0u) {
        return STEP_FETCH_MISALIGNED;
    }
    if (addr > RAM_SIZE - 4u) {
        return STEP_FETCH_OUT_OF_RANGE;
    }
    *word = (uint32_t)m->ram[addr]
          | ((uint32_t)m->ram[addr + 1u] << 8)
          | ((uint32_t)m->ram[addr + 2u] << 16)
          | ((uint32_t)m->ram[addr + 3u] << 24);
    return STEP_OK;
}

/* Store a 32-bit word as four little-endian bytes (used to preload code). */
static void ram_store_word(Machine *m, uint32_t addr, uint32_t word)
{
    m->ram[addr]      = (uint8_t)(word & 0xFFu);
    m->ram[addr + 1u] = (uint8_t)((word >> 8) & 0xFFu);
    m->ram[addr + 2u] = (uint8_t)((word >> 16) & 0xFFu);
    m->ram[addr + 3u] = (uint8_t)((word >> 24) & 0xFFu);
}

/* SIGN EXTENSION of a 12-bit field, with no signed shifts.
 *
 * `field` holds 0..4095. In 12-bit two's complement, bit 11 is the sign
 * bit, and values 2048..4095 mean -2048..-1. So:
 *     if bit 11 is set:  value = field - 4096
 *     otherwise:         value = field
 * Both branches stay within int32_t, so there is no overflow, and we
 * never right-shift a negative number (whose result is implementation
 * defined in C). */
static int32_t sign_extend_12(uint32_t field)
{
    field &= 0xFFFu;
    if ((field & 0x800u) != 0u) {
        return (int32_t)field - 4096;
    }
    return (int32_t)field;
}

static const char *status_text(StepStatus s)
{
    switch (s) {
    case STEP_OK:                 return "ok";
    case STEP_FETCH_MISALIGNED:   return "instruction fetch is not 4-byte aligned";
    case STEP_FETCH_OUT_OF_RANGE: return "instruction fetch is outside guest RAM";
    case STEP_UNSUPPORTED:        return "unsupported instruction (only ADDI exists)";
    }
    return "unknown error";
}

/* ------------------------------------------------------------------ */
/* Machine setup                                                       */
/* ------------------------------------------------------------------ */

/* Deterministic start state for this test machine: every register 0,
 * PC 0, all RAM 0, then the test program and data are preloaded. */
static void machine_init(Machine *m)
{
    for (uint32_t i = 0; i < NUM_REGS; i++) {
        m->x[i] = 0u;
    }
    m->pc = 0u;
    for (uint32_t i = 0; i < RAM_SIZE; i++) {
        m->ram[i] = 0u;
    }
}

static void load_demo_program(Machine *m)
{
    ram_store_word(m, 0u, 0x06400093u); /* addi x1, x0, 100 */
    ram_store_word(m, 4u, 0xFFC08113u); /* addi x2, x1, -4  */
    m->ram[100] = 5u;                   /* data byte the program must not touch */
}

/* ------------------------------------------------------------------ */
/* One instruction                                                     */
/* ------------------------------------------------------------------ */

static StepStatus cpu_step(Machine *m, StepInfo *info)
{
    StepStatus st;
    uint32_t word = 0u;

    /* 1+2. Fetch four bytes at PC and rebuild the instruction word. */
    st = fetch_word(m, m->pc, &word);
    if (st != STEP_OK) {
        return st;
    }

    /* 3. Validate: opcode is the low 7 bits, funct3 is bits 14..12. */
    uint32_t opcode = word & 0x7Fu;
    uint32_t funct3 = (word >> 12) & 0x7u;
    if (opcode != OPCODE_OP_IMM || funct3 != FUNCT3_ADDI) {
        return STEP_UNSUPPORTED;
    }

    /* 4. Extract bit fields: shift the field down to bit 0, then mask. */
    uint32_t rd  = (word >> 7)  & 0x1Fu;  /* bits 11..7  */
    uint32_t rs1 = (word >> 15) & 0x1Fu;  /* bits 19..15 */
    int32_t  imm = sign_extend_12(word >> 20); /* bits 31..20 */

    /* 5. Read the source register (x0 reads as zero). */
    uint32_t src = reg_read(m, rs1);

    /* 6. Compute. Converting a negative int32_t to uint32_t is defined
     *    in C: it wraps modulo 2^32, which is exactly two's complement.
     *    Unsigned addition also wraps by definition, so there is no
     *    signed overflow anywhere. Example: imm = -4 becomes
     *    0xFFFFFFFC, and 100 + 0xFFFFFFFC wraps to 96. */
    uint32_t result = src + (uint32_t)imm;

    /* 7. Writeback (reg_write drops writes to x0). */
    reg_write(m, rd, result);

    /* 8. Advance PC. Nothing here can branch, so the next PC is PC+4. */
    uint32_t next_pc = m->pc + 4u;

    if (info != NULL) {
        info->pc = m->pc;
        info->word = word;
        info->rd = rd;
        info->rs1 = rs1;
        info->imm = imm;
        info->src = src;
        info->result = result;
        info->next_pc = next_pc;
    }
    m->pc = next_pc;
    return STEP_OK;
}

static void print_trace(unsigned n, const StepInfo *t)
{
    printf("step %u: pc=0x%08" PRIX32 " word=0x%08" PRIX32
           " rd=x%" PRIu32 " rs1=x%" PRIu32 " imm=%" PRId32
           " src=%" PRIu32 " result=%" PRIu32 " next_pc=0x%08" PRIX32 "\n",
           n, t->pc, t->word, t->rd, t->rs1, t->imm,
           t->src, t->result, t->next_pc);
}

/* ------------------------------------------------------------------ */
/* Checks                                                              */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

static void check_eq(const char *name, uint32_t got, uint32_t want)
{
    if (got == want) {
        printf("  PASS %-12s = %" PRIu32 "\n", name, got);
    } else {
        printf("  FAIL %-12s = %" PRIu32 " (wanted %" PRIu32 ")\n",
               name, got, want);
        g_failures++;
    }
}

/* Separate test: ADDI must never change x0, even if rd is x0 and the
 * source register holds a nonzero value. */
static void test_addi_cannot_change_x0(void)
{
    printf("\nTest: ADDI cannot change x0\n");
    Machine m;
    machine_init(&m);
    m.x[1] = 100u;
    ram_store_word(&m, 0u, 0x00708013u); /* addi x0, x1, 7 */

    StepInfo t;
    StepStatus st = cpu_step(&m, &t);
    check_eq("step status", (uint32_t)st, (uint32_t)STEP_OK);
    check_eq("rd decoded", t.rd, 0u);
    check_eq("raw result", t.result, 107u); /* computed, then discarded */
    check_eq("x0", m.x[0], 0u);
    check_eq("x1", m.x[1], 100u);
    check_eq("pc", m.pc, 4u);
}

/* A rejected instruction must report the right error and leave the PC
 * and registers untouched. */
static void expect_reject(const char *label, Machine *m, StepStatus want)
{
    uint32_t pc_before = m->pc;
    uint32_t x1_before = m->x[1];
    StepStatus got = cpu_step(m, NULL);
    printf("  %-26s -> %s\n", label, status_text(got));
    if (got != want || m->pc != pc_before || m->x[1] != x1_before) {
        printf("  FAIL: wrong status or state changed\n");
        g_failures++;
    }
}

static void test_rejections(void)
{
    printf("\nTest: rejecting bad instructions and fetches\n");
    Machine m;

    machine_init(&m);
    ram_store_word(&m, 0u, 0x00000033u); /* add x0,x0,x0: opcode 0x33 */
    expect_reject("opcode 0x33 (add)", &m, STEP_UNSUPPORTED);

    machine_init(&m);
    ram_store_word(&m, 0u, 0x0000C093u); /* xori x1,x1,0: funct3 = 4 */
    expect_reject("OP-IMM funct3=4 (xori)", &m, STEP_UNSUPPORTED);

    machine_init(&m);
    m.pc = 2u;
    expect_reject("misaligned pc=2", &m, STEP_FETCH_MISALIGNED);

    machine_init(&m);
    m.pc = RAM_SIZE;
    expect_reject("pc=256 (past RAM)", &m, STEP_FETCH_OUT_OF_RANGE);

    machine_init(&m);
    m.pc = 0xFFFFFFFCu;
    expect_reject("pc=0xFFFFFFFC", &m, STEP_FETCH_OUT_OF_RANGE);
}

/* ------------------------------------------------------------------ */
/* Main demonstration                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    Machine m;
    machine_init(&m);
    load_demo_program(&m);

    printf("Running the demo program\n");

    /* HOST-SIDE TEST LIMIT: run exactly two completed instructions.
     * This loop bound is NOT a guest HALT instruction. */
    const unsigned host_test_limit = 2u;
    for (unsigned n = 1u; n <= host_test_limit; n++) {
        StepInfo t;
        StepStatus st = cpu_step(&m, &t);
        if (st != STEP_OK) {
            printf("step %u failed at pc=0x%08" PRIX32 ": %s\n",
                   n, m.pc, status_text(st));
            return 1;
        }
        print_trace(n, &t);
    }

    printf("\nStopped by the host-side test limit (%u instructions); "
           "this is not a guest HALT.\n", host_test_limit);

    printf("\nFinal checks\n");
    check_eq("x1", m.x[1], 100u);
    check_eq("x2", m.x[2], 96u);
    check_eq("x0", m.x[0], 0u);
    check_eq("PC", m.pc, 8u);
    check_eq("RAM[100]", m.ram[100], 5u);

    test_addi_cannot_change_x0();
    test_rejections();

    if (g_failures != 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
