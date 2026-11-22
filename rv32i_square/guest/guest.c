#include <stdint.h>
#include "protocol.h"

_Static_assert(sizeof(uint32_t)==4 && sizeof(void *)==4,"RV32/ILP32 required");
volatile uint32_t initialized_global=7; /* image contains these .data bytes */
volatile uint32_t zero_global;         /* startup must clear .bss */
volatile uint32_t values[6];
volatile uint32_t nested_count;

__attribute__((noinline)) uint32_t inner(uint32_t v)
{
    ++nested_count;
    return (v<<1)+initialized_global;
}
__attribute__((noinline)) uint32_t outer(uint32_t v)
{
    return inner(v)+1u;
}
uint32_t guest_main(void)
{
    uint32_t before=zero_global | nested_count;
    for (uint32_t i=0;i<6u;++i) before |= values[i];
    uint32_t checksum=0;
    for (uint32_t i=0;i<6u;++i) {
        values[i]=outer(i+1u); /* guest_main -> outer -> inner */
        checksum+=values[i];
    }
    volatile uint32_t *const signature=(volatile uint32_t *)(uintptr_t)GUEST_SIGNATURE;
    signature[SIG_CHECKSUM/4u]=checksum;
    signature[SIG_COUNT/4u]=6;
    signature[SIG_INITIALIZED/4u]=initialized_global;
    signature[SIG_BSS_BEFORE/4u]=before;
    signature[SIG_NESTED_COUNT/4u]=nested_count;
    return before==0u ? 0u : 1u; /* assembly publishes DONE/FAIL LAST */
}
