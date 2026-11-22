#include "../runtime.h"
#include <limits.h>
#define HEAP_START ((void *)(uintptr_t)0x30000u)
#define HEAP_BYTES 0x40000u
static uint32_t checks,failures,asset_sum;
#define CHECK(c) do { ++checks; if (!(c)) ++failures; } while (0)

static void memory_tests(void)
{
    unsigned char a[32],b[32];
    CHECK(memset(a,0xa5,sizeof a)==a);
    CHECK(memcpy(b,a,sizeof a)==b && memcmp(a,b,sizeof a)==0);
    b[3]=0xff; CHECK(memcmp(a,b,sizeof a)<0 && memcmp(b,a,sizeof a)>0);
    CHECK(memcmp(a,b,0)==0);
    (void)memcpy(a,"abcdefgh",9);
    CHECK(memmove(a+2,a,6)==a+2 && memcmp(a,"ababcdef",8)==0);
    (void)memmove(a,a+2,6); CHECK(memcmp(a,"abcdef",6)==0);
    CHECK(memmove(a,a,6)==a && memmove(a,a,0)==a);
    CHECK(strlen("")==0u && strlen("doom")==4u);
    CHECK(strcmp("doom","doom")==0 && strcmp("doom","door")<0);
    CHECK(strcmp("\xff","\x7f")>0);
    char out[6];
    CHECK(rt_strlcpy(out,"DOOM",sizeof out)==4u && strcmp(out,"DOOM")==0);
    CHECK(rt_strlcpy(out,"TOOLONG",sizeof out)==7u && strcmp(out,"TOOLO")==0);
    out[0]='x'; CHECK(rt_strlcpy(out,"doom",0)==4u && out[0]=='x');
    CHECK(rt_strlcpy(out,"doom",1)==4u && out[0]=='\0');
}
static void heap_tests(void)
{
    CHECK(rt_heap_init(HEAP_START,HEAP_BYTES));
    const size_t initial=rt_heap_available(); CHECK(initial==HEAP_BYTES-16u);
    CHECK(!rt_heap_init((uint8_t *)HEAP_START+1,HEAP_BYTES-16u));
    CHECK(rt_heap_available()==initial);
    CHECK(malloc(0)==NULL && malloc(SIZE_MAX)==NULL);
    unsigned char *a=malloc(17),*b=malloc(33),*c=malloc(17);
    CHECK(a!=NULL && b!=NULL && c!=NULL);
    if (a==NULL || b==NULL || c==NULL) return;
    CHECK(((uintptr_t)a & 15u)==0u && ((uintptr_t)b & 15u)==0u);
    (void)memset(a,0x11,17); (void)memset(b,0x22,33); (void)memset(c,0x33,17);
    const uintptr_t old_b=(uintptr_t)b;
    free(b); b=malloc(32); CHECK((uintptr_t)b==old_b);
    CHECK(a[16]==0x11u && c[16]==0x33u);
    free(a); free(b); free(c); CHECK(rt_heap_available()==initial);
    uint8_t *z=calloc(31,3); CHECK(z!=NULL);
    if (z==NULL) return;
    for (unsigned i=0;i<93u;++i) CHECK(z[i]==0u);
    free(z); CHECK(calloc(SIZE_MAX,2)==NULL);
    a=malloc(20); CHECK(a!=NULL); if (a==NULL) return;
    for (unsigned i=0;i<20u;++i) a[i]=(uint8_t)i;
    b=realloc(a,100); CHECK(b!=NULL); if (b==NULL) return;
    for (unsigned i=0;i<20u;++i) CHECK(b[i]==i);
    CHECK(realloc(b,8)==b);
    const size_t before=rt_heap_available();
    CHECK(realloc(b,SIZE_MAX)==NULL && rt_heap_available()==before && b[7]==7u);
    CHECK(realloc(b,0)==NULL && rt_heap_available()==initial);
    a=realloc(NULL,16); CHECK(a!=NULL); free(a); free(NULL);
    CHECK(rt_heap_available()==initial);
    /* Exhaustion and coalescing, without a giant native-host allocation. */
    void *slots[64]; unsigned n=0;
    while (n<64u && (slots[n]=malloc(4096))!=NULL) ++n;
    CHECK(n>0u && n<64u);
    for (unsigned i=0;i<n;++i) free(slots[i]);
    CHECK(rt_heap_available()==initial);
    a=malloc(initial); CHECK(a!=NULL && rt_heap_available()==0u);
    CHECK(malloc(1)==NULL); free(a); CHECK(rt_heap_available()==initial);
    a=malloc(16); CHECK(a!=NULL); if (a==NULL) return;
    free(a+1); CHECK(rt_heap_errors()==1u);
    free(a); free(a); CHECK(rt_heap_errors()==2u);
    CHECK(rt_heap_available()==initial);
}
static void asset_tests(void)
{
    const volatile uint32_t *descriptor=(const volatile uint32_t *)(uintptr_t)0x10080u;
    RtFile file={(const uint8_t *)(uintptr_t)descriptor[0],descriptor[1]};
    RtWad wad={0}; RtLump lump={0};
    const bool opened=rt_wad_open(file,&wad);
    CHECK(opened); if (!opened) return;
    CHECK(wad.count==2u);
    const bool found=rt_wad_find(&wad,"GREETING",&lump);
    CHECK(found); if (!found) return;
    uint8_t buf[16]; (void)memset(buf,0xa5,sizeof buf);
    CHECK(rt_read_at(file,lump.offset,buf,lump.size)==7u);
    CHECK(memcmp(buf,"HELLO!",7)==0 && buf[7]==0xa5u);
    CHECK(rt_wad_find(&wad,"NUMBERS",&lump));
    const size_t read=rt_read_at(file,lump.offset,buf,lump.size);
    CHECK(read==4u);
    if (read==4u) for (unsigned i=0;i<4u;++i) asset_sum+=buf[i];
    CHECK(asset_sum==24u);
    CHECK(!rt_wad_find(&wad,"MISSING",&lump));
    CHECK(rt_read_at(file,file.size,buf,4)==0u);
    CHECK(rt_read_at(file,UINT32_MAX,buf,4)==0u);
    CHECK(rt_read_at(file,file.size-1u,buf,9)==1u);
    uint8_t bad[64];
    CHECK(file.size<=sizeof bad); if (file.size>sizeof bad) return;
    (void)memcpy(bad,file.data,file.size);
    const RtWad before=wad;
    bad[0]='X'; CHECK(!rt_wad_open((RtFile){bad,file.size},&wad));
    CHECK(wad.file.data==before.file.data && wad.count==before.count);
    CHECK(!rt_wad_open((RtFile){bad,8},&wad));
    (void)memcpy(bad,file.data,file.size); (void)memset(bad+4,0xff,4);
    CHECK(!rt_wad_open((RtFile){bad,file.size},&wad));
    (void)memcpy(bad,file.data,file.size); (void)memset(bad+8,0xff,4);
    CHECK(!rt_wad_open((RtFile){bad,file.size},&wad));
    (void)memcpy(bad,file.data,file.size); (void)memset(bad+before.directory+4,0xff,4);
    CHECK(!rt_wad_open((RtFile){bad,file.size},&wad));
    /* Duplicate names use the later directory entry. */
    (void)memcpy(bad,file.data,file.size);
    (void)memcpy(bad+before.directory+16u+8u,"GREETING",8);
    CHECK(rt_wad_open((RtFile){bad,file.size},&wad));
    CHECK(rt_wad_find(&wad,"GREETING",&lump) && lump.size==4u);
    (void)memcpy(bad,file.data,file.size); bad[0]='I';
    CHECK(rt_wad_open((RtFile){bad,file.size},&wad));
    /* A zero-length marker at EOF is valid; a nonempty lump beyond EOF is not. */
    (void)memcpy(bad,file.data,file.size);
    for (unsigned i=0;i<4u;++i) bad[before.directory+i]=(uint8_t)(file.size>>(8u*i));
    (void)memset(bad+before.directory+4u,0,4);
    CHECK(rt_wad_open((RtFile){bad,file.size},&wad));
    bad[before.directory+4u]=1;
    CHECK(!rt_wad_open((RtFile){bad,file.size},&wad));
}
static void math_tests(void)
{
    volatile int32_t a=-17,b=3;
    CHECK(a*b==-51 && a/b==-5 && a%b==-2);
    volatile uint32_t x=4000000000u,y=3;
    CHECK(x/y==1333333333u && x%y==1u);
    volatile int64_t big=INT64_C(-1234567890123),den=97;
    CHECK(big/den==INT64_C(-12727504021) && big%den==-86);
    volatile uint64_t wide=UINT64_C(0x123456789),factor=UINT64_C(0x10001);
    CHECK(wide*factor==UINT64_C(0x123468ace6789));
    int32_t out=0;
    CHECK(rt_fixed_mul(98304,147456,&out) && out==221184); /* 1.5*2.25 */
    CHECK(rt_fixed_mul(-98304,147456,&out) && out==-221184);
    CHECK(rt_fixed_mul(-1,1,&out) && out==-1); /* negative fractional rounding */
    CHECK(rt_fixed_div(7*65536,2*65536,&out) && out==229376);
    CHECK(rt_fixed_div(-7*65536,2*65536,&out) && out==-229376);
    out=123;
    CHECK(!rt_fixed_div(65536,0,&out) && out==123);
    CHECK(!rt_fixed_div(INT32_MIN,-1,&out) && out==123);
    CHECK(!rt_fixed_mul(INT32_MAX,INT32_MAX,&out) && out==123);
}
uint32_t runtime_main(void)
{
    memory_tests(); heap_tests(); asset_tests(); math_tests();
    volatile uint32_t *signature=(volatile uint32_t *)(uintptr_t)0x10000u;
    signature[1]=checks; signature[2]=failures; signature[3]=asset_sum;
    signature[4]=(uint32_t)rt_heap_available(); signature[5]=rt_heap_errors();
    return failures;
}
