#ifndef RV32I_H
#define RV32I_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host-owned RAM; guest addresses are array offsets, never host pointers.
 * Initialize with rv_init(), then edit state only between steps. x[0] stays 0.
 */
typedef struct {
    uint32_t x[32];
    uint32_t pc;
    uint8_t *ram;
    size_t ram_size;
} RvCpu;

typedef enum {
    RV_OK, RV_INSTRUCTION_MISALIGNED, RV_INSTRUCTION_ACCESS,
    RV_ILLEGAL_INSTRUCTION, RV_LOAD_MISALIGNED, RV_LOAD_ACCESS,
    RV_STORE_MISALIGNED, RV_STORE_ACCESS, RV_ECALL, RV_EBREAK
} RvStatus;

/* Diagnostic output, outside guest state. On a target alignment fault, pc is
 * the branch/jump address and address is its target. On an access fault after
 * a successful jump, pc is the new PC. word is 0 if fetching was impossible.
 */
typedef struct {
    RvStatus status;
    uint32_t pc, word, address;
} RvTrap;

/* RAM may be 4 .. 2^32 bytes (subject to host size_t/allocation limits).
 * Initialization zeros registers and PC, but leaves supplied RAM untouched.
 */
bool rv_init(RvCpu *cpu, uint8_t *ram, size_t size);
RvStatus rv_step(RvCpu *cpu, RvTrap *trap);
const char *rv_status_name(RvStatus status);

/* Host loading/inspection helpers. Failure writes nothing. Overlapping image
 * and RAM are permitted. Helpers do not enforce guest instruction alignment.
 */
bool rv_load_image(RvCpu *cpu, uint32_t address, const void *image, size_t size);
bool rv_peek_u32(const RvCpu *cpu, uint32_t address, uint32_t *value);

#endif
