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
#include <limits.h>

int sys_open(struct fharray *pfhs, userptr_t path, int flags, int* retval){

    int err = 0;

    char* file = kmalloc(__PATH_MAX);
    
    err = copyinstr(path,file,__PATH_MAX,NULL);
    if(err){
        kfree(file);
        return err;
    }

    err = _fh_open(pfhs,file,flags,retval);
    if(err){
        kfree(file);
        return err;
    }
    kfree(file);

    // the file descriptor is in retval
    return SUCC;
}


int sys_read(int fd, const void *buf, size_t nbytes, int* retval){

    KASSERT(buf != NULL);

    int ret;
    void* desc = kmalloc(1);
    ret = copyin((const_userptr_t)buf,desc,1);
    kfree(desc);
    if(ret != 0){
        return ret;
    }

    struct fh *handle = _get_fh(fd,&curproc->p_fhs);
    if(handle == NULL){
        return EBADF;
    }

    if(handle->flag && O_WRONLY){
        return EBADF;
    }

    return _fh_read(handle,buf,nbytes,retval);
}


int sys_write(int fd, const void *buf, size_t nbytes, int* retval){

    KASSERT(buf != NULL);

    int ret;
    void* desc = kmalloc(1);
    ret = copyin((const_userptr_t)buf,desc,1);
    kfree(desc);
    if(ret != 0){
        return ret;
    }

    struct fh *handle = _get_fh(fd,&curproc->p_fhs);
    if(handle == NULL){
        return EBADF;
    }

    if(handle->flag && O_RDONLY){
        return EBADF;
    }

    return _fh_write(handle,buf,nbytes,retval);
}

int sys_close(struct fharray *pfhs, int fd){

    // validate fd
    if(fd < 0 && fd >= MAX_FD){
        return EBADF;
    }

    _fhs_close(fd, pfhs);

    return SUCC;
}
