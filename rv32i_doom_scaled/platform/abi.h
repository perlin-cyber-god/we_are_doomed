#ifndef PLATFORM_ABI_H
#define PLATFORM_ABI_H
#define PL_RAM 0x40000u
#define PL_APP_END 0x8000u
#define PL_FB 0x10000u
#define PL_WIDTH 160u
#define PL_HEIGHT 100u
#define PL_PIXELS (PL_WIDTH * PL_HEIGHT)
#define PL_FRAME_BYTES (PL_PIXELS * 4u)
#define PL_LEFT 1u
#define PL_RIGHT 2u
#define PL_UP 4u
#define PL_DOWN 8u
#define PL_QUIT 16u
#define PL_TICKS 1u
#define PL_KEYS 2u
#define PL_PRESENT 3u
#define PL_SLEEP 4u
#define PL_EXIT 5u
#endif
