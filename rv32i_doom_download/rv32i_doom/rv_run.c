#include "rv_run.h"
#include "guest/protocol.h"

RvRunResult rv_run_signature(RvCpu *c, uint32_t sig, uint64_t budget)
{
    RvRunResult r = { .status = RV_RUN_INVALID_SIGNATURE };
    if ((sig & 3u) != 0u || c->ram_size < SIG_USED_BYTES ||
        (uint64_t)sig > (uint64_t)(c->ram_size - SIG_USED_BYTES)) return r;
    for (;;) {
        uint32_t status=0;
        if (!rv_peek_u32(c,sig,&status)) return r;
        if (status==GUEST_DONE) { r.status=RV_RUN_COMPLETE; return r; }
        if (status==GUEST_FAILED) { r.status=RV_RUN_GUEST_FAILED; return r; }
        if (status!=0u) { r.status=RV_RUN_BAD_SIGNATURE; return r; }
        if (r.steps==budget) { r.status=RV_RUN_BUDGET; return r; }
        if (rv_step(c,&r.trap)!=RV_OK) { r.status=RV_RUN_TRAP; return r; }
        ++r.steps;
    }
}
