#ifndef MINI_RUNTIME_H
#define MINI_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
void *memcpy(void *dst,const void *src,size_t n);
void *memmove(void *dst,const void *src,size_t n);
void *memset(void *dst,int value,size_t n);
int memcmp(const void *a,const void *b,size_t n);
size_t strlen(const char *s);
int strcmp(const char *a,const char *b);
size_t rt_strlcpy(char *dst,const char *src,size_t capacity);

/* One contiguous 16-aligned arena; init returns false without touching it if
 * alignment/size is invalid. No concurrency. Invalid free is diagnosed. */
bool rt_heap_init(void *arena,size_t bytes);
size_t rt_heap_available(void);
unsigned rt_heap_errors(void);
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t count,size_t size);
void *realloc(void *p,size_t size);

typedef struct { const uint8_t *data; uint32_t size; } RtFile;
typedef struct { RtFile file; uint32_t count,directory; } RtWad;
typedef struct { uint32_t offset,size; } RtLump;
size_t rt_read_at(RtFile file,uint32_t offset,void *dst,size_t count);
bool rt_wad_open(RtFile file,RtWad *wad);
/* name points to exactly eight bytes, including any zero padding. */
bool rt_wad_find(const RtWad *wad,const char name[8],RtLump *lump);
bool rt_fixed_mul(int32_t a,int32_t b,int32_t *out);
bool rt_fixed_div(int32_t a,int32_t b,int32_t *out);
#endif
