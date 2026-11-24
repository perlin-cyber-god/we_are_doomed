#include "runtime.h"
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
size_t rt_read_at(RtFile file,uint32_t offset,void *dst,size_t count)
{
    if (offset>file.size) return 0;
    size_t n=file.size-offset;
    if (n>count) n=count;
    if (n!=0u) (void)memcpy(dst,file.data+offset,n);
    return n;
}
bool rt_wad_open(RtFile file,RtWad *wad)
{
    if (file.size<12u) return false;
    if (memcmp(file.data,"IWAD",4)!=0 && memcmp(file.data,"PWAD",4)!=0) return false;
    const uint32_t count=le32(file.data+4),directory=le32(file.data+8);
    if ((count & 0x80000000u)!=0u || directory>file.size || count>(file.size-directory)/16u) return false;
    for (uint32_t i=0;i<count;++i) {
        const uint8_t *entry=file.data+directory+16u*i;
        const uint32_t offset=le32(entry),size=le32(entry+4);
        if ((offset & 0x80000000u)!=0u || (size & 0x80000000u)!=0u || offset>file.size || size>file.size-offset)
            return false;
    }
    *wad=(RtWad){file,count,directory};
    return true;
}
bool rt_wad_find(const RtWad *wad,const char name[8],RtLump *lump)
{
    /* Search backwards so a later duplicate name overrides an earlier one. */
    for (uint32_t i=wad->count;i>0u;--i) {
        const uint8_t *entry=wad->file.data+wad->directory+16u*(i-1u);
        if (memcmp(entry+8,name,8)==0) { *lump=(RtLump){le32(entry),le32(entry+4)}; return true; }
    }
    return false;
}
