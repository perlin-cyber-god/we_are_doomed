#include <stdint.h>

_Static_assert(sizeof(void *)==4 && sizeof(uint32_t)==4,"RV32/ILP32 required");
enum { TX_DATA=0x10000000,TX_STATUS=0x10000004,TIMER=0x10000010,SIGNATURE=0x2000 };
volatile uint32_t bytes_sent;
volatile uint32_t byte_sum;

__attribute__((noinline)) void console_putc(uint8_t ch)
{
    volatile uint32_t *const status=(volatile uint32_t *)(uintptr_t)TX_STATUS;
    volatile uint8_t *const data=(volatile uint8_t *)(uintptr_t)TX_DATA;
    while((*status & 1u)==0u) { /* Poll readiness; the actual SB can still wait. */ }
    *data=ch;
    ++bytes_sent;
    byte_sum+=ch;
}
__attribute__((noinline)) uint32_t timer_read(void)
{
    return *(volatile uint32_t *)(uintptr_t)TIMER;
}
uint32_t guest_main(void)
{
    const char *text="Hello from our RV32I CPU!\n";
    const uint32_t first=timer_read();
    while(*text!='\0') {
        console_putc((uint8_t)*text);
        ++text;
    }
    __asm__ volatile("fence iorw,iorw" ::: "memory");
    const uint32_t before=timer_read();
    uint32_t after;
    do { after=timer_read(); } while((uint32_t)(after-before)<64u);
    volatile uint32_t *const signature=(volatile uint32_t *)(uintptr_t)SIGNATURE;
    signature[1]=bytes_sent;
    signature[2]=byte_sum;
    signature[3]=first;
    signature[4]=before;
    signature[5]=after;
    signature[6]=after-before; // Unsigned subtraction also works across wrap.
    signature[7]=1;
    return 0; // Startup publishes DONE last, then explicit EBREAK.
}
