#include <types.h>
#include <lib.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <uio.h>
#include <proc.h>
#include <smpfs.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/seek.h>
#include <kern/iovec.h>

static int _fh_allotfd(struct fharray *fhs);

int _fh_open(struct fharray *handlers, char* path, int flags, int* ret){

    int fd = _fh_allotfd(handlers);
    KASSERT(fd <= MAX_FD);
    if(fd == MAX_FD){
        *ret = EMFILE;
        return ERR;
    }
    
    /* W , R , RW */
	if ( 
		((flags & O_RDONLY) && (flags & O_WRONLY)) ||
		((flags & O_RDWR) && ((flags & O_RDONLY) || (flags & O_WRONLY)))
	) {
        *ret = EINVAL;
		return ERR;
	}

    struct fh *handle = kmalloc(sizeof(struct fh));
    if(handle == NULL){
        return EINVAL;
    }

    handle->filename = kstrdup(path);
    if(handle->filename == NULL){
        kfree(handle);
        return EINVAL;
    }

    struct lock *lk= lock_create(handle->filename);
    if(lk == NULL){
        kfree(handle->filename);
        kfree(handle);
        return EINVAL;
    }

    struct vnode **obj = kmalloc(sizeof(struct vnode *));
    if(obj == NULL){
        kfree(handle->filename);
        kfree(handle);
        lock_destroy(lk);
        return EINVAL;
    }

	*ret = vfs_open(path,flags,0,obj);
    if(*ret != 0){
        kfree(handle->filename);
        kfree(obj);
        lock_destroy(lk);
        kfree(handle);
        return *ret;
    }

    handle->flag = flags;
    handle->fh_seek = 0;
    handle->refs = 1;
    handle->fd = fd;
    handle->fh_lock = lk; 
    handle->fh_vnode = obj;

    if(flags & O_APPEND){
        struct stat filestats;
        VOP_STAT(*handle->fh_vnode, &filestats);
        handle->fh_seek = filestats.st_size;
    }
    
    fharray_set(handlers,fd,handle);
    KASSERT(fharray_get(handlers,fd) == handle);
    *ret = fd;

    return SUCC;
}


int _fh_read(struct fh* handle, const void* buf, size_t nbytes, int* ret){
    
    int errno;
    struct uio uio;
    struct iovec iov;
    iov.iov_ubase = (userptr_t)buf;
    iov.iov_len = nbytes;

    lock_acquire(handle->fh_lock);

    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = handle->fh_seek;
    uio.uio_resid = nbytes;
    uio.uio_segflg = UIO_USERSPACE;
    uio.uio_rw = UIO_READ;
    uio.uio_space = proc_getas();

    errno = VOP_READ(*handle->fh_vnode,&uio);

    if(errno == 0){
        *ret = nbytes - uio.uio_resid;
        handle->fh_seek = handle->fh_seek + *ret;
    }

    lock_release(handle->fh_lock);

    return errno;    
}

int _fh_write(struct fh* handle, const void* buf, size_t nbytes, int* ret){
    
    int errno;
    struct uio uio;
    struct iovec iov;
    iov.iov_ubase = (userptr_t)buf;
    iov.iov_len = nbytes;

    lock_acquire(handle->fh_lock);

    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = handle->fh_seek;
    uio.uio_resid = nbytes;
    uio.uio_segflg = UIO_USERSPACE;
    uio.uio_rw = UIO_WRITE;
    uio.uio_space = proc_getas();

    errno = VOP_WRITE(*handle->fh_vnode,&uio);

    if(errno == 0){
        *ret = nbytes - uio.uio_resid;
        handle->fh_seek = handle->fh_seek + *ret;
    }

    lock_release(handle->fh_lock);

    return errno;
}

/* Remove the file handle from the file handler table */
void _fhs_close(int fd, struct fharray *fhs){
    KASSERT(fhs != NULL);
    KASSERT(fd >= 0 && fd < MAX_FD);

    struct fh *handle = fharray_get(fhs,fd);

	if(handle == NULL){
		return;
	}

    lock_acquire(handle->fh_lock);

    /* Decrement the reference counter */
    handle->refs = handle->refs - 1;

    /* Close the handler if the refs drops to 0 */
    if(handle->refs == 0){
        lock_release(handle->fh_lock);
        vfs_close(*handle->fh_vnode);
        kfree(handle->fh_vnode);
        lock_destroy(handle->fh_lock);
        kfree(handle->filename);
        kfree(handle);
    }else{
        lock_release(handle->fh_lock);
    }

    fharray_set(fhs,fd,NULL);

    KASSERT(fharray_get(fhs,fd) == NULL);
}

int _fh_lseek(struct fh* handle, off_t pos, int whence, off_t* res){
    KASSERT(handle != NULL);

    lock_acquire(handle->fh_lock);
    
    bool seekable = VOP_ISSEEKABLE(*handle->fh_vnode);

    if(!seekable){
        return ESPIPE;
    }

    switch(whence){
        case SEEK_SET:
            *res = pos;
        break;
        
        case SEEK_END: ;
            /* Get end of file */
            struct stat filestats;
            VOP_STAT(*handle->fh_vnode, &filestats);
            *res = filestats.st_size + pos;
        break;
        
        case SEEK_CUR:
            *res = handle->fh_seek + pos;
        break;
        
        default:
        	return EINVAL;
    }

    if(*res < 0){
        lock_release(handle->fh_lock);
        return EINVAL;
    }else{
        handle->fh_seek = *res;
    }

    lock_release(handle->fh_lock);

    return SUCC;
}

int _fh_dup2(int oldfd, int newfd, struct fharray* fhs, int* retval){

    struct fh* oldhandle = _get_fh(oldfd, fhs);

    if(oldhandle == NULL){
        return EBADF;
    }

    if(newfd < 0 || newfd >= MAX_FD){
        return EBADF;
    }

    if(oldfd == newfd){
        *retval = oldfd;
        return SUCC;
    }

    struct fh* newhandle = _get_fh(newfd, fhs);

    if(newhandle != NULL){
        _fhs_close(newfd, fhs);
    }

    lock_acquire(oldhandle->fh_lock);

    oldhandle->refs = oldhandle->refs + 1;
    fharray_set(fhs,newfd,oldhandle);

    *retval = newfd;

    lock_release(oldhandle->fh_lock);

    return SUCC;
}

/* Bootstrap the file handler table by initializing it and adding console file handles */
int _fh_bootstrap(struct fharray *fhs){

    /* Initialize the file handle array of this process */
	fharray_init(fhs);

    /* String variables initialized for passage to vfs_open */
    char* console_inp = kstrdup(CONSOLE);
    char* console_out = kstrdup(console_inp);
    char* console_err = kstrdup(console_inp);

    /* Return variable */
    int ret = 0;

    /*************************** STDIN *****************************/
    struct fh *stdinfh = kmalloc(sizeof(struct fh));
    stdinfh->filename = kstrdup(console_inp);
    
    struct vnode **stdin = kmalloc(sizeof(struct vnode *));
	ret = vfs_open(console_inp,O_RDONLY,0,stdin);
    if(ret != 0){
        kfree(stdin);
        kfree(stdinfh);
        return ret;
    }
	kfree(console_inp);

    stdinfh->fh_lock = lock_create(stdinfh->filename);
    if(stdinfh->fh_lock == NULL){
        kfree(stdin);
        kfree(stdinfh);         
        return ERR;
    }

    stdinfh->flag = O_RDONLY;
    stdinfh->fh_seek = 0;
    stdinfh->refs = 1;
    stdinfh->fh_vnode = stdin;
    stdinfh->fd = STDIN_FILENO;

	fharray_add(fhs,stdinfh,NULL);

    /*************************** STDOUT *****************************/
    struct fh *stdoutfh = kmalloc(sizeof(struct fh));
    stdoutfh->filename = kstrdup(console_out);
    
    struct vnode **stdout = kmalloc(sizeof(struct vnode *));
	ret = vfs_open(console_out,O_WRONLY,0,stdout);
    if(ret != 0){
        kfree(stdout);
        kfree(stdoutfh);
        return ret;
    }
	kfree(console_out);

    stdoutfh->fh_lock = lock_create(stdoutfh->filename);
    if(stdoutfh->fh_lock == NULL){
        kfree(stdout);
        kfree(stdoutfh);         
        return ERR;
    }

    stdoutfh->flag = O_WRONLY;
    stdoutfh->fh_seek = 0;
    stdoutfh->refs = 1;
    stdoutfh->fh_vnode = stdout;
    stdoutfh->fd = STDOUT_FILENO;

	fharray_add(fhs,stdoutfh,NULL);

    /*************************** STDERR *****************************/
    struct fh *stderrfh = kmalloc(sizeof(struct fh));
    stderrfh->filename = kstrdup(console_err);
    
    struct vnode **stderr = kmalloc(sizeof(struct vnode *));
	ret = vfs_open(console_err,O_WRONLY,0,stderr);
    if(ret != 0){
        kfree(stderr);
        kfree(stderrfh);
        return ret;
    }
	kfree(console_err);

    stderrfh->fh_lock = lock_create(stderrfh->filename);
    if(stderrfh->fh_lock == NULL){
        kfree(stderr);
        kfree(stderrfh);         
        return ERR;
    }

    stderrfh->flag = O_WRONLY;
    stderrfh->fh_seek = 0;
    stderrfh->refs = 1;
    stderrfh->fh_vnode = stderr;
    stderrfh->fd = STDERR_FILENO;

	fharray_add(fhs,stderrfh,NULL);

    fharray_setsize(fhs,MAX_FD);	
	
	return 0;

	/* Initialization of stdin, out and err filehandlers complete */
}

struct fharray* _fhs_clone(struct fharray *fhs){
    if(fhs == NULL){
        return NULL;
    }

    struct fharray *newfhs = kmalloc(sizeof(struct fharray));
    if(newfhs == NULL){
        return NULL;
    }
    
    fharray_init(newfhs);
    int len = fharray_num(fhs);
    fharray_setsize(newfhs, len);

    int idx = 0;
    for(idx = 0;idx < len; idx++){
		struct fh * handle = fharray_get(fhs, idx);
		if(handle != NULL){
			lock_acquire(handle->fh_lock);
			fharray_set(newfhs, idx, fharray_get(fhs, idx));
			handle->refs = handle->refs + 1;
			lock_release(handle->fh_lock);
		}
    }

    return newfhs;
}

/* Cleanup the file handler array */
void _fhs_cleanup(struct fharray *pfhs){
    int idx = 0;
    for(idx = 0; idx < MAX_FD; idx++){
        _fhs_close(idx, pfhs);
    }

	while(fharray_num(pfhs) != 0){
		fharray_remove(pfhs, 0);
	}

}

struct fh * _get_fh(int fd, struct fharray* fhs){
    if(fd < 0 || fd >= MAX_FD){
        return NULL;
    }

    return fharray_get(fhs,fd);
}

static 
int _fh_allotfd(struct fharray *fhs){

    KASSERT(fhs != NULL);

    int idx = 3;
    for(idx = 3;idx < MAX_FD; idx++){
        if(fharray_get(fhs,idx) == NULL){
            break;
        }
    }

    // if MAX_FD is returned then fh table is full
    return idx;
}
