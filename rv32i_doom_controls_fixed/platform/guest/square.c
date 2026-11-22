#include <stdint.h>
#include "../abi.h"
extern uint32_t platform_call(uint32_t service,uint32_t argument);
static volatile uint32_t *const pixels=(volatile uint32_t *)(uintptr_t)PL_FB;

static void draw(int32_t x, int32_t y)
{
    /* Every pixel is written by guest SW instructions. Host draws no square. */
    for (uint32_t i=0;i<PL_PIXELS;++i) pixels[i]=0x00102030u;
    for (uint32_t row=0;row<8u;++row) {
        const uint32_t yy=(uint32_t)y+row;
        const uint32_t start=(yy<<7)+(yy<<5)+(uint32_t)x; /* yy*160+x */
        for (uint32_t col=0;col<8u;++col) pixels[start+col]=0x00ffcc00u;
    }
}
uint32_t square_main(void)
{
    int32_t x=76,y=46;
    uint32_t last=platform_call(PL_TICKS,0);
    for (;;) {
        const uint32_t keys=platform_call(PL_KEYS,0);
        if ((keys & PL_QUIT)!=0u) return 0;
        const uint32_t now=platform_call(PL_TICKS,0);
        if ((uint32_t)(now-last)>=16u) {
            x+=((keys & PL_RIGHT)!=0u ? 1:0)-((keys & PL_LEFT)!=0u ? 1:0);
            y+=((keys & PL_DOWN)!=0u ? 1:0)-((keys & PL_UP)!=0u ? 1:0);
            if (x<0) x=0;
            if (x>152) x=152;
            if (y<0) y=0;
            if (y>92) y=92;
            last=now;
        }
        draw(x,y);
        (void)platform_call(PL_PRESENT,0);
        (void)platform_call(PL_SLEEP,16);
    }
}
