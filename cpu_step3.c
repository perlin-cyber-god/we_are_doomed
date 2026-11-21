/*
 * cpu_step3.c -- Milestone 3 of a 32-bit RISC-V (RV32) educational model.
 *
 * SCOPE: exactly four instructions: ADDI, LBU, SB and BNE.
 * cpu_step1.c and cpu_step2.c are left untouched; this file is a new,
 * self-contained copy that grows from cpu_step2.c.
 *
 * ---------------------------------------------------------------------
 * WHAT IS NEW COMPARED WITH STEP 2
 * ---------------------------------------------------------------------
 *  - BNE rs1, rs2, offset   "Branch if Not Equal"
 *      if (rs1 != rs2) next PC = (this instruction's PC) + offset
 *      else            next PC = (this instruction's PC) + 4
 *    BNE is the first instruction that can make the PC do something
 *    other than "+4". That one ability turns a straight list of
 *    instructions into a LOOP.
 *  - A third immediate format, the "B" format.
 *  - A new error: a taken branch whose target is not 4-byte aligned.
 *  - A host-side run loop with a step budget (see below).
 *
 * ---------------------------------------------------------------------
 * HOW REVISITING ADDRESSES MAKES A LOOP
 * ---------------------------------------------------------------------
 * The PC is only a number. Each step fetches the 4 bytes at the address
 * the PC holds. Normally the PC grows by 4, so we walk forward through
 * RAM once. A taken BNE instead puts an EARLIER address (here 12) into
 * the PC. The CPU then fetches the same bytes at 12, 16, 20 ... again
 * and decodes them again. Nothing was "copied" or "repeated" in memory:
 * the code exists once, and the PC simply returns to it. The loop does
 * not do the same work each time because the REGISTERS changed (x1 and
 * x2 moved on by one byte, x4 counted down), so the same instruction
 * words act on different data. When x4 reaches 0 the BNE is not taken,
 * the PC falls through to 36, and the loop is over.
 *
 * ---------------------------------------------------------------------
 * ONE STEP (cpu_step)
 * ---------------------------------------------------------------------
 *   1. FETCH      4 bytes at PC, little-endian, PC must be 4-aligned
 *   2. VALIDATE   (opcode, funct3) must be ADDI, LBU, SB or BNE
 *   3. EXTRACT    register numbers and the right signed immediate
 *   4. COMPUTE    result / effective address / branch decision
 *   5. CHECK      data address < 256; taken branch target 4-aligned
 *   6. COMMIT     RAM write (SB), register write (ADDI/LBU), PC update
 *
 * Steps 1-5 only READ the machine. Nothing changes until step 6, so a
 * rejected instruction leaves PC, registers and RAM untouched.
 *
 * A branch target is NOT range-checked when the branch executes. It is
 * checked when the CPU tries to FETCH from it on the next step, which
 * is where the existing fetch checks reject an out-of-range PC.
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
 *  B-type (BNE):
 *   31    30   25 24   20 19   15 14  12 11    8   7     6      0
 *  +-----+-------+-------+-------+------+-------+-----+--------+
 *  |i[12]|i[10:5]|  rs2  |  rs1  |funct3|i[4:1] |i[11]| opcode |
 *  +-----+-------+-------+-------+------+-------+-----+--------+
 *   The offset is a multiple of 2, so imm[0] is not stored: it is 0.
 *   The pieces are scattered; imm_b() reassembles them.
 *
 *   ADDI: opcode 0x13, funct3 0
 *   LBU : opcode 0x03, funct3 4
 *   SB  : opcode 0x23, funct3 0
 *   BNE : opcode 0x63, funct3 1   (other funct3 values are other
 *                                  branches and are rejected)
 *
 * Hand-derived example: "bne x4, x0, -20" at address 32 (target 12).
 *   -20 as 13 bits = 0x1FEC = 1 1111 1110 1100
 *     imm[12]=1  imm[11]=1  imm[10:5]=111111  imm[4:1]=0110
 *   word = 1<<31 | 0x3F<<25 | rs2 0<<20 | rs1 4<<15 | 1<<12
 *        | 0x6<<8 | 1<<7 | 0x63                    = 0xFE0216E3
 *
 * HOST-SIDE LIMITS: the demonstration stops when the PC reaches 36 and
 * has a maximum-step budget. Neither is a guest HALT instruction; the
 * guest CPU still has no way to stop itself. If the budget runs out the
 * host reports FAILURE (a probable infinite loop), not completion.
 *
 * Build:  gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step3 cpu_step3.c
 * Run:    ./cpu_step3
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RAM_SIZE   256u
#define NUM_REGS   32u

#define OP_LOAD    0x03u
#define OP_IMM     0x13u
#define OP_STORE   0x23u
#define OP_BRANCH  0x63u
#define F3_ADDI    0x0u
#define F3_LBU     0x4u
#define F3_SB      0x0u
#define F3_BNE     0x1u

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
    STEP_DATA_OUT_OF_RANGE,
    STEP_BRANCH_MISALIGNED
} StepStatus;

typedef enum { KIND_ADDI, KIND_LBU, KIND_SB, KIND_BNE } InstrKind;

/* Everything one successful step learned; used for the trace. */
typedef struct {
    InstrKind kind;
    uint32_t  pc;
    uint32_t  word;
    uint32_t  rd;        /* ADDI/LBU only; 0 otherwise */
    uint32_t  rs1;
    uint32_t  rs2;       /* SB and BNE */
    int32_t   imm;       /* signed offset or immediate */
    uint32_t  src1;      /* value of rs1 */
    uint32_t  src2;      /* value of rs2 (SB, BNE) */
    uint32_t  ea;        /* effective data address (LBU/SB) */
    uint32_t  byte;      /* byte read (LBU) or written (SB) */
    bool      writes_reg;
    uint32_t  reg_value; /* value headed for rd */
    bool      taken;     /* BNE: was the branch taken? */
    uint32_t  target;    /* BNE: pc + offset, whether or not taken */
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

/* SIGN EXTENSION without signed shifts. A field of `bits` bits holds an
 * unsigned number; if its top bit (the sign bit) is set the value is
 * field - 2^bits, otherwise it is the field itself. Both results stay
 * well inside int32_t. */
static int32_t sign_extend_12(uint32_t field)
{
    field &= 0xFFFu;
    if ((field & 0x800u) != 0u) {
        return (int32_t)field - 4096;
    }
    return (int32_t)field;
}

/* Same idea for the 13-bit branch offset: sign bit is bit 12. */
static int32_t sign_extend_13(uint32_t field)
{
    field &= 0x1FFFu;
    if ((field & 0x1000u) != 0u) {
        return (int32_t)field - 8192;
    }
    return (int32_t)field;
}

/* I-type immediate: bits 31:20 in one piece. */
static int32_t imm_i(uint32_t word)
{
    return sign_extend_12(word >> 20);
}

/* S-type immediate: bits 31:25 are imm[11:5], bits 11:7 are imm[4:0]. */
static int32_t imm_s(uint32_t word)
{
    uint32_t high = (word >> 25) & 0x7Fu; /* imm[11:5] */
    uint32_t low  = (word >> 7)  & 0x1Fu; /* imm[4:0]  */
    return sign_extend_12((high << 5) | low);
}

/* B-type immediate (13 bits, bit 0 always 0):
 *   imm[12]   = bit 31
 *   imm[11]   = bit 7
 *   imm[10:5] = bits 30:25
 *   imm[4:1]  = bits 11:8
 * Move each piece to its place, OR them together, then sign-extend. */
static int32_t imm_b(uint32_t word)
{
    uint32_t b12    = (word >> 31) & 0x1u;
    uint32_t b11    = (word >> 7)  & 0x1u;
    uint32_t b10_5  = (word >> 25) & 0x3Fu;
    uint32_t b4_1   = (word >> 8)  & 0xFu;
    uint32_t field  = (b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1);
    return sign_extend_13(field);
}

static const char *status_text(StepStatus s)
{
    switch (s) {
    case STEP_OK:                 return "ok";
    case STEP_FETCH_MISALIGNED:   return "instruction fetch is not 4-byte aligned";
    case STEP_FETCH_OUT_OF_RANGE: return "instruction fetch is outside guest RAM";
    case STEP_UNSUPPORTED:        return "unsupported instruction (only ADDI, LBU, SB, BNE exist)";
    case STEP_DATA_OUT_OF_RANGE:  return "data address is outside guest RAM";
    case STEP_BRANCH_MISALIGNED:  return "taken branch target is not 4-byte aligned";
    }
    return "unknown error";
}

/* ------------------------------------------------------------------ */
/* Machine setup and the demonstration program                         */
/* ------------------------------------------------------------------ */

#define DEMO_LOOP_ADDR   12u
#define DEMO_DONE_PC     36u   /* host completion point, not a HALT */
#define DEMO_STEP_BUDGET 100u

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
    ram_store_word(m, 0u,  0x06400093u); /*  0:       addi x1, x0, 100 */
    ram_store_word(m, 4u,  0x0C800113u); /*  4:       addi x2, x0, 200 */
    ram_store_word(m, 8u,  0x00300213u); /*  8:       addi x4, x0, 3   */
    ram_store_word(m, 12u, 0x0000C183u); /* 12: loop: lbu  x3, 0(x1)   */
    ram_store_word(m, 16u, 0x00310023u); /* 16:       sb   x3, 0(x2)   */
    ram_store_word(m, 20u, 0x00108093u); /* 20:       addi x1, x1, 1   */
    ram_store_word(m, 24u, 0x00110113u); /* 24:       addi x2, x2, 1   */
    ram_store_word(m, 28u, 0xFFF20213u); /* 28:       addi x4, x4, -1  */
    ram_store_word(m, 32u, 0xFE0216E3u); /* 32:       bne  x4, x0, loop (-20) */
    /* 36: nothing is stored here. The host stops when PC == 36. */

    m->ram[100] = 5u;  /* source bytes */
    m->ram[101] = 7u;
    m->ram[102] = 9u;
    m->ram[200] = 0u;  /* destination bytes */
    m->ram[201] = 0u;
    m->ram[202] = 0u;
    m->ram[199] = 0xA5u; /* sentinels */
    m->ram[203] = 0xA5u;
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
     *    0x03 but XORI under 0x13, and BLT under 0x63). For branches,
     *    only funct3 1 (BNE) is accepted; BEQ, BLT, ... are rejected. */
    uint32_t opcode = word & 0x7Fu;
    uint32_t funct3 = (word >> 12) & 0x7u;
    if (opcode == OP_IMM && funct3 == F3_ADDI) {
        t.kind = KIND_ADDI;
    } else if (opcode == OP_LOAD && funct3 == F3_LBU) {
        t.kind = KIND_LBU;
    } else if (opcode == OP_STORE && funct3 == F3_SB) {
        t.kind = KIND_SB;
    } else if (opcode == OP_BRANCH && funct3 == F3_BNE) {
        t.kind = KIND_BNE;
    } else {
        return STEP_UNSUPPORTED;
    }

    /* 3. Extract fields. SB and BNE have no rd: their bits 11:7 belong
     *    to the immediate, so rd is left at 0 and never written. */
    t.pc   = m->pc;
    t.word = word;
    t.rs1  = (word >> 15) & 0x1Fu;
    switch (t.kind) {
    case KIND_SB:
        t.rs2 = (word >> 20) & 0x1Fu;
        t.imm = imm_s(word);
        break;
    case KIND_BNE:
        t.rs2 = (word >> 20) & 0x1Fu;
        t.imm = imm_b(word);
        break;
    case KIND_ADDI:
    case KIND_LBU:
        t.rd  = (word >> 7) & 0x1Fu;
        t.imm = imm_i(word);
        break;
    }
    t.src1    = reg_read(m, t.rs1);
    t.next_pc = m->pc + 4u;

    /* 4+5. Compute and check. Converting a negative int32_t to
     * uint32_t wraps modulo 2^32 by definition, and unsigned addition
     * wraps too, so address arithmetic has no signed overflow. */
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
    case KIND_BNE:
        /* Compare the FULL 32-bit values of both registers. */
        t.src2   = reg_read(m, t.rs2);
        t.target = m->pc + (uint32_t)t.imm;  /* modulo 2^32 */
        t.taken  = (t.src1 != t.src2);
        if (t.taken) {
            /* Only a branch that is actually taken needs a valid
             * target. An untaken branch never uses its target. */
            if ((t.target & 3u) != 0u) {
                return STEP_BRANCH_MISALIGNED; /* nothing changed yet */
            }
            t.next_pc = t.target;
        }
        break;
    }

    /* 6. Commit. Every check has passed, so side effects are safe now.
     *    BNE writes no register and no data memory: it only sets PC. */
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
/* Host-side run loop                                                  */
/* ------------------------------------------------------------------ */

typedef enum {
    RUN_DONE,             /* PC reached the stop address */
    RUN_BUDGET_EXHAUSTED, /* ran out of steps: failure, not completion */
    RUN_CPU_ERROR         /* a step was rejected */
} RunResult;

#define MAX_OUTCOMES 16u

typedef struct {
    unsigned steps;                  /* completed instructions */
    unsigned stores;                 /* SB instructions completed */
    unsigned branches;               /* BNE instructions completed */
    bool     taken[MAX_OUTCOMES];    /* first MAX_OUTCOMES BNE outcomes */
} RunStats;

static void print_trace(unsigned n, const StepInfo *t);

/* Run until PC == stop_pc. The stop test comes BEFORE each fetch, so
 * the instruction at stop_pc is never executed. The budget is only
 * consulted when another step is actually needed, so a program that
 * finishes in exactly `budget` steps still counts as done. */
static RunResult run_until_pc(Machine *m, uint32_t stop_pc, unsigned budget,
                              bool trace, RunStats *s, StepStatus *err)
{
    s->steps = 0u;
    s->stores = 0u;
    s->branches = 0u;
    for (unsigned i = 0; i < MAX_OUTCOMES; i++) {
        s->taken[i] = false;
    }
    *err = STEP_OK;

    while (m->pc != stop_pc) {
        if (s->steps >= budget) {
            return RUN_BUDGET_EXHAUSTED;
        }
        StepInfo t;
        StepStatus st = cpu_step(m, &t);
        if (st != STEP_OK) {
            *err = st;
            return RUN_CPU_ERROR;
        }
        s->steps++;
        if (t.kind == KIND_SB) {
            s->stores++;
        }
        if (t.kind == KIND_BNE) {
            if (s->branches < MAX_OUTCOMES) {
                s->taken[s->branches] = t.taken;
            }
            s->branches++;
        }
        if (trace) {
            print_trace(s->steps, &t);
        }
    }
    return RUN_DONE;
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
    printf("step %2u: pc=0x%08" PRIX32 " word=0x%08" PRIX32 " ",
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
    case KIND_BNE:
        printf("BNE  x%" PRIu32 ", x%" PRIu32 " | x%" PRIu32 "=%" PRIu32
               " x%" PRIu32 "=%" PRIu32 " | offset=%" PRId32
               " target=0x%08" PRIX32 " | %s | ",
               t->rs1, t->rs2, t->rs1, t->src1, t->rs2, t->src2,
               t->imm, t->target, t->taken ? "TAKEN" : "not taken");
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
        printf("  PASS %-26s = %" PRIu32 "\n", name, got);
    } else {
        printf("  FAIL %-26s = %" PRIu32 " (wanted %" PRIu32 ")\n",
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

/* Same, but ignoring the PC: "no register or RAM changes". */
static bool regs_and_ram_equal(const Machine *a, const Machine *b)
{
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
 * literal words, and Test 0 proves the encoders agree with them. */
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

static uint32_t enc_b(uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    uint32_t u = (uint32_t)imm & 0x1FFFu;
    return (((u >> 12) & 0x1u) << 31) | (((u >> 5) & 0x3Fu) << 25)
         | (rs2 << 20) | (rs1 << 15) | (funct3 << 12)
         | (((u >> 1) & 0xFu) << 8) | (((u >> 11) & 0x1u) << 7) | OP_BRANCH;
}

/* Fresh machine with one instruction at address 0 (PC = 0). */
static void setup(Machine *m, uint32_t word)
{
    machine_init(m);
    ram_store_word(m, 0u, word);
}

/* Fresh machine with one instruction at `addr`, and PC pointing at it. */
static void setup_at(Machine *m, uint32_t addr, uint32_t word)
{
    machine_init(m);
    ram_store_word(m, addr, word);
    m->pc = addr;
}

/* Run one step that must be rejected; verify the status AND that the
 * whole machine (PC, registers, RAM) is exactly as it was. */
static void expect_reject(const char *label, Machine *m, StepStatus want)
{
    Machine before = *m;
    StepStatus got = cpu_step(m, NULL);
    bool same = machine_equal(&before, m);
    printf("  %-34s -> %s%s\n", label, status_text(got),
           same ? " [state unchanged]" : " [STATE CHANGED]");
    if (got != want || !same) {
        printf("  FAIL: wanted a different status or state changed\n");
        g_failures++;
    }
}

/* ------------------------------------------------------------------ */
/* Tests carried over from milestones 1 and 2                          */
/* ------------------------------------------------------------------ */

static void test_encoders_match_demo_words(void)
{
    printf("\nTest 0: test encoders reproduce the demo program's words\n");
    check_eq("addi x1,x0,100", enc_i(OP_IMM, F3_ADDI, 1, 0, 100), 0x06400093u);
    check_eq("addi x2,x0,200", enc_i(OP_IMM, F3_ADDI, 2, 0, 200), 0x0C800113u);
    check_eq("addi x4,x0,3",   enc_i(OP_IMM, F3_ADDI, 4, 0, 3),   0x00300213u);
    check_eq("lbu x3,0(x1)",   enc_i(OP_LOAD, F3_LBU, 3, 1, 0),   0x0000C183u);
    check_eq("sb x3,0(x2)",    enc_s(F3_SB, 2, 3, 0),             0x00310023u);
    check_eq("addi x1,x1,1",   enc_i(OP_IMM, F3_ADDI, 1, 1, 1),   0x00108093u);
    check_eq("addi x2,x2,1",   enc_i(OP_IMM, F3_ADDI, 2, 2, 1),   0x00110113u);
    check_eq("addi x4,x4,-1",  enc_i(OP_IMM, F3_ADDI, 4, 4, -1),  0xFFF20213u);
    check_eq("bne x4,x0,-20",  enc_b(F3_BNE, 4, 0, -20),          0xFE0216E3u);
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

/* Steps 1 and 2 rejections, kept. (Branch rejections have their own test.) */
static void test_rejections(void)
{
    printf("\nTest (steps 1-2): rejecting bad instructions and fetches\n");
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

static void test_lbu_zero_extends(void)
{
    printf("\nTest (step 2) 1: LBU of 0xF0 produces 240 (not a negative number)\n");
    Machine m;
    setup(&m, enc_i(OP_LOAD, F3_LBU, 3, 1, 0)); /* lbu x3, 0(x1) */
    m.x[1] = 50u;
    m.ram[50] = 0xF0u;
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("x3", m.x[3], 240u);
    check_eq("RAM[50] unchanged", m.ram[50], 0xF0u);
}

static void test_sb_stores_low_byte_only(void)
{
    printf("\nTest (step 2) 2: SB of 0x123456AB stores only 0xAB\n");
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

static void test_negative_offsets(void)
{
    printf("\nTest (step 2) 3: offset -4 works for LBU and SB\n");
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

static void test_sb_immediate_bits_are_not_rd(void)
{
    printf("\nTest (step 2) 4: SB with nonzero bits 11:7 changes no register\n");
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

    check_eq("word bits 11:7", (word >> 7) & 0x1Fu, 5u);
    StepInfo t;
    check_eq("step status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("decoded imm", (uint32_t)t.imm, 37u);
    check_eq("ea (100+37)", t.ea, 137u);
    check_eq("RAM[137]", m.ram[137], 0x44u);
    check_eq("RAM[105] untouched", m.ram[105], 0u);

    uint32_t changed = 0u;
    for (uint32_t i = 0; i < NUM_REGS; i++) {
        if (m.x[i] != regs_before[i]) {
            changed++;
        }
    }
    check_eq("registers changed", changed, 0u);
}

static void test_address_bounds(void)
{
    printf("\nTest (step 2) 5: byte address 255 works; 256 and 0xFFFFFFFF are rejected\n");
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

static void test_load_into_x0(void)
{
    printf("\nTest (step 2) 6: loads targeting x0\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_i(OP_LOAD, F3_LBU, 0, 1, 0)); /* lbu x0, 0(x1), bad address */
    m.x[1] = 256u;
    expect_reject("invalid load to x0", &m, STEP_DATA_OUT_OF_RANGE);

    setup(&m, enc_i(OP_LOAD, F3_LBU, 0, 1, 0)); /* lbu x0, 0(x1), good address */
    m.x[1] = 100u;
    m.ram[100] = 5u;
    check_eq("valid load status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("byte was read", t.byte, 5u);
    check_eq("x0 still zero", m.x[0], 0u);
    check_eq("pc", m.pc, 4u);
}

/* ------------------------------------------------------------------ */
/* New tests for BNE                                                   */
/* ------------------------------------------------------------------ */

/* The B-immediate decoder against hand-derived words (not produced by
 * our own encoder), then a round trip across the offset range. */
static void test_bne_immediate_decoding(void)
{
    printf("\nTest B0: B-type immediate decoding\n");
    check_eq("imm of 0xFE0216E3", (uint32_t)imm_b(0xFE0216E3u), (uint32_t)-20);
    check_eq("imm of 0x000010E3 (+2048)", (uint32_t)imm_b(0x000010E3u), 2048u);
    check_eq("imm of 0x7E001FE3 (+4094)", (uint32_t)imm_b(0x7E001FE3u), 4094u);
    check_eq("imm of 0x80001063 (-4096)", (uint32_t)imm_b(0x80001063u), (uint32_t)-4096);

    static const int32_t offsets[] = { -4096, -2050, -20, -4, -2, 0, 2, 8,
                                       12, 2046, 2048, 4094 };
    uint32_t bad = 0u;
    for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
        if (imm_b(enc_b(F3_BNE, 1, 2, offsets[i])) != offsets[i]) {
            bad++;
        }
    }
    check_eq("round-trip mismatches", bad, 0u);
    check_eq("bit 0 of any offset", (uint32_t)imm_b(0xFFFFFFFFu) & 1u, 0u);
}

static void test_bne_taken_and_not_taken(void)
{
    printf("\nTest B1: BNE taken and not taken\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_b(F3_BNE, 1, 2, 8)); /* bne x1, x2, +8 */
    m.x[1] = 1u;
    m.x[2] = 2u;
    check_eq("taken: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("taken: flag", (uint32_t)t.taken, 1u);
    check_eq("taken: pc", m.pc, 8u);

    setup(&m, enc_b(F3_BNE, 1, 2, 8));
    m.x[1] = 7u;
    m.x[2] = 7u;
    check_eq("not taken: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("not taken: flag", (uint32_t)t.taken, 0u);
    check_eq("not taken: pc", m.pc, 4u);

    /* Full 32-bit comparison: values differ only in the top bit. */
    setup(&m, enc_b(F3_BNE, 1, 2, 8));
    m.x[1] = 0x00000001u;
    m.x[2] = 0x80000001u;
    check_eq("high-bit-only diff: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("high-bit-only diff: taken", (uint32_t)t.taken, 1u);
    check_eq("high-bit-only diff: pc", m.pc, 8u);
}

static void test_bne_forward_and_backward(void)
{
    printf("\nTest B2: forward and backward targets\n");
    Machine m;
    StepInfo t;

    setup_at(&m, 16u, enc_b(F3_BNE, 1, 2, 8)); /* forward: 16 + 8 */
    m.x[1] = 3u;
    m.x[2] = 4u;
    check_eq("forward status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("forward offset", (uint32_t)t.imm, 8u);
    check_eq("forward pc", m.pc, 24u);

    setup_at(&m, 16u, enc_b(F3_BNE, 1, 2, -12)); /* backward: 16 - 12 */
    m.x[1] = 3u;
    m.x[2] = 4u;
    check_eq("backward status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("backward offset", (uint32_t)t.imm, (uint32_t)-12);
    check_eq("backward pc", m.pc, 4u);
}

static void test_bne_nonzero_rs2(void)
{
    printf("\nTest B3: a nonzero-numbered rs2 register (x6)\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_b(F3_BNE, 5, 6, 8)); /* bne x5, x6, +8 */
    m.x[5] = 9u;
    m.x[6] = 9u;
    check_eq("equal: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("equal: rs2 decoded", t.rs2, 6u);
    check_eq("equal: pc (not taken)", m.pc, 4u);

    setup(&m, enc_b(F3_BNE, 5, 6, 8));
    m.x[5] = 9u;
    m.x[6] = 10u;
    check_eq("differ: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("differ: src2 value", t.src2, 10u);
    check_eq("differ: pc (taken)", m.pc, 8u);

    setup(&m, enc_b(F3_BNE, 0, 6, 8)); /* bne x0, x6, +8: x0 reads as 0 */
    m.x[6] = 1u;
    check_eq("x0 vs x6: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("x0 vs x6: src1", t.src1, 0u);
    check_eq("x0 vs x6: pc (taken)", m.pc, 8u);
}

static void test_bne_changes_no_registers_or_ram(void)
{
    printf("\nTest B4: BNE changes no register and no RAM (only PC)\n");
    Machine m;
    StepInfo t;

    for (int taken = 1; taken >= 0; taken--) {
        setup(&m, enc_b(F3_BNE, 5, 6, 8));
        for (uint32_t i = 1; i < NUM_REGS; i++) {
            m.x[i] = 0x100u + i;                    /* all distinct */
        }
        m.x[6] = taken ? m.x[5] + 1u : m.x[5];       /* force the outcome */
        for (uint32_t i = 8; i < RAM_SIZE; i++) {
            m.ram[i] = (uint8_t)(i * 7u + 3u);
        }
        Machine before = m;
        check_eq(taken ? "taken: status" : "untaken: status",
                 (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
        check_eq(taken ? "taken: wrote reg" : "untaken: wrote reg",
                 (uint32_t)t.writes_reg, 0u);
        check_eq(taken ? "taken: regs+RAM same" : "untaken: regs+RAM same",
                 (uint32_t)regs_and_ram_equal(&before, &m), 1u);
        check_eq(taken ? "taken: pc moved" : "untaken: pc moved",
                 m.pc, taken ? 8u : 4u);
    }
}

static void test_bne_misaligned_targets(void)
{
    printf("\nTest B5: misaligned targets (taken rejected, untaken accepted)\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_b(F3_BNE, 1, 2, 6)); /* target 6: not a multiple of 4 */
    m.x[1] = 1u;
    m.x[2] = 2u;
    expect_reject("taken, offset +6", &m, STEP_BRANCH_MISALIGNED);

    setup_at(&m, 8u, enc_b(F3_BNE, 1, 2, -2)); /* target 6 */
    m.x[1] = 1u;
    m.x[2] = 2u;
    expect_reject("taken, offset -2 at pc 8", &m, STEP_BRANCH_MISALIGNED);

    setup(&m, enc_b(F3_BNE, 1, 2, -2)); /* target wraps to 0xFFFFFFFE */
    m.x[1] = 1u;
    m.x[2] = 2u;
    expect_reject("taken, wraps to 0xFFFFFFFE", &m, STEP_BRANCH_MISALIGNED);

    setup(&m, enc_b(F3_BNE, 1, 2, 6)); /* same word, but not taken */
    m.x[1] = 5u;
    m.x[2] = 5u;
    check_eq("untaken misaligned: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("untaken misaligned: taken", (uint32_t)t.taken, 0u);
    check_eq("untaken misaligned: pc", m.pc, 4u);

    setup(&m, enc_b(F3_BNE, 1, 2, 2)); /* offset +2, not taken */
    check_eq("untaken +2: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("untaken +2: pc", m.pc, 4u);
}

static void test_bne_target_bounds_checked_at_fetch(void)
{
    printf("\nTest B6: out-of-RAM targets are rejected when fetched\n");
    Machine m;
    StepInfo t;

    setup(&m, enc_b(F3_BNE, 1, 2, 256)); /* aligned target = 256 */
    m.x[1] = 1u;
    m.x[2] = 2u;
    check_eq("branch to 256: status", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("pc is now", m.pc, 256u);
    expect_reject("fetch at pc=256", &m, STEP_FETCH_OUT_OF_RANGE);

    setup(&m, enc_b(F3_BNE, 1, 2, -4)); /* target wraps to 0xFFFFFFFC */
    m.x[1] = 1u;
    m.x[2] = 2u;
    check_eq("branch to 0xFFFFFFFC", (uint32_t)cpu_step(&m, &t), (uint32_t)STEP_OK);
    check_eq("pc is now", m.pc, 0xFFFFFFFCu);
    expect_reject("fetch at pc=0xFFFFFFFC", &m, STEP_FETCH_OUT_OF_RANGE);
}

static void test_unsupported_branches(void)
{
    printf("\nTest B7: unsupported branch types are rejected\n");
    static const uint32_t funct3s[] = { 0u, 2u, 3u, 4u, 5u, 6u, 7u };
    static const char *const names[] = {
        "funct3=0 (beq)", "funct3=2 (invalid)", "funct3=3 (invalid)",
        "funct3=4 (blt)", "funct3=5 (bge)", "funct3=6 (bltu)",
        "funct3=7 (bgeu)"
    };
    for (unsigned i = 0; i < sizeof funct3s / sizeof funct3s[0]; i++) {
        Machine m;
        setup(&m, enc_b(funct3s[i], 1, 2, 8));
        m.x[1] = 1u;
        m.x[2] = 2u;
        expect_reject(names[i], &m, STEP_UNSUPPORTED);
    }
}

/* Run-loop tests: budget handling reports failure, not completion. */
static void test_step_budget(void)
{
    printf("\nTest B8: step budget catches infinite loops\n");
    Machine m;
    RunStats s;
    StepStatus err;

    /* 0: addi x1,x0,1   4: bne x1,x0,0  (branches to itself forever) */
    machine_init(&m);
    ram_store_word(&m, 0u, enc_i(OP_IMM, F3_ADDI, 1, 0, 1));
    ram_store_word(&m, 4u, enc_b(F3_BNE, 1, 0, 0));
    RunResult r = run_until_pc(&m, DEMO_DONE_PC, 50u, false, &s, &err);
    check_eq("infinite loop: result is BUDGET", (uint32_t)r, (uint32_t)RUN_BUDGET_EXHAUSTED);
    check_eq("infinite loop: steps used", s.steps, 50u);

    machine_init(&m);
    load_demo_program(&m);
    r = run_until_pc(&m, DEMO_DONE_PC, 20u, false, &s, &err);
    check_eq("demo, budget 20: BUDGET", (uint32_t)r, (uint32_t)RUN_BUDGET_EXHAUSTED);

    machine_init(&m);
    load_demo_program(&m);
    r = run_until_pc(&m, DEMO_DONE_PC, 21u, false, &s, &err);
    check_eq("demo, budget 21: DONE", (uint32_t)r, (uint32_t)RUN_DONE);
    check_eq("demo, budget 21: steps", s.steps, 21u);
}

/* ------------------------------------------------------------------ */
/* Main demonstration                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    Machine m;
    machine_init(&m);
    load_demo_program(&m);

    printf("Running the demo program (stop at PC=%u, budget %u steps)\n",
           (unsigned)DEMO_DONE_PC, (unsigned)DEMO_STEP_BUDGET);

    RunStats s;
    StepStatus err;
    RunResult r = run_until_pc(&m, DEMO_DONE_PC, DEMO_STEP_BUDGET, true, &s, &err);

    if (r == RUN_BUDGET_EXHAUSTED) {
        printf("\nFAILURE: step budget of %u exhausted before PC reached %u "
               "(probable infinite loop)\n",
               (unsigned)DEMO_STEP_BUDGET, (unsigned)DEMO_DONE_PC);
        return 1;
    }
    if (r == RUN_CPU_ERROR) {
        printf("\nFAILURE: step %u rejected at pc=0x%08" PRIX32 ": %s\n",
               s.steps + 1u, m.pc, status_text(err));
        return 1;
    }

    printf("\nPC reached %u after %u completed instructions. This is the host "
           "demonstration completion point, not a guest HALT.\n",
           (unsigned)DEMO_DONE_PC, s.steps);

    printf("\nFinal checks\n");
    check_eq("instructions completed", s.steps, 21u);
    check_eq("x1", m.x[1], 103u);
    check_eq("x2", m.x[2], 203u);
    check_eq("x4", m.x[4], 0u);
    check_eq("x3", m.x[3], 9u);
    check_eq("x0", m.x[0], 0u);
    check_eq("PC", m.pc, 36u);
    check_eq("RAM[200]", m.ram[200], 5u);
    check_eq("RAM[201]", m.ram[201], 7u);
    check_eq("RAM[202]", m.ram[202], 9u);
    check_eq("RAM[100] source", m.ram[100], 5u);
    check_eq("RAM[101] source", m.ram[101], 7u);
    check_eq("RAM[102] source", m.ram[102], 9u);
    check_eq("RAM[199] sentinel", m.ram[199], 0xA5u);
    check_eq("RAM[203] sentinel", m.ram[203], 0xA5u);
    check_eq("stores performed", s.stores, 3u);
    check_eq("branches executed", s.branches, 3u);
    check_eq("branch 1 taken", (uint32_t)s.taken[0], 1u);
    check_eq("branch 2 taken", (uint32_t)s.taken[1], 1u);
    check_eq("branch 3 taken", (uint32_t)s.taken[2], 0u);

    test_encoders_match_demo_words();
    test_addi_cannot_change_x0();
    test_rejections();
    test_lbu_zero_extends();
    test_sb_stores_low_byte_only();
    test_negative_offsets();
    test_sb_immediate_bits_are_not_rd();
    test_address_bounds();
    test_load_into_x0();

    test_bne_immediate_decoding();
    test_bne_taken_and_not_taken();
    test_bne_forward_and_backward();
    test_bne_nonzero_rs2();
    test_bne_changes_no_registers_or_ram();
    test_bne_misaligned_targets();
    test_bne_target_bounds_checked_at_fetch();
    test_unsupported_branches();
    test_step_budget();

    if (g_failures != 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
