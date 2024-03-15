#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "pagedir.h"


static void syscall_handler (struct intr_frame *);

/** GET nth argument from user esp*/
bool
argraw(int n, struct intr_frame *f, uint32_t *ret)
{
  // return read_from_user(f->esp + n * sizeof(uint32_t), ret, sizeof(uint32_t));
  if (check_user_pointer(f->esp + n * sizeof(uint32_t), sizeof(uint32_t), false, f)) {
    *ret = *(uint32_t *)(f->esp + n * sizeof(uint32_t));
    return true;
  }
  return false;
}

extern uint32_t sys_halt(struct intr_frame *f);
extern uint32_t sys_exit(struct intr_frame *f);
extern uint32_t sys_exec(struct intr_frame *f);
extern uint32_t sys_wait(struct intr_frame *f);
extern uint32_t sys_create(struct intr_frame *f);
extern uint32_t sys_remove(struct intr_frame *f);
extern uint32_t sys_open(struct intr_frame *f);
extern uint32_t sys_filesize(struct intr_frame *f);
extern uint32_t sys_read(struct intr_frame *f);
extern uint32_t sys_write(struct intr_frame *f);
extern uint32_t sys_seek(struct intr_frame *f);
extern uint32_t sys_tell(struct intr_frame *f);
extern uint32_t sys_close(struct intr_frame *f);
extern uint32_t sys_mmap(struct intr_frame *f);
extern uint32_t sys_munmap(struct intr_frame *f);
extern uint32_t sys_chdir(struct intr_frame *f);
extern uint32_t sys_mkdir(struct intr_frame *f);
extern uint32_t sys_readdir(struct intr_frame *f);
extern uint32_t sys_isdir(struct intr_frame *f);
extern uint32_t sys_inumber(struct intr_frame *f);


static uint32_t (*syscalls[])(struct intr_frame *f) = {
[SYS_HALT]    sys_halt,
[SYS_EXIT]    sys_exit,
[SYS_EXEC]    sys_exec,
[SYS_WAIT]    sys_wait,
[SYS_CREATE]    sys_create,
[SYS_REMOVE]    sys_remove,
[SYS_OPEN]    sys_open,
[SYS_FILESIZE]   sys_filesize,
[SYS_READ]   sys_read,
[SYS_WRITE]     sys_write,
[SYS_SEEK]  sys_seek,
[SYS_TELL]    sys_tell,
[SYS_CLOSE]   sys_close,
[SYS_MMAP]  sys_mmap,
[SYS_MUNMAP]    sys_munmap,
[SYS_CHDIR]   sys_chdir,
[SYS_MKDIR]   sys_mkdir,
[SYS_READDIR]  sys_readdir,
[SYS_ISDIR]    sys_isdir,
[SYS_INUMBER]   sys_inumber,
};

static char * sysCallName[] = {
[SYS_HALT]    "SYS_HALT",
[SYS_EXIT]    "SYS_EXIT",
[SYS_EXEC]    "SYS_EXEC",
[SYS_WAIT]    "SYS_WAIT",
[SYS_CREATE]    "SYS_CREATE",
[SYS_REMOVE]    "SYS_REMOVE",
[SYS_OPEN]    "SYS_OPEN",
[SYS_FILESIZE]   "SYS_FILESIZE",
[SYS_READ]   "SYS_READ",
[SYS_WRITE]     "SYS_WRITE",
[SYS_SEEK]  "SYS_SEEK",
[SYS_TELL]    "SYS_TELL",
[SYS_CLOSE]   "SYS_CLOSE",
[SYS_MMAP]  "SYS_MMAP",
[SYS_MUNMAP]    "SYS_MUNMAP",
[SYS_CHDIR]   "SYS_CHDIR",
[SYS_MKDIR]   "SYS_MKDIR",
[SYS_READDIR]  "SYS_READDIR",
[SYS_ISDIR]    "SYS_ISDIR",
[SYS_INUMBER]   "SYS_INUMBER",
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num;
  bool success = argraw(0, f, &syscall_num);
  if (!success) {
    // printf("Error reading syscall number\n");
    thread_exit_with_status(-1);
  }
  if (syscall_num < 0 || syscall_num >= sizeof(syscalls)/sizeof(syscalls[0]))
  {
    printf ("Unknown system call : %d\n", syscall_num);
    thread_exit_with_status(-1);
  }           
  // printf ("system call : %s\n", sysCallName[syscall_num]);
  // thread_exit ();
  f->eax = syscalls[syscall_num](f);
}
