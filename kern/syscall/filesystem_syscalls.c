#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <smpfs.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <uio.h>
#include <spinlock.h>

int sys_read(int fd, const void *buf, size_t nbytes, int* errno){

    KASSERT(buf != NULL);

    int ret;
    void* desc = kmalloc(1);
    ret = copyin((const_userptr_t)buf,desc,1);
    kfree(desc);
    if(ret != 0){
        *errno = EFAULT;
        return ret;
    }

    struct fh *handle = _get_fh(fd,&curproc->p_fhs);
    if(handle == NULL){
        *errno = EBADF;
        return 1;
    }

    if(handle->flag && O_WRONLY){
        *errno = EBADF;
        return 1;
    }

    return _fh_read(handle,buf,nbytes,errno);
}


int sys_write(int fd, const void *buf, size_t nbytes, int* errno){

    KASSERT(buf != NULL);

    int ret;
    void* desc = kmalloc(1);
    ret = copyin((const_userptr_t)buf,desc,1);
    kfree(desc);
    if(ret != 0){
        *errno = EFAULT;
        return ret;
    }

    struct fh *handle = _get_fh(fd,&curproc->p_fhs);
    if(handle == NULL){
        *errno = EBADF;
        return 1;
    }

    if(handle->flag && O_RDONLY){
        *errno = EBADF;
        return 1;
    }

    return _fh_write(handle,buf,nbytes,errno);
}
