// See LICENSE for license details.

#include "syscall.h"
#include "pk.h"
#include "file.h"
#include "frontend.h"
#include "vm.h"
#include <string.h>
#include <errno.h>
//#include <fcntl.h>

typedef long (*syscall_t)(long, long, long, long, long, long, long);

#define long_bytes (4 + 4*current.elf64)
#define get_long(base, i) ({ long res; \
  if (current.elf64) res = ((long*)base)[i]; \
  else res = ((int*)base)[i]; \
  res; })
#define put_long(base, i, data) ({ long res; \
  if (current.elf64) ((long*)base)[i] = (data); \
  else ((int*)base)[i] = (data); })

#define CLOCK_FREQ 1000000000

void sys_exit(int code)
{
  if (current.t0)
    printk("%ld cycles\n", rdcycle() - current.t0);

  frontend_syscall(SYS_exit, code, 0, 0, 0, 0);
  while (1);
}

ssize_t sys_read(int fd, char* buf, size_t n)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_read(f, buf, n);
    file_decref(f);
  }

  return r;
}

ssize_t sys_pread(int fd, char* buf, size_t n, off_t offset)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_pread(f, buf, n, offset);
    file_decref(f);
  }

  return r;
}

ssize_t sys_write(int fd, const char* buf, size_t n)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_write(f, buf, n);
    file_decref(f);
  }

  return r;
}

int sys_open(const char* name, int flags, int mode)
{
  file_t* file = file_open(name, flags, mode);
  if (IS_ERR_VALUE(file))
    return PTR_ERR(file);

  int fd = file_dup(file);
  if (fd < 0)
    return -ENOMEM;

  return fd;
}

int sys_openat(int dirfd, const char* name, int flags, int mode)
{
  if(name[0] == '/'){
    return sys_open(name, flags, mode);
  }
  file_t* dir = file_get(dirfd);
  if(dir)
  {
    file_t* file = file_openat(dir->kfd, name, flags, mode);
    if (IS_ERR_VALUE(file))
      return PTR_ERR(file);

    int fd = file_dup(file);
    if (fd < 0)
      return -ENOMEM;

    return fd;
   }
  return -EBADF;
}

int sys_close(int fd)
{
  int ret = fd_close(fd);
  if (ret < 0)
    return -EBADF;
  return ret;
}

int sys_fstat(int fd, void* st)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_stat(f, st);
    file_decref(f);
  }

  return r;
}

int sys_fcntl(int fd, int cmd, int arg)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = frontend_syscall(SYS_fcntl, f->kfd, cmd, arg, 0, 0);
    file_decref(f);
  }

  return r;
}

int sys_dup(int fd)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_dup(f);
    file_decref(f);
  }

  return r;
}

ssize_t sys_lseek(int fd, size_t ptr, int dir)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_lseek(f, ptr, dir);
    file_decref(f);
  }

  return r;
}

long sys_stat(const char* name, void* st)
{
  size_t name_size = strlen(name)+1;
  populate_mapping(st, sizeof(struct stat), PROT_WRITE);
  return frontend_syscall(SYS_stat, (uintptr_t)name, name_size, (uintptr_t)st, 0, 0);
}

long sys_lstat(const char* name, void* st)
{
  size_t name_size = strlen(name)+1;
  populate_mapping(st, sizeof(struct stat), PROT_WRITE);
  return frontend_syscall(SYS_lstat, (uintptr_t)name, name_size, (uintptr_t)st, 0, 0);
}
long sys_fstatat(int dirfd, const char* name, void* st, int flags)
{
  if(name[0] == '/'){
    return sys_stat(name, st);
  }
  file_t* dir = file_get(dirfd);
  if(dir)
  {
    size_t name_size = strlen(name)+1;
    populate_mapping(st, sizeof(struct stat), PROT_WRITE);
    return frontend_syscall(SYS_fstatat, dir->kfd, (uintptr_t)name, name_size, (uintptr_t)st, flags);
  }
  return -EBADF;
}

int sys_access(const char *name, int mode){
  size_t name_size = strlen(name)+1;
  return frontend_syscall(SYS_access, (uintptr_t)name, name_size, mode, 0, 0);
}

int sys_faccessat(int dirfd, const char *name, int mode, int flags){
  if(name[0] == '/'){
    return sys_access(name, mode);
  }
  file_t* dir = file_get(dirfd);
  if(dir)
  {
    size_t name_size = strlen(name)+1;
    return frontend_syscall(SYS_access, dir->kfd, (uintptr_t)name, name_size, mode, flags);
  }
  return -EBADF;
}

long sys_link(const char* old_name, const char* new_name)
{
  size_t old_size = strlen(old_name)+1;
  size_t new_size = strlen(new_name)+1;
  return frontend_syscall(SYS_link, (uintptr_t)old_name, old_size,
                                    (uintptr_t)new_name, new_size, 0);
}

long sys_unlink(const char* name, size_t len)
{
  size_t name_size = strlen(name)+1;
  return frontend_syscall(SYS_unlink, (uintptr_t)name, name_size, 0, 0, 0);
}

long sys_mkdir(const char* name, int mode)
{
  size_t name_size = strlen(name)+1;
  return frontend_syscall(SYS_mkdir, (uintptr_t)name, name_size, mode, 0, 0);
}

long sys_getcwd(const char* buf, size_t size)
{
  populate_mapping(buf, size, PROT_WRITE);
  return frontend_syscall(SYS_getcwd, (uintptr_t)buf, size, 0, 0, 0);
}

size_t sys_brk(size_t pos)
{
  return do_brk(pos);
}

int sys_uname(void* buf)
{
  const int sz = 65;
  strcpy(buf + 0*sz, "Proxy Kernel");
  strcpy(buf + 1*sz, "");
  strcpy(buf + 2*sz, "3.4.5");
  strcpy(buf + 3*sz, "");
  strcpy(buf + 4*sz, "");
  strcpy(buf + 5*sz, "");
  return 0;
}

pid_t sys_getpid()
{
  return 0;
}

int sys_getuid()
{
  return 0;
}

uintptr_t sys_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  uintptr_t ret =  do_mmap(addr, length, prot, flags, fd, offset);
  return ret;
}

int sys_munmap(uintptr_t addr, size_t length)
{
  return do_munmap(addr, length);
}

uintptr_t sys_mremap(uintptr_t addr, size_t old_size, size_t new_size, int flags)
{
  return do_mremap(addr, old_size, new_size, flags);
}

uintptr_t sys_mprotect(uintptr_t addr, size_t length, int prot)
{
  return do_mprotect(addr, length, prot);
}

int sys_rt_sigaction(int sig, const void* act, void* oact, size_t sssz)
{
  if (oact)
  {
    size_t sz = long_bytes * 3;
    populate_mapping(oact, sz, PROT_WRITE);
    memset(oact, 0, sz);
  }

  return 0;
}

long sys_time(void* loc)
{
  uintptr_t t = rdcycle() / CLOCK_FREQ;
  if (loc)
  {
    populate_mapping(loc, long_bytes, PROT_WRITE);
    put_long(loc, 0, t);
  }
  return t;
}

int sys_times(void* restrict loc)
{
  populate_mapping(loc, 4*long_bytes, PROT_WRITE);

  uintptr_t t = rdcycle();
  kassert(CLOCK_FREQ % 1000000 == 0);
  put_long(loc, 0, t / (CLOCK_FREQ / 1000000));
  put_long(loc, 1, 0);
  put_long(loc, 2, 0);
  put_long(loc, 3, 0);
  
  return 0;
}

int sys_gettimeofday(long* loc)
{
  populate_mapping(loc, 2*long_bytes, PROT_WRITE);

  uintptr_t t = rdcycle();
  put_long(loc, 0, t/CLOCK_FREQ);
  put_long(loc, 1, (t % CLOCK_FREQ) / (CLOCK_FREQ / 1000000));
  
  return 0;
}

ssize_t sys_writev(int fd, const void* iov, int cnt)
{
  populate_mapping(iov, cnt*2*long_bytes, PROT_READ);

  ssize_t ret = 0;
  for (int i = 0; i < cnt; i++)
  {
    ssize_t r = sys_write(fd, (void*)get_long(iov, 2*i), get_long(iov, 2*i+1));
    if (r < 0)
      return r;
    ret += r;
  }
  return ret;
}


int sys_getdents(int fd, void* dirbuf, int count)
{
  return 0; //stub
}

long syscall(long a0, long a1, long a2, long a3, long a4, long a5, long n)
{
  const static void* syscall_table[] = {
    [SYS_exit] = sys_exit,
    [SYS_exit_group] = sys_exit,
    [SYS_read] = sys_read,
    [SYS_pread] = sys_pread,
    [SYS_write] = sys_write,
    [SYS_open] = sys_open,
    [SYS_openat] = sys_openat,
    [SYS_close] = sys_close,
    [SYS_fstat] = sys_fstat,
    [SYS_lseek] = sys_lseek,
    [SYS_stat] = sys_stat,
    [SYS_lstat] = sys_lstat,
    [SYS_fstatat] = sys_fstatat,
    [SYS_link] = sys_link,
    [SYS_unlink] = sys_unlink,
    [SYS_mkdir] = sys_mkdir,
    [SYS_getcwd] = sys_getcwd,
    [SYS_brk] = sys_brk,
    [SYS_uname] = sys_uname,
    [SYS_getpid] = sys_getpid,
    [SYS_getuid] = sys_getuid,
    [SYS_geteuid] = sys_getuid,
    [SYS_getgid] = sys_getuid,
    [SYS_getegid] = sys_getuid,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_mremap] = sys_mremap,
    [SYS_mprotect] = sys_mprotect,
    [SYS_rt_sigaction] = sys_rt_sigaction,
    [SYS_time] = sys_time,
    [SYS_gettimeofday] = sys_gettimeofday,
    [SYS_times] = sys_times,
    [SYS_writev] = sys_writev,
    [SYS_access] = sys_access,
    [SYS_faccessat] = sys_faccessat,
    [SYS_fcntl] = sys_fcntl,
    [SYS_getdents] = sys_getdents,
    [SYS_dup] = sys_dup,
  };

  if(n >= ARRAY_SIZE(syscall_table) || !syscall_table[n])
    panic("bad syscall #%ld!",n);

  long r = ((syscall_t)syscall_table[n])(a0, a1, a2, a3, a4, a5, n);
  return r;
}
