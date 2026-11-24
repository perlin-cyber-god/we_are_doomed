#ifndef RV_RUN_H
#define RV_RUN_H
#include "rv32i.h"

typedef enum {
    RV_RUN_COMPLETE, RV_RUN_GUEST_FAILED, RV_RUN_BAD_SIGNATURE,
    RV_RUN_INVALID_SIGNATURE, RV_RUN_BUDGET, RV_RUN_TRAP
} RvRunStatus;
typedef struct {
    RvRunStatus status;
    uint64_t steps;
    RvTrap trap;
} RvRunResult;

/* Completion is a guest RAM write, not an ISA HALT. Check completion before
 * checking the budget so the last allowed instruction may publish success.
 */
RvRunResult rv_run_signature(RvCpu *cpu, uint32_t signature, uint64_t budget);
#endif
