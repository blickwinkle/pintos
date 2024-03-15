#include "threads/thread.h"
#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "pagedir.h"
#include "devices/input.h"


static int 
check_str(char *puser, struct intr_frame *f);

uint32_t sys_halt(struct intr_frame *f) {
    shutdown_power_off();
    NOT_REACHED();
}


uint32_t sys_exit(struct intr_frame *f) {
    struct thread *t = thread_current();
    int status;
    bool success = argraw(1, f, &status);
    if (!success) {
        status = -1;
    }
    thread_exit_with_status(status);
    NOT_REACHED();
}

struct file_descriptor *get_file_descriptor(int fd) {
    struct thread *t = thread_current();
    struct list_elem *e;
    // filesys_getlock();
    for (e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)) {
        struct file_descriptor *fdesc = list_entry(e, struct file_descriptor, elem);
        if (fdesc->fd == fd) {
            return fdesc;
        }
    }
    // filesys_releaselock();
    return NULL;
}

uint32_t sys_write(struct intr_frame *f) {
    int fd;
    bool success = argraw(1, f, &fd);
    void *buffer;
    success = argraw(2, f, &buffer);
    if (!success) {
        thread_exit_with_status(-1);
    }
    unsigned size;
    success = argraw(3, f, &size);
    if (!success) {
        thread_exit_with_status(-1);
    }
    success = check_user_pointer(buffer, size, false, f);
    if (!success) {
        // free(cpy_buffer);
        thread_exit_with_status(-1);
    }
    if (fd == 1) {
        // void *cpy_buffer = malloc(size);
        // success = read_from_user(buffer, cpy_buffer, size);
        
        putbuf(buffer, size);
        // free(cpy_buffer);
        return size;
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }
    filesys_getlock();
    int ret = file_write(file->file, buffer, size);
    filesys_releaselock();
    return ret;
}


uint32_t sys_exec(struct intr_frame *f) {
    void *cmd_line;
    bool success = argraw(1, f, &cmd_line);
    if (!success) {
        thread_exit_with_status(-1);
    }
    
    int size = check_str(cmd_line, f);
    if (size == -1) {
        thread_exit_with_status(-1);
    }
    return process_execute(cmd_line);
}
uint32_t sys_wait(struct intr_frame *f) {
    tid_t tid;
    bool success = argraw(1, f, &tid);
    if (!success) {
        thread_exit_with_status(-1);
    }
    return process_wait(tid);
}
uint32_t sys_create(struct intr_frame *f){
    char *file;
    unsigned initial_size;
    bool success = argraw(1, f, &file);
    if (!success) {
        thread_exit_with_status(-1);
    }
    success = argraw(2, f, &initial_size);
    if (!success) {
        thread_exit_with_status(-1);
    }
    int len = check_str(file, f);
    if (len == -1) {
        thread_exit_with_status(-1);
    }
    pin_user_pointer(file, len);
    filesys_getlock();
    bool ret = filesys_create(file, initial_size);
    filesys_releaselock();
    unpin_user_pointer(file, len);

    return ret;
}
uint32_t sys_remove(struct intr_frame *f){
    char *file;
    bool success = argraw(1, f, &file);
    if (!success) {
        thread_exit_with_status(-1);
    }
    int len = check_str(file, f);
    if (len == -1) {
        thread_exit_with_status(-1);
    }

    pin_user_pointer(file, len);
    filesys_getlock();
    bool ret = filesys_remove(file);
    filesys_releaselock();
    unpin_user_pointer(file, len);
    return ret;
}
uint32_t sys_open(struct intr_frame *f){
    char *file;
    bool success = argraw(1, f, &file);
    if (!success) {
        thread_exit_with_status(-1);
    }
    int len = check_str(file, f);
    if (len == -1) {
        thread_exit_with_status(-1);
    }

    pin_user_pointer(file, len);
    filesys_getlock();
    struct file *fileptr = filesys_open(file);
    filesys_releaselock();
    unpin_user_pointer(file, len);

    if (fileptr == NULL) {
        // filesys_releaselock();
        return -1;
    }
    struct thread *t = thread_current();
    struct file_descriptor *fd = malloc(sizeof(struct file_descriptor));
    if (fd == NULL) {
        
        return -1;
    }
    fd->file = fileptr;
    fd->fd = t->next_fd;
    t->next_fd++;
    list_push_back(&t->file_list, &fd->elem);
    // filesys_releaselock();
    return fd->fd;
}



uint32_t sys_filesize(struct intr_frame *f){
    int fd;
    bool success = argraw(1, f, &fd);
    if (!success) {
        thread_exit_with_status(-1);
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }
    filesys_getlock();
    int ret = file_length(file->file);
    filesys_releaselock();
    return ret;
}
uint32_t sys_read(struct intr_frame *f){
    int fd;
    bool success = argraw(1, f, &fd);
    void *buffer;
    success = argraw(2, f, &buffer);
    if (!success) {
        thread_exit_with_status(-1);
    }
    unsigned size;
    success = argraw(3, f, &size);
    if (!success) {
        thread_exit_with_status(-1);
    }
    success = check_user_pointer(buffer, size, true, f);
    if (!success) {
        thread_exit_with_status(-1);
    }
    if (fd == 0) {
        for (int i = 0; i < size; i++) {
            ((uint8_t *)buffer)[i] = input_getc();
        }
        return size;
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }

    pin_user_pointer(buffer, size);
    filesys_getlock();
    int ret = file_read(file->file, buffer, size);
    filesys_releaselock();
    unpin_user_pointer(buffer, size);

    return ret;
}
uint32_t sys_seek(struct intr_frame *f){
    int fd;
    bool success = argraw(1, f, &fd);
    if (!success) {
        thread_exit_with_status(-1);
    }
    unsigned position;
    success = argraw(2, f, &position);
    if (!success) {
        thread_exit_with_status(-1);
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }
    filesys_getlock();
    file_seek(file->file, position);
    filesys_releaselock();
    return 0;
    
}
uint32_t sys_tell(struct intr_frame *f){
    int fd;
    bool success = argraw(1, f, &fd);
    if (!success) {
        thread_exit_with_status(-1);
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }
    filesys_getlock();
    int ret = file_tell(file->file);
    filesys_releaselock();
    return ret;
}

uint32_t sys_close(struct intr_frame *f){
    int fd;
    bool success = argraw(1, f, &fd);
    if (!success) {
        thread_exit_with_status(-1);
    }
    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL) {
        return -1;
    }
    filesys_getlock();
    file_close(file->file);
    filesys_releaselock();
    list_remove(&file->elem);
    free(file);
    return 0;
}
uint32_t sys_mmap(struct intr_frame *f){
    int fd;
    void *addr;
    bool success = argraw(1, f, &fd);
    if (!success) {
        thread_exit_with_status(-1);
    }
    success = argraw(2, f, &addr);
    if (!success) {
        thread_exit_with_status(-1);
    }

    if (fd == 1 || fd == 0 || addr == 0) {
        return -1;
    }

    struct file_descriptor *file = get_file_descriptor(fd);
    if (file == NULL || file_length(file->file) == 0) {
        return -1;
    }
    return do_mmap(addr, file_length(file->file), true, file->file, 0);
    
}
uint32_t sys_munmap(struct intr_frame *f) {
    void *addr;
    bool success = argraw(1, f, &addr);
    if (!success) {
        thread_exit_with_status(-1);
    }
    do_munmap(addr);
    return 0;
}
uint32_t sys_chdir(struct intr_frame *f){
    PANIC("sys_chdir");
}
uint32_t sys_mkdir(struct intr_frame *f){
    PANIC("sys_mkdir");
}
uint32_t sys_readdir(struct intr_frame *f){
    PANIC("sys_readdir");
}
uint32_t sys_isdir(struct intr_frame *f){
    PANIC("sys_isdir");
}
uint32_t sys_inumber(struct intr_frame *f){
    PANIC("sys_inumber");
}

static int 
check_str(char *puser, struct intr_frame *f) {
    int size = 0;
    while (check_user_pointer(puser, 1, false, f)) {
        size++;
        if (*puser == '\0') {
            return size;
        }
        puser++;
    }
    return -1;
}