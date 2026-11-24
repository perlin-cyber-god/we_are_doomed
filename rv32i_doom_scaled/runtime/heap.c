#include "runtime.h"
#define END UINT32_MAX
#define MAGIC 0x48454150u
/* Payloads and headers are 16-aligned. Sizes count payload bytes only. */
typedef struct { uint32_t size,next,used,magic; } Block;
_Static_assert(sizeof(Block)==16,"heap header must occupy 16 bytes");
static uint8_t *base;
static uint32_t capacity;
static unsigned errors;
static Block *block(uint32_t offset) { return (Block *)(void *)(base+offset); }
static bool valid(uint32_t offset)
{
    if ((offset & 15u)!=0u || offset>capacity-16u) return false;
    const Block *b=block(offset);
    if (b->magic!=MAGIC || b->used>1u || (b->size & 15u)!=0u || b->size>capacity-offset-16u) return false;
    if (b->next==END) return offset+16u+b->size==capacity;
    return b->next==offset+16u+b->size && b->next<=capacity-16u;
}
bool rt_heap_init(void *arena,size_t bytes)
{
    if (arena==NULL || ((uintptr_t)arena & 15u)!=0u || bytes<32u || bytes>UINT32_MAX || (bytes & 15u)!=0u)
        return false;
    base=arena; capacity=(uint32_t)bytes; errors=0;
    *block(0)=(Block){capacity-16u,END,0,MAGIC};
    return true;
}
unsigned rt_heap_errors(void) { return errors; }
size_t rt_heap_available(void)
{
    if (base==NULL) return 0;
    size_t total=0;
    for (uint32_t off=0;off!=END;off=block(off)->next) {
        if (!valid(off)) { ++errors; return 0; }
        if (block(off)->used==0u) total+=block(off)->size;
    }
    return total;
}
void *malloc(size_t n)
{
    if (base==NULL || n==0u || n>UINT32_MAX-15u) return NULL;
    const uint32_t wanted=((uint32_t)n+15u)&~15u;
    for (uint32_t off=0;off!=END;off=block(off)->next) {
        if (!valid(off)) { ++errors; return NULL; }
        Block *b=block(off);
        if (b->used!=0u || b->size<wanted) continue;
        if (b->size-wanted>=32u) {
            const uint32_t split=off+16u+wanted;
            *block(split)=(Block){b->size-wanted-16u,b->next,0,MAGIC};
            b->size=wanted; b->next=split;
        }
        b->used=1;
        return base+off+16u;
    }
    return NULL;
}
static Block *find(void *p)
{
    if (base==NULL) return NULL;
    const uintptr_t address=(uintptr_t)p, start=(uintptr_t)base;
    if (address<start+16u || address-start>=capacity || (address & 15u)!=0u) return NULL;
    for (uint32_t off=0;off!=END;off=block(off)->next) {
        if (!valid(off)) return NULL;
        if (address==start+off+16u) return block(off)->used!=0u ? block(off):NULL;
    }
    return NULL;
}
void free(void *p)
{
    if (p==NULL) return;
    Block *b=find(p);
    if (b==NULL) { ++errors; return; }
    b->used=0;
    /* Merge in address order; adjacent free blocks become one usable block. */
    for (uint32_t off=0;off!=END;) {
        if (!valid(off)) { ++errors; return; }
        Block *a=block(off);
        if (a->next!=END && a->used==0u) {
            if (!valid(a->next)) { ++errors; return; }
            Block *next=block(a->next);
            if (next->used==0u) {
                a->size+=16u+next->size; a->next=next->next; continue;
            }
        }
        off=a->next;
    }
}
void *calloc(size_t count,size_t size)
{
    if (count!=0u && size>SIZE_MAX/count) return NULL;
    const size_t bytes=count*size;
    void *p=malloc(bytes);
    if (p!=NULL) (void)memset(p,0,bytes);
    return p;
}
void *realloc(void *p,size_t size)
{
    if (p==NULL) return malloc(size);
    Block *b=find(p);
    if (b==NULL) { ++errors; return NULL; }
    if (size==0u) { free(p); return NULL; }
    if (size<=b->size) return p; /* shrinking preserves spare capacity */
    void *newp=malloc(size);
    if (newp==NULL) return NULL; /* old allocation and bytes remain valid */
    (void)memcpy(newp,p,b->size);
    free(p);
    return newp;
}
