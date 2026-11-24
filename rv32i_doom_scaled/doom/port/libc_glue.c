/* Restricted read-only guest filesystem + console + heap syscalls.
 * GPL-2.0-or-later. No guest request can open an arbitrary host path. */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
extern uint32_t dg_call(uint32_t,uint32_t,uint32_t,uint32_t);
extern unsigned char __heap_start[],__heap_end[];
static uintptr_t brk_address;
void *sbrk(ptrdiff_t increment)
{
    if (brk_address==0u) brk_address=(uintptr_t)__heap_start;
    const int64_t next=(int64_t)brk_address+increment;
    if (next<(int64_t)(uintptr_t)__heap_start || next>(int64_t)(uintptr_t)__heap_end) {
        errno=ENOMEM; return (void *)(intptr_t)-1;
    }
    void *old=(void *)brk_address; brk_address=(uintptr_t)next; return old;
}
static int put(char c,FILE *stream)
{ (void)stream; (void)dg_call(6,(uint32_t)(unsigned char)c,0,0); return 0; }
static int get(FILE *stream) { (void)stream; return _FDEV_EOF; }
static FILE console=FDEV_SETUP_STREAM(put,get,NULL,_FDEV_SETUP_RW);
FILE *const stdin=&console;
FILE *const stdout=&console;
FILE *const stderr=&console;
static char *environment[]={"HOME=.",NULL};
char **environ=environment;
static struct { int used; uint32_t offset; } handles[8];
static uint32_t asset_size(void)
{ return ((volatile uint32_t *)(uintptr_t)0x1f0000u)[1]; }
static const uint8_t *asset_base(void)
{ return (const uint8_t *)(uintptr_t)((volatile uint32_t *)(uintptr_t)0x1f0000u)[0]; }
static int valid(int fd) { return fd>=3 && fd<8 && handles[fd].used; }
int open(const char *path,int flags,...)
{
    const char *base=strrchr(path,'/'); base=base==NULL ? path:base+1;
    if ((flags & (O_WRONLY|O_RDWR|O_CREAT|O_TRUNC))!=0) { errno=EROFS; return -1; }
    if (strcmp(base,"freedoom1.wad")!=0) { errno=ENOENT; return -1; }
    for (int fd=3;fd<8;++fd) if (!handles[fd].used) {
        handles[fd].used=1; handles[fd].offset=0; return fd;
    }
    errno=EMFILE; return -1;
}
int close(int fd)
{
    if (!valid(fd)) { errno=EBADF; return -1; }
    handles[fd].used=0; return 0;
}
ssize_t read(int fd,void *buffer,size_t count)
{
    if (!valid(fd)) { errno=EBADF; return -1; }
    uint32_t available=asset_size()-handles[fd].offset;
    if (count>available) count=available;
    (void)memcpy(buffer,asset_base()+handles[fd].offset,count);
    handles[fd].offset+=(uint32_t)count; return (ssize_t)count;
}
ssize_t write(int fd,const void *buffer,size_t count)
{
    if (fd!=1 && fd!=2) { errno=EROFS; return -1; }
    const uint8_t *p=buffer;
    for (size_t i=0;i<count;++i) (void)dg_call(6,p[i],0,0);
    return (ssize_t)count;
}
off_t lseek(int fd,off_t offset,int whence)
{
    if (!valid(fd)) { errno=EBADF; return -1; }
    int64_t start;
    if (whence==SEEK_SET) start=0;
    else if (whence==SEEK_CUR) start=handles[fd].offset;
    else if (whence==SEEK_END) start=asset_size();
    else { errno=EINVAL; return -1; }
    const int64_t pos=start+offset;
    if (pos<0 || pos>asset_size()) { errno=EINVAL; return -1; }
    handles[fd].offset=(uint32_t)pos; return (off_t)pos;
}
int fstat(int fd,struct stat *st)
{
    if (!valid(fd)) { errno=EBADF; return -1; }
    memset(st,0,sizeof *st); st->st_mode=S_IFREG|0444; st->st_size=(off_t)asset_size(); return 0;
}
int stat(const char *path,struct stat *st)
{ int fd=open(path,O_RDONLY); if(fd<0)return -1; int r=fstat(fd,st); (void)close(fd); return r; }
int isatty(int fd) { return fd==1 || fd==2; }
int access(const char *path,int mode)
{ if ((mode&W_OK)!=0) { errno=EROFS; return -1; } int fd=open(path,O_RDONLY); if(fd<0)return -1; return close(fd); }
int mkdir(const char *path,mode_t mode) { (void)path; (void)mode; errno=EROFS; return -1; }
int unlink(const char *path) { (void)path; errno=EROFS; return -1; }
int rename(const char *old_path,const char *new_path)
{ (void)old_path; (void)new_path; errno=EROFS; return -1; }
char *getcwd(char *buffer,size_t size)
{ if(buffer==NULL) { errno=EINVAL; return NULL; } if(size<2u) { errno=ERANGE; return NULL; } buffer[0]='.'; buffer[1]='\0'; return buffer; }
int system(const char *command) { (void)command; errno=ENOSYS; return -1; }
int gettimeofday(struct timeval *tv,void *tz)
{ (void)tz; uint32_t ms=dg_call(1,0,0,0); tv->tv_sec=(time_t)(ms/1000u); tv->tv_usec=(suseconds_t)((ms%1000u)*1000u); return 0; }
void _exit(int code)
{ (void)dg_call(5,(uint32_t)code,0,0); for(;;) {} }
