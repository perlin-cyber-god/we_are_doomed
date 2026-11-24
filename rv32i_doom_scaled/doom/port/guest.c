/* Doom port glue; GPL-2.0-or-later, matching the linked engine. */
#include <stdint.h>
#include "doomgeneric.h"
#include "doomstat.h"
#include "p_mobj.h"
extern uint32_t dg_call(uint32_t service,uint32_t a,uint32_t b,uint32_t c);
void DG_Init(void) { }
void DG_DrawFrame(void)
{
    /* Read-only host observability: guest copies actual player state into a
     * reserved report area. The host never writes the player's coordinates. */
    volatile uint32_t *report=(volatile uint32_t *)(uintptr_t)0x1f0010u;
    report[0]=0;
    if (gamestate==GS_LEVEL && players[consoleplayer].mo!=NULL) {
        report[1]=(uint32_t)players[consoleplayer].mo->x;
        report[2]=(uint32_t)players[consoleplayer].mo->y;
        report[3]=players[consoleplayer].mo->angle;
        report[0]=0x504c4159u;
    }
    (void)dg_call(3,(uint32_t)(uintptr_t)DG_ScreenBuffer,DOOMGENERIC_RESX,DOOMGENERIC_RESY);
}
void DG_SleepMs(uint32_t ms) { (void)dg_call(4,ms,0,0); }
uint32_t DG_GetTicksMs(void) { return dg_call(1,0,0,0); }
int DG_GetKey(int *pressed,unsigned char *key)
{
    const uint32_t event=dg_call(2,0,0,0);
    if ((event & 0x10000u)==0u) return 0;
    *pressed=(int)((event>>8)&1u); *key=(unsigned char)event; return 1;
}
void DG_SetWindowTitle(const char *title) { (void)title; }
int port_main(void)
{
    char *args[]={"doom","-iwad","freedoom1.wad","-nosound","-nogui",
                  "-warp","1","1","-skill","2",NULL};
    doomgeneric_Create(10,args);
    for (;;) doomgeneric_Tick();
}
