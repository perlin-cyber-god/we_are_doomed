#ifndef GUEST_PROTOCOL_H
#define GUEST_PROTOCOL_H
/* Byte addresses in the default 64 KiB guest. This is ordinary RAM, not MMIO.
 * Payload words are written first; status is published LAST after FENCE.
 * Host clears all six words before running. Only guest instructions publish.
 */
#define GUEST_RAM_BYTES       0x10000u
#define GUEST_PROGRAM_LIMIT   0x0c000u
#define GUEST_SIGNATURE       0x0c000u
#define GUEST_SIGNATURE_BYTES 64u
#define GUEST_STACK_BOTTOM    0x0f000u
#define GUEST_STACK_TOP       0x10000u
#define GUEST_DONE            0x444f4e45u
#define GUEST_FAILED          0x4641494cu
#define SIG_STATUS           0u
#define SIG_CHECKSUM         4u
#define SIG_COUNT            8u
#define SIG_INITIALIZED      12u
#define SIG_BSS_BEFORE        16u
#define SIG_NESTED_COUNT      20u
#define SIG_USED_BYTES       24u
#endif
