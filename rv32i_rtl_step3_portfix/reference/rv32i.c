/* RV32I instruction-level interpreter, grown from cpu_step3/4.
 * Specification: RV32I v2.1, official ISA manual release v20260120.
 * https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html
 *
 * Structure is the same as the early milestones:
 * fetch -> decode -> compute/check -> commit. No failure path reaches commit.
 * This is functional execution, not RTL or a cycle/performance model.
 */
#include "rv32i.h"
#include <string.h>

static bool in_range(const RvCpu *c, uint32_t a, size_t n)
{
    /* Subtract before comparing: a+n could wrap or cross the end of RAM. */
    return n <= c->ram_size && (uint64_t)a <= (uint64_t)(c->ram_size - n);
}

bool rv_init(RvCpu *c, uint8_t *ram, size_t size)
{
    if (c == NULL || ram == NULL || size < 4u ||
        (uint64_t)size > UINT64_C(0x100000000)) return false;
    *c = (RvCpu){ .ram = ram, .ram_size = size };
    return true;
}

bool rv_load_image(RvCpu *c, uint32_t a, const void *image, size_t size)
{
    if (image == NULL || !in_range(c,a,size)) return false;
    memmove(c->ram + a,image,size);
    return true;
}

static uint32_t read_le(const RvCpu *c, uint32_t a, unsigned width)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i)
        result |= (uint32_t)c->ram[(size_t)a + i] << (8u * i);
    return result;
}

bool rv_peek_u32(const RvCpu *c, uint32_t a, uint32_t *value)
{
    if (value == NULL || !in_range(c,a,4)) return false;
    *value = read_le(c,a,4);
    return true;
}

static uint32_t reg(const RvCpu *c, uint32_t r)
{ return r == 0u ? 0u : c->x[r]; }

/* Callers pass an already masked field. Result is the 32-bit bit pattern,
 * avoiding implementation-defined unsigned-to-signed conversions. */
static uint32_t sext(uint32_t value, unsigned bits)
{
    const uint32_t sign = UINT32_C(1) << (bits - 1u);
    return (value ^ sign) - sign;
}

static bool signed_less(uint32_t a, uint32_t b)
{ return (a ^ UINT32_C(0x80000000)) < (b ^ UINT32_C(0x80000000)); }

static uint32_t arithmetic_right(uint32_t value, unsigned amount)
{
    if (amount == 0u) return value; /* also avoids a shift by 32 below */
    const uint32_t fill = (value & UINT32_C(0x80000000)) != 0u
                        ? UINT32_MAX << (32u - amount) : 0u;
    return (value >> amount) | fill;
}

static RvStatus report(RvTrap *t, RvStatus s, uint32_t pc,
                       uint32_t word, uint32_t address)
{
    if (t != NULL) *t = (RvTrap){s,pc,word,address};
    return s;
}

RvStatus rv_step(RvCpu *c, RvTrap *trap)
{
    const uint32_t pc = c->pc;
    if ((pc & 3u) != 0u)
        return report(trap,RV_INSTRUCTION_MISALIGNED,pc,0,pc);
    if (!in_range(c,pc,4))
        return report(trap,RV_INSTRUCTION_ACCESS,pc,0,pc);

    const uint32_t w = read_le(c,pc,4);
    const uint32_t op = w & 127u, f3 = (w >> 12) & 7u, f7 = w >> 25;
    const uint32_t rd = (w >> 7) & 31u;
    /* Read both sources before writing anything: JALR rd==rs1 is safe. */
    const uint32_t a = reg(c,(w >> 15) & 31u);
    const uint32_t b = reg(c,(w >> 20) & 31u);
    const uint32_t ii = sext(w >> 20,12);
    const uint32_t si = sext(((w >> 25) << 5) | ((w >> 7) & 31u),12);
    uint32_t next = pc + 4u, value = 0u, address = 0u;
    unsigned width = 0u;
    bool write_reg = false, store = false, sign_load = false, jump = false;

    switch (op) {
    case 0x13u: /* OP-IMM */
        write_reg = true;
        switch (f3) {
        case 0: value = a + ii; break;                         /* ADDI */
        case 2: value = signed_less(a,ii) ? 1u : 0u; break;   /* SLTI */
        case 3: value = a < ii ? 1u : 0u; break;              /* SLTIU */
        case 4: value = a ^ ii; break;                        /* XORI */
        case 6: value = a | ii; break;                        /* ORI */
        case 7: value = a & ii; break;                        /* ANDI */
        case 1:
            if (f7 != 0u) goto illegal;
            value = a << ((w >> 20) & 31u); break;            /* SLLI */
        case 5:
            if (f7 == 0u) value = a >> ((w >> 20) & 31u);      /* SRLI */
            else if (f7 == 0x20u)
                value = arithmetic_right(a,(w >> 20) & 31u); /* SRAI */
            else goto illegal;
            break;
        }
        break;
    case 0x33u: /* OP: reject M/other extensions, even with rd==x0. */
        if (f7 != 0u && !(f7 == 0x20u && (f3 == 0u || f3 == 5u))) goto illegal;
        write_reg = true;
        switch (f3) {
        case 0: value = f7 == 0u ? a + b : a - b; break;       /* ADD/SUB */
        case 1: value = a << (b & 31u); break;                /* SLL */
        case 2: value = signed_less(a,b) ? 1u : 0u; break;    /* SLT */
        case 3: value = a < b ? 1u : 0u; break;               /* SLTU */
        case 4: value = a ^ b; break;                         /* XOR */
        case 5: value = f7 == 0u ? a >> (b & 31u)
                                : arithmetic_right(a,b & 31u); break;
        case 6: value = a | b; break;                         /* OR */
        case 7: value = a & b; break;                         /* AND */
        }
        break;
    case 0x37u: value = w & 0xfffff000u; write_reg = true; break; /* LUI */
    case 0x17u: value = pc + (w & 0xfffff000u); write_reg = true; break; /* AUIPC */
    case 0x63u: { /* BRANCH */
        bool taken;
        switch (f3) {
        case 0: taken = a == b; break;
        case 1: taken = a != b; break;
        case 4: taken = signed_less(a,b); break;
        case 5: taken = !signed_less(a,b); break;
        case 6: taken = a < b; break;
        case 7: taken = a >= b; break;
        default: goto illegal;
        }
        if (taken) {
            const uint32_t bi = ((w >> 31) << 12) | (((w >> 7) & 1u) << 11)
                              | (((w >> 25) & 63u) << 5) | (((w >> 8) & 15u) << 1);
            next = pc + sext(bi,13); jump = true;
        }
        break;
    }
    case 0x6fu: { /* JAL */
        const uint32_t ji = ((w >> 31) << 20) | (w & 0xff000u)
                          | (((w >> 20) & 1u) << 11) | (((w >> 21) & 1023u) << 1);
        next = pc + sext(ji,21); value = pc + 4u; write_reg = true; jump = true;
        break;
    }
    case 0x67u: /* JALR */
        if (f3 != 0u) goto illegal;
        next = (a + ii) & ~UINT32_C(1); value = pc + 4u;
        write_reg = true; jump = true; break;
    case 0x03u: /* LOAD: signed byte/halfword or unsigned byte/halfword/word */
        switch (f3) {
        case 0: width = 1; sign_load = true; break;
        case 1: width = 2; sign_load = true; break;
        case 2: width = 4; break;
        case 4: width = 1; break;
        case 5: width = 2; break;
        default: goto illegal;
        }
        address = a + ii; write_reg = true; break;
    case 0x23u: /* STORE */
        switch (f3) {
        case 0: width = 1; break;
        case 1: width = 2; break;
        case 2: width = 4; break;
        default: goto illegal;
        }
        address = a + si; store = true; break;
    case 0x0fu: /* FENCE: single hart, no devices, accesses already in order. */
        if (f3 != 0u) goto illegal; /* FENCE.I belongs to Zifencei. */
        /* The base ISA requires ignoring rd/rs1 and treating reserved fm/
         * pred/succ configurations conservatively as FENCE, too. No state
         * change other than PC is needed in this execution environment. */
        break;
    case 0x73u:
        if (w == 0x00000073u) return report(trap,RV_ECALL,pc,w,0);
        if (w == 0x00100073u) return report(trap,RV_EBREAK,pc,w,0);
        goto illegal; /* no CSRs, WFI, MRET or invented syscall success */
    default:
        goto illegal;
    }

    if (jump && (next & 3u) != 0u)
        return report(trap,RV_INSTRUCTION_MISALIGNED,pc,w,next);
    /* Target ACCESS checks occur on the next fetch, not on this jump. */
    if (width != 0u) {
        if ((address & (width - 1u)) != 0u)
            return report(trap,store ? RV_STORE_MISALIGNED : RV_LOAD_MISALIGNED,
                          pc,w,address);
        if (!in_range(c,address,width))
            return report(trap,store ? RV_STORE_ACCESS : RV_LOAD_ACCESS,pc,w,address);
        if (!store) {
            value = read_le(c,address,width);
            if (sign_load) value = sext(value,width * 8u);
        }
    }

    /* COMMIT. Every possible fault/trap is above this point. */
    if (store) {
        for (unsigned i = 0; i < width; ++i)
            c->ram[(size_t)address + i] = (uint8_t)(b >> (8u * i));
    }
    if (write_reg && rd != 0u) c->x[rd] = value;
    c->pc = next;
    return report(trap,RV_OK,pc,w,0);

illegal:
    return report(trap,RV_ILLEGAL_INSTRUCTION,pc,w,0);
}

const char *rv_status_name(RvStatus s)
{
    switch (s) {
    case RV_OK: return "ok";
    case RV_INSTRUCTION_MISALIGNED: return "instruction address misaligned";
    case RV_INSTRUCTION_ACCESS: return "instruction access fault";
    case RV_ILLEGAL_INSTRUCTION: return "illegal/unsupported instruction";
    case RV_LOAD_MISALIGNED: return "load address misaligned";
    case RV_LOAD_ACCESS: return "load access fault";
    case RV_STORE_MISALIGNED: return "store address misaligned";
    case RV_STORE_ACCESS: return "store access fault";
    case RV_ECALL: return "ECALL trap";
    case RV_EBREAK: return "EBREAK trap";
    }
    return "unknown status";
}
