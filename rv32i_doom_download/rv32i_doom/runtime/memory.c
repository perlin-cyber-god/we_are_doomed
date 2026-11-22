#include "runtime.h"
void *memcpy(void *dst,const void *src,size_t n)
{
    unsigned char *d=dst; const unsigned char *s=src;
    for (size_t i=0;i<n;++i) d[i]=s[i];
    return dst;
}
void *memmove(void *dst,const void *src,size_t n)
{
    unsigned char *d=dst; const unsigned char *s=src;
    /* Address ordering is defined by this flat-address execution environment.
     * Integer comparison avoids comparing pointers to unrelated C objects. */
    if ((uintptr_t)d<(uintptr_t)s) return memcpy(dst,src,n);
    for (size_t i=n;i>0;--i) d[i-1u]=s[i-1u];
    return dst;
}
void *memset(void *dst,int value,size_t n)
{
    unsigned char *d=dst;
    for (size_t i=0;i<n;++i) d[i]=(unsigned char)value;
    return dst;
}
int memcmp(const void *a,const void *b,size_t n)
{
    const unsigned char *x=a,*y=b;
    for (size_t i=0;i<n;++i) if (x[i]!=y[i]) return x[i]<y[i] ? -1:1;
    return 0;
}
size_t strlen(const char *s)
{ size_t n=0; while (s[n]!='\0') ++n; return n; }
int strcmp(const char *a,const char *b)
{
    while (*a!='\0' && *a==*b) { ++a; ++b; }
    return (int)(unsigned char)*a-(int)(unsigned char)*b;
}
size_t rt_strlcpy(char *dst,const char *src,size_t capacity)
{
    const size_t n=strlen(src);
    if (capacity!=0u) {
        const size_t copy=n<capacity-1u ? n:capacity-1u;
        (void)memcpy(dst,src,copy); dst[copy]='\0';
    }
    return n;
}
