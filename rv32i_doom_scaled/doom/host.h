#ifndef DOOM_HOST_H
#define DOOM_HOST_H
#include "../rv32i.h"
typedef struct DoomMachine DoomMachine;
enum { DH_YIELD, DH_FRAME, DH_SLEEP, DH_EXIT, DH_ERROR };
DoomMachine *dh_create(const uint8_t *image,size_t image_size,const uint8_t *wad,size_t wad_size);
void dh_destroy(DoomMachine *m);
int dh_run(DoomMachine *m,uint32_t keys,uint32_t milliseconds,uint32_t budget);
const uint8_t *dh_frame(const DoomMachine *m);
uint64_t dh_steps(const DoomMachine *m);
uint32_t dh_frames(const DoomMachine *m);
uint32_t dh_exit_code(const DoomMachine *m);
const char *dh_error(const DoomMachine *m);
const char *dh_log(const DoomMachine *m);
uint32_t dh_report(const DoomMachine *m,unsigned index);
uint32_t dh_boot_word(const DoomMachine *m);
#endif
