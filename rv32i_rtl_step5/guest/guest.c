#include <stdint.h>

_Static_assert(sizeof(uint32_t)==4 && sizeof(void *)==4,"RV32/ILP32 required");
enum { SIGNATURE_ADDRESS=0x2000, COUNT=6 };

volatile uint32_t initialized_global=7; /* Real .data bytes in the load image. */
volatile uint32_t zero_global;         /* Startup, not the host, must clear .bss. */
volatile uint32_t values[COUNT];
volatile uint32_t nested_count;

__attribute__((noinline)) uint32_t inner(uint32_t value)
{
    ++nested_count;
    return (value<<1)+initialized_global;
}
__attribute__((noinline)) uint32_t outer(uint32_t value)
{
    return inner(value)+1u;
}
uint32_t guest_main(void)
{
    uint32_t before=zero_global | nested_count;
    for(uint32_t i=0;i<COUNT;++i) before |= values[i];
    uint32_t checksum=0;
    for(uint32_t i=0;i<COUNT;++i) {
        values[i]=outer(i+1u); /* guest_main -> outer -> inner */
        checksum+=values[i];
    }
    volatile uint32_t *const signature=(volatile uint32_t *)(uintptr_t)SIGNATURE_ADDRESS;
    signature[1]=checksum;
    signature[2]=COUNT;
    signature[3]=initialized_global;
    signature[4]=before;
    signature[5]=nested_count;
    return before==0u ? 0u : 1u; /* Assembly publishes DONE or FAIL last. */
}
