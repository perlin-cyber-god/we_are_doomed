#ifndef DEMO_PLATFORM_H
#define DEMO_PLATFORM_H
#include "../rv32i.h"
#include "abi.h"
typedef struct Platform Platform;
enum { PL_YIELD, PL_FRAME, PL_WAIT, PL_STOP, PL_ERROR };
Platform *pl_create(const uint8_t *image, size_t length);
void pl_destroy(Platform *p);
/* Execute a bounded slice; time is a caller-supplied 32-bit millisecond clock.
 * Caller must initialize through pl_create; keep inputs fixed for a slice.
 */
int pl_run(Platform *p, uint32_t keys, uint32_t milliseconds, uint32_t budget);
const uint8_t *pl_frame(const Platform *p);
uint64_t pl_steps(const Platform *p);
const char *pl_error(const Platform *p);
uint32_t pl_exit_code(const Platform *p);
uint32_t pl_pc(const Platform *p);
uint32_t pl_register(const Platform *p, unsigned index);
#endif
