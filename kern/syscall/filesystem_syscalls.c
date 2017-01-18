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
#include <vfs.h>
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

int sys__getcwd(userptr_t buf, size_t nbytes, int* retval){
    
    KASSERT(buf != NULL);

    int ret;
    void* desc = kmalloc(1);
    ret = copyout((const void *)desc,buf,1);
    kfree(desc);
    if(ret){
        return ret;
    }

    struct uio uio;
    struct iovec iov;
    iov.iov_ubase = (userptr_t)buf;
    iov.iov_len = nbytes;

    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = 0;
    uio.uio_resid = nbytes;
    uio.uio_segflg = UIO_USERSPACE;
    uio.uio_rw = UIO_READ;
    uio.uio_space = proc_getas();

    ret = vfs_getcwd(&uio);
    if(ret){
        return ret;
    }

    *retval = (int)(nbytes - uio.uio_resid);
    
    return SUCC;
}

int sys_lseek(int fd, off_t pos, int whence, off_t* retval){
    
    struct fh *handle = _get_fh(fd,&curproc->p_fhs);
    if(handle == NULL){
        return EBADF;
    }

    return _fh_lseek(handle,pos,whence,retval);
}

int sys_dup2(int oldfd, int newfd, int* retval){
    return _fh_dup2(oldfd, newfd, retval);
}
