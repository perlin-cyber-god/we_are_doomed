/*
 * cpu_step2.c -- Milestone 2 of a 32-bit RISC-V (RV32) educational model.
 *
 * SCOPE: exactly three instructions: ADDI, LBU and SB.
 * cpu_step1.c (ADDI only) is left untouched; this file is a new,
 * self-contained copy that grows from it.
 *
 * ---------------------------------------------------------------------
 * WHAT IS NEW COMPARED WITH STEP 1
 * ---------------------------------------------------------------------
 *  - Instructions now touch DATA memory, not just registers.
 *      LBU rd, off(rs1)   load Byte, Unsigned: rd = zero-extended RAM byte
 *      SB  rs2, off(rs1)  Store Byte: RAM byte = low 8 bits of rs2
 *  - A second immediate format, the "S" format, used by SB.
 *  - A new error: a data address outside RAM.
 *
 * The machine is unchanged: 32 x uint32_t registers (x0 reads 0),
 * a 32-bit PC, and 256 bytes of RAM. A guest address is only ever an
 * index into the ram[] array and is range-checked before every access.
 *
 * ---------------------------------------------------------------------
 * ONE STEP (cpu_step)
 * ---------------------------------------------------------------------
 *   1. FETCH      4 bytes at PC, little-endian, PC must be 4-aligned
 *   2. VALIDATE   (opcode, funct3) must be ADDI, LBU or SB
 *   3. EXTRACT    rd / rs1 / rs2 and the right signed immediate
 *   4. COMPUTE    result, or effective address = rs1 + offset (mod 2^32)
 *   5. CHECK      data address must be < 256  (loads and stores only)
 *   6. COMMIT     RAM write (SB), register write (ADDI/LBU), PC += 4
 *
 * Steps 1-5 only READ the machine. Nothing is changed until step 6,
 * so a rejected instruction leaves PC, registers and RAM untouched.
 *
 * Encodings (bit 31 on the left):
 *
 *  I-type (ADDI, LBU):
 *   31          20 19   15 14  12 11    7 6      0
 *  +--------------+-------+------+-------+--------+
 *  |  imm[11:0]   |  rs1  |funct3|  rd   | opcode |
 *  +--------------+-------+------+-------+--------+
 *
 *  S-type (SB):
 *   31      25 24   20 19   15 14  12 11      7 6      0
 *  +----------+-------+-------+------+---------+--------+
 *  | imm[11:5]|  rs2  |  rs1  |funct3| imm[4:0]| opcode |
 *  +----------+-------+-------+------+---------+--------+
 *
 *   In SB, bits 11:7 are the LOW FIVE BITS OF THE OFFSET. They are not
 *   a destination register; SB changes no register at all.
 *
 *   ADDI: opcode 0x13, funct3 0
 *   LBU : opcode 0x03, funct3 4
 *   SB  : opcode 0x23, funct3 0
 *
 * HOST-SIDE TEST LIMIT: the demo stops after exactly four instructions.
 * That is a counter in main(), NOT a guest HALT instruction.
 *
 * Build:  gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step2 cpu_step2.c
 * Run:    ./cpu_step2
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RAM_SIZE  256u
#define NUM_REGS  32u

#define OP_LOAD   0x03u
#define OP_IMM    0x13u
#define OP_STORE  0x23u
#define F3_ADDI   0x0u
#define F3_LBU    0x4u
#define F3_SB     0x0u

/* ------------------------------------------------------------------ */
/* Machine state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t x[NUM_REGS]; /* guest registers; x[0] is kept at 0 */
    uint32_t pc;
    uint8_t  ram[RAM_SIZE];
} Machine;

typedef enum {
    STEP_OK = 0,
    STEP_FETCH_MISALIGNED,
    STEP_FETCH_OUT_OF_RANGE,
    STEP_UNSUPPORTED,
    STEP_DATA_OUT_OF_RANGE
} StepStatus;

typedef enum { KIND_ADDI, KIND_LBU, KIND_SB } InstrKind;

/* Everything one successful step learned; used for the trace. */
typedef struct {
    InstrKind kind;
    uint32_t  pc;
    uint32_t  word;
    uint32_t  rd;        /* ADDI/LBU only; 0 for SB */
    uint32_t  rs1;
    uint32_t  rs2;       /* SB only */
    int32_t   imm;       /* signed offset or immediate */
    uint32_t  src1;      /* value of rs1 */
    uint32_t  src2;      /* value of rs2 (SB) */
    uint32_t  ea;        /* effective data address (LBU/SB) */
    uint32_t  byte;      /* byte read (LBU) or written (SB) */
    bool      writes_reg;
    uint32_t  reg_value; /* value headed for rd */
    uint32_t  next_pc;
} StepInfo;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint32_t reg_read(const Machine *m, uint32_t index)
{
    if (index == 0u || index >= NUM_REGS) {
        return 0u; /* x0 always reads as zero */
    }
    return m->x[index];
}

static void reg_write(Machine *m, uint32_t index, uint32_t value)
{
    if (index == 0u || index >= NUM_REGS) {
        return; /* writes to x0 are discarded */
    }
    m->x[index] = value;
}

/* FETCH: four bytes at a 4-aligned address, lowest address = lowest
 * bits. The range test uses RAM_SIZE - 4 so addr + 4 can never wrap. */
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

static void ram_store_word(Machine *m, uint32_t addr, uint32_t word)
{
    m->ram[addr]      = (uint8_t)(word & 0xFFu);
    m->ram[addr + 1u] = (uint8_t)((word >> 8) & 0xFFu);
    m->ram[addr + 2u] = (uint8_t)((word >> 16) & 0xFFu);
    m->ram[addr + 3u] = (uint8_t)((word >> 24) & 0xFFu);
}

/* SIGN EXTENSION of a 12-bit field without signed shifts: if bit 11 is
 * set the value is field - 4096, otherwise it is the field itself. */
static int32_t sign_extend_12(uint32_t field)
{
    field &= 0xFFFu;
    if ((field & 0x800u) != 0u) {
        return (int32_t)field - 4096;
    }
    return (int32_t)field;
}

/* I-type immediate: bits 31:20 in one piece. */
static int32_t imm_i(uint32_t word)
{
    return sign_extend_12(word >> 20);
}

/* S-type immediate: bits 31:25 are imm[11:5], bits 11:7 are imm[4:0].
 * Glue the two pieces into one 12-bit field, then sign-extend. */
static int32_t imm_s(uint32_t word)
{
    uint32_t high = (word >> 25) & 0x7Fu; /* imm[11:5] */
    uint32_t low  = (word >> 7)  & 0x1Fu; /* imm[4:0]  */
    return sign_extend_12((high << 5) | low);
}

static const char *status_text(StepStatus s)
{
    switch (s) {
    case STEP_OK:                 return "ok";
    case STEP_FETCH_MISALIGNED:   return "instruction fetch is not 4-byte aligned";
    case STEP_FETCH_OUT_OF_RANGE: return "instruction fetch is outside guest RAM";
    case STEP_UNSUPPORTED:        return "unsupported instruction (only ADDI, LBU, SB exist)";
    case STEP_DATA_OUT_OF_RANGE:  return "data address is outside guest RAM";
    }
    return "unknown error";
}

/* ------------------------------------------------------------------ */
/* Machine setup                                                       */
/* ------------------------------------------------------------------ */

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
    ram_store_word(m, 0u,  0x06400093u); /* addi x1, x0, 100 */
    ram_store_word(m, 4u,  0x0000C183u); /* lbu  x3, 0(x1)   */
    ram_store_word(m, 8u,  0x0C800113u); /* addi x2, x0, 200 */
    ram_store_word(m, 12u, 0x00310023u); /* sb   x3, 0(x2)   */

    m->ram[100] = 5u;     /* the byte the program loads              */
    m->ram[200] = 0u;     /* the byte the program stores into        */
    m->ram[199] = 0xA5u;  /* neighbour sentinels: must stay 0xA5     */
    m->ram[201] = 0xA5u;
}

/* ------------------------------------------------------------------ */
/* One instruction                                                     */
/* ------------------------------------------------------------------ */

static StepStatus cpu_step(Machine *m, StepInfo *info)
{
    StepInfo t = {0};
    uint32_t word = 0u;

    /* 1. Fetch. */
    StepStatus st = fetch_word(m, m->pc, &word);
    if (st != STEP_OK) {
        return st;
    }

    /* 2. Validate: the (opcode, funct3) pair selects the instruction.
     *    Matching BOTH matters: the same funct3 value means different
     *    things under different opcodes (funct3 4 is LBU under opcode
     *    0x03 but XORI under 0x13, which we must reject). */
    uint32_t opcode = word & 0x7Fu;
    uint32_t funct3 = (word >> 12) & 0x7u;
    if (opcode == OP_IMM && funct3 == F3_ADDI) {
        t.kind = KIND_ADDI;
    } else if (opcode == OP_LOAD && funct3 == F3_LBU) {
        t.kind = KIND_LBU;
    } else if (opcode == OP_STORE && funct3 == F3_SB) {
        t.kind = KIND_SB;
    } else {
        return STEP_UNSUPPORTED;
    }

    /* 3. Extract fields. */
    t.pc   = m->pc;
    t.word = word;
    t.rs1  = (word >> 15) & 0x1Fu;
    if (t.kind == KIND_SB) {
        t.rs2 = (word >> 20) & 0x1Fu;
        t.rd  = 0u;              /* bits 11:7 are offset bits, not rd */
        t.imm = imm_s(word);
    } else {
        t.rd  = (word >> 7) & 0x1Fu;
        t.imm = imm_i(word);
    }
    t.src1    = reg_read(m, t.rs1);
    t.next_pc = m->pc + 4u;

    /* 4+5. Compute and check. Converting a negative int32_t to
     * uint32_t wraps modulo 2^32 by definition, and unsigned addition
     * wraps too, so the address arithmetic has no signed overflow. */
    switch (t.kind) {
    case KIND_ADDI:
        t.reg_value  = t.src1 + (uint32_t)t.imm;
        t.writes_reg = true;
        break;
    case KIND_LBU:
        t.ea = t.src1 + (uint32_t)t.imm;
        if (t.ea >= RAM_SIZE) {
            return STEP_DATA_OUT_OF_RANGE; /* nothing changed yet */
        }
        t.byte       = m->ram[t.ea];   /* one byte, zero-extended */
        t.reg_value  = t.byte;
        t.writes_reg = true;
        break;
    case KIND_SB:
        t.ea = t.src1 + (uint32_t)t.imm;
        if (t.ea >= RAM_SIZE) {
            return STEP_DATA_OUT_OF_RANGE; /* nothing changed yet */
        }
        t.src2 = reg_read(m, t.rs2);
        t.byte = t.src2 & 0xFFu;       /* keep only the low 8 bits */
        break;
    }

    /* 6. Commit. Every check has passed, so side effects are safe now. */
    if (t.kind == KIND_SB) {
        m->ram[t.ea] = (uint8_t)t.byte;
    }
    if (t.writes_reg) {
        reg_write(m, t.rd, t.reg_value); /* x0 write is discarded here */
    }
    m->pc = t.next_pc;

    if (info != NULL) {
        *info = t;
    }
    return STEP_OK;
}

/* ------------------------------------------------------------------ */
/* Trace                                                               */
/* ------------------------------------------------------------------ */

static void print_reg_write(const StepInfo *t)
{
    if (!t->writes_reg) {
        printf("reg write: none");
    } else if (t->rd == 0u) {
        printf("reg write: x0 discarded (value %" PRIu32 ")", t->reg_value);
    } else {
        printf("reg write: x%" PRIu32 " = %" PRIu32, t->rd, t->reg_value);
    }
}

static void print_trace(unsigned n, const StepInfo *t)
{
    printf("step %u: pc=0x%08" PRIX32 " word=0x%08" PRIX32 " ",
           n, t->pc, t->word);
    switch (t->kind) {
    case KIND_ADDI:
        printf("ADDI x%" PRIu32 ", x%" PRIu32 ", %" PRId32
               " | src=%" PRIu32 " | ",
               t->rd, t->rs1, t->imm, t->src1);
        break;
    case KIND_LBU:
        printf("LBU  x%" PRIu32 ", %" PRId32 "(x%" PRIu32
               ") | ea=%" PRIu32 " | read RAM[%" PRIu32 "]=%" PRIu32 " | ",
               t->rd, t->imm, t->rs1, t->ea, t->ea, t->byte);
        break;
    case KIND_SB:
        printf("SB   x%" PRIu32 ", %" PRId32 "(x%" PRIu32
               ") | ea=%" PRIu32 " | wrote RAM[%" PRIu32 "]=%" PRIu32
               " (rs2=%" PRIu32 ") | ",
               t->rs2, t->imm, t->rs1, t->ea, t->ea, t->byte, t->src2);
        break;
    }
    print_reg_write(t);
    printf(" | next_pc=0x%08" PRIX32 "\n", t->next_pc);
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

static void check_eq(const char *name, uint32_t got, uint32_t want)
{
    if (got == want) {
        printf("  PASS %-22s = %" PRIu32 "\n", name, got);
    } else {
        printf("  FAIL %-22s = %" PRIu32 " (wanted %" PRIu32 ")\n",
               name, got, want);
        g_failures++;
    }
}

/* Field-by-field comparison (no memcmp, so struct padding can't matter). */
static bool machine_equal(const Machine *a, const Machine *b)
{
    if (a->pc != b->pc) {
        return false;
    }
    for (uint32_t i = 0; i < NUM_REGS; i++) {
        if (a->x[i] != b->x[i]) {
            return false;
        }
    }
    for (uint32_t i = 0; i < RAM_SIZE; i++) {
        if (a->ram[i] != b->ram[i]) {
            return false;
        }
    }
    return true;
}

/* Tiny encoders so tests can build instructions. The demo itself uses
 * the literal words from the spec, not these. */
static uint32_t enc_i(uint32_t opcode, uint32_t funct3,
                      uint32_t rd, uint32_t rs1, int32_t imm)
{
    return (((uint32_t)imm & 0xFFFu) << 20) | (rs1 << 15)
         | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t enc_s(uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    uint32_t u = (uint32_t)imm & 0xFFFu;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15)
         | (funct3 << 12) | ((u & 0x1Fu) << 7) | OP_STORE;
}

/* Fresh machine with one instruction at address 0. */
static void setup(Machine *m, uint32_t word)
{
    machine_init(m);
    ram_store_word(m, 0u, word);
}

/* Run one step that must be rejected; verify the status AND that the
 * whole machine (PC, registers, RAM) is exactly as it was. */
static void expect_reject(const char *label, Machine *m, StepStatus want)
{
    Machine before = *m;
    StepStatus got = cpu_step(m, NULL);
    bool same = machine_equal(&before, m);
    printf("  %-30s -> %s%s\n", label, status_text(got),
           same ? " [state unchanged]" : " [STATE CHANGED]");
    if (got != want || !same) {
        printf("  FAIL: wanted a different status or state changed\n");
        g_failures++;
    }
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_encoders_match_demo_words(void)
{
    printf("\nTest 0: test encoders reproduce the spec's words\n");
    check_eq("addi x1,x0,100", enc_i(OP_IMM, F3_ADDI, 1, 0, 100), 0x06400093u);
    check_eq("lbu x3,0(x1)",   enc_i(OP_LOAD, F3_LBU, 3, 1, 0),   0x0000C183u);
    check_eq("addi x2,x0,200", enc_i(OP_IMM, F3_ADDI, 2, 0, 200), 0x0C800113u);
    check_eq("sb x3,0(x2)",    enc_s(F3_SB, 2, 3, 0),             0x00310023u);
}

/* Step 1 test, kept: ADDI cannot change x0. */
static void test_addi_cannot_change_x0(void)
{
    printf("\nTest (step 1): ADDI cannot change x0\n");
    Machine m;
    setup(&m, enc_i(OP_IMM, F3_ADDI, 0, 1, 7)); /* addi x0, x1, 7 */
    m.x[1] = 100u;
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("raw result", t.reg_value, 107u); /* computed, then discarded */
    check_eq("x0", m.x[0], 0u);
    check_eq("x1", m.x[1], 100u);
    check_eq("pc", m.pc, 4u);
}

/* Step 1 tests, kept (plus new ones for the new instructions). */
static void test_rejections(void)
{
    printf("\nTest (step 1 + new): rejecting bad instructions and fetches\n");
    Machine m;

    setup(&m, 0x00000033u); /* add: opcode 0x33 */
    expect_reject("opcode 0x33 (add)", &m, STEP_UNSUPPORTED);

    setup(&m, 0x0000C093u); /* xori x1,x1,0: opcode 0x13, funct3 4 */
    expect_reject("OP-IMM funct3=4 (xori)", &m, STEP_UNSUPPORTED);

    setup(&m, enc_i(OP_LOAD, 0, 3, 1, 0)); /* lb: funct3 0 */
    expect_reject("load funct3=0 (lb)", &m, STEP_UNSUPPORTED);

    setup(&m, enc_i(OP_LOAD, 2, 3, 1, 0)); /* lw: funct3 2 */
    expect_reject("load funct3=2 (lw)", &m, STEP_UNSUPPORTED);

    setup(&m, enc_s(1, 2, 3, 0)); /* sh: funct3 1 */
    expect_reject("store funct3=1 (sh)", &m, STEP_UNSUPPORTED);

    setup(&m, enc_s(2, 2, 3, 0)); /* sw: funct3 2 */
    expect_reject("store funct3=2 (sw)", &m, STEP_UNSUPPORTED);

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

/* New test 1 */
static void test_lbu_zero_extends(void)
{
    printf("\nTest 1: LBU of 0xF0 produces 240 (not a negative number)\n");
    Machine m;
    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, 0)); /* lbu x3, 0(x1) */
    m.x[1] = 50u;
    m.ram[50] = 0xF0u;
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("x3", m.x[3], 240u);
    check_eq("RAM[50] unchanged", m.ram[50], 0xF0u);
}

/* New test 2 */
static void test_sb_stores_low_byte_only(void)
{
    printf("\nTest 2: SB of 0x123456AB stores only 0xAB\n");
    Machine m;
    setup(&m, enc_s(F3_SB, 2, 3, 0)); /* sb x3, 0(x2) */
    m.x[2] = 200u;
    m.x[3] = 0x123456ABu;
    m.ram[199] = 0xA5u;
    m.ram[201] = 0xA5u;
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("RAM[200]", m.ram[200], 0xABu);
    check_eq("RAM[199]", m.ram[199], 0xA5u);
    check_eq("RAM[201]", m.ram[201], 0xA5u);
    check_eq("x3 unchanged", m.x[3], 0x123456ABu);
}

/* New test 3 */
static void test_negative_offsets(void)
{
    printf("\nTest 3: offset -4 works for LBU and SB\n");
    Machine m;

    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, -4)); /* lbu x3, -4(x1) */
    m.x[1] = 104u;
    m.ram[100] = 0x7Eu;
    StepInfo t;
    check_eq("LBU status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("LBU ea", t.ea, 100u);
    check_eq("LBU x3", m.x[3], 0x7Eu);

    setup(&m, enc_s(F3_SB, 2, 3, -4)); /* sb x3, -4(x2) */
    m.x[2] = 104u;
    m.x[3] = 0x42u;
    m.ram[99] = 0xA5u;
    m.ram[101] = 0xA5u;
    check_eq("SB status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("SB ea", t.ea, 100u);
    check_eq("RAM[100]", m.ram[100], 0x42u);
    check_eq("RAM[99]", m.ram[99], 0xA5u);
    check_eq("RAM[101]", m.ram[101], 0xA5u);
}

/* New test 4 */
static void test_sb_immediate_bits_are_not_rd(void)
{
    printf("\nTest 4: SB with nonzero bits 11:7 changes no register\n");
    Machine m;
    uint32_t word = enc_s(F3_SB, 2, 3, 37); /* imm = 37 = (1<<5) | 5 */
    setup(&m, word);
    m.x[2] = 100u;
    m.x[3] = 0x11223344u;
    for (uint32_t i = 4; i < NUM_REGS; i++) {
        m.x[i] = 0x1000u + i;   /* distinct values so any stray write shows */
    }
    uint32_t regs_before[NUM_REGS];
    for (uint32_t i = 0; i < NUM_REGS; i++) {
        regs_before[i] = m.x[i];
    }

    check_eq("word bits 11:7", (word >> 7) & 0x1Fu, 5u); /* nonzero, as required */
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("decoded imm", (uint32_t)t.imm, 37u);
    check_eq("ea (100+37)", t.ea, 137u);
    check_eq("RAM[137]", m.ram[137], 0x44u);
    check_eq("RAM[105] untouched", m.ram[105], 0u); /* x5's number is not an address */

    uint32_t changed = 0u;
    for (uint32_t i = 0; i < NUM_REGS; i++) {
        if (m.x[i] != regs_before[i]) {
            changed++;
        }
    }
    check_eq("registers changed", changed, 0u);
}

/* New test 5 */
static void test_address_bounds(void)
{
    printf("\nTest 5: byte address 255 works; 256 and 0xFFFFFFFF are rejected\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, 0)); /* lbu x3, 0(x1) */
    m.x[1] = 255u;
    m.ram[255] = 0x77u;
    check_eq("LBU@255 status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("LBU@255 x3", m.x[3], 0x77u);

    setup(&m, enc_s(F3_SB, 2, 3, 0)); /* sb x3, 0(x2) */
    m.x[2] = 255u;
    m.x[3] = 0x99u;
    m.ram[254] = 0xA5u;
    check_eq("SB@255 status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("RAM[255]", m.ram[255], 0x99u);
    check_eq("RAM[254] untouched", m.ram[254], 0xA5u);

    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, 0));
    m.x[1] = 256u;
    expect_reject("LBU @256", &m, STEP_DATA_OUT_OF_RANGE);

    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, 0));
    m.x[1] = 0xFFFFFFFFu;
    expect_reject("LBU @0xFFFFFFFF", &m, STEP_DATA_OUT_OF_RANGE);

    setup(&m, enc_s(F3_SB, 2, 3, 0));
    m.x[2] = 256u;
    m.x[3] = 0x55u;
    expect_reject("SB @256", &m, STEP_DATA_OUT_OF_RANGE);

    setup(&m, enc_s(F3_SB, 2, 3, 0));
    m.x[2] = 0xFFFFFFFFu;
    m.x[3] = 0x55u;
    expect_reject("SB @0xFFFFFFFF", &m, STEP_DATA_OUT_OF_RANGE);
}

/* New test 6 (plus the valid-load-to-x0 case) */
static void test_load_into_x0(void)
{
    printf("\nTest 6: loads targeting x0\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_i(OP_LOAD, F3_LBU, 0, 1, 0)); /* lbu x0, 0(x1), bad address */
    m.x[1] = 256u;
    expect_reject("invalid load to x0", &m, STEP_DATA_OUT_OF_RANGE);

    setup(&m, enc_i(OP_LOAD, F3_LBU, 0, 1, 0)); /* lbu x0, 0(x1), good address */
    m.x[1] = 100u;
    m.ram[100] = 5u;
    check_eq("valid load status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("byte was read", t.byte, 5u);   /* the access really happened */
    check_eq("x0 still zero", m.x[0], 0u);   /* ...but the write was dropped */
    check_eq("pc", m.pc, 4u);
}

/* Test 7 is built into expect_reject(): every rejection above also
 * proves PC, all 32 registers and all 256 RAM bytes are unchanged. */

/* ------------------------------------------------------------------ */
/* Main demonstration                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    Machine m;
    machine_init(&m);
    load_demo_program(&m);

    printf("Running the demo program\n");

    /* HOST-SIDE TEST LIMIT: exactly four completed instructions.
     * This loop bound is NOT a guest HALT instruction. */
    const unsigned host_test_limit = 4u;
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
    check_eq("x2", m.x[2], 200u);
    check_eq("x3", m.x[3], 5u);
    check_eq("x0", m.x[0], 0u);
    check_eq("PC", m.pc, 16u);
    check_eq("RAM[100]", m.ram[100], 5u);
    check_eq("RAM[200]", m.ram[200], 5u);
    check_eq("RAM[199] sentinel", m.ram[199], 0xA5u);
    check_eq("RAM[201] sentinel", m.ram[201], 0xA5u);

    test_encoders_match_demo_words();
    test_addi_cannot_change_x0();
    test_rejections();
    test_lbu_zero_extends();
    test_sb_stores_low_byte_only();
    test_negative_offsets();
    test_sb_immediate_bits_are_not_rd();
    test_address_bounds();
    test_load_into_x0();

    if (g_failures != 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
