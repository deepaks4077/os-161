#include <types.h>
#include <lib.h>
#include <smpfs.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <uio.h>
#include <proc.h>
#include <kern/iovec.h>

static int _fh_create(int flag, struct vnode **file, struct fh *handle);
static int _fh_allotfd(struct fharray *fhs);

int _fh_write(struct fh* handle, const void* buf, size_t nbytes, int* ret){
    
    int errno;
    struct uio uio;
    struct iovec iov;
    iov.iov_ubase = *(userptr_t *)buf;
    iov.iov_len = nbytes;

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

    return errno;
}


/* Create and add a file handle to the process file handler table */
struct fh *  _fh_add(int flag, struct vnode **file, struct fharray *fhs, int* errno){
    uint32_t fd = _fh_allotfd(fhs);
    if(fd == MAX_FD){
        *errno = EMFILE;
        return NULL;
    }
    KASSERT(fharray_get(fhs,fd) == NULL);

    struct fh *handle = (struct fh *)kmalloc(sizeof(struct fh *));
    int ret = _fh_create(flag,file,handle);
    if(ret != 0){
        *errno = ret;
        return NULL;
    }

    handle->fd = fd;
    fharray_set(fhs,fd,handle);

    return 0;
}

/* Remove the file handle from the file handler table */
void _fhs_close(int fd, struct fharray *fhs){
    KASSERT(fhs != NULL);
    KASSERT(fd >= 0 && fd < MAX_FD);
   
	/* Deallocate this piece of memmory since 
	file handles are created from the heap */
	kfree(fharray_get(fhs,fd)); 
    fharray_set(fhs, fd, NULL);
    KASSERT(fharray_get(fhs,fd) == NULL);
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

	/* Initialize the console files STDIN, STDOUT and STDERR */
	struct vnode **stdin;
	ret = vfs_open(console_inp,O_RDONLY,0,stdin);
    if(ret != 0){
        return ret;
    }
	kfree(console_inp);

	struct fh *stdinfh = kmalloc(sizeof(struct fh));
    ret =  _fh_create(O_RDONLY,stdin,stdinfh);
    if(ret != 0){
        return ret;
    }

	stdinfh->fd = STDIN_FILENO;
	fharray_add(fhs,stdinfh,NULL);

	struct vnode **stdout;
	ret = vfs_open(console_out,O_WRONLY,0,stdout);
	if(ret != 0){
        return ret;
    }
	kfree(console_out);

	struct fh *stdoutfh = kmalloc(sizeof(struct fh));
    ret =  _fh_create(O_WRONLY,stdout,stdoutfh);
    if(ret != 0){
        return ret;
    }

	stdoutfh->fd = STDOUT_FILENO;
	fharray_add(fhs,stdoutfh,NULL);

	struct vnode **stderr;
	ret = vfs_open(console_err,O_WRONLY,0,stderr);
	if(ret != 0){
        return ret;
    }
	kfree(console_err);

	struct fh *stderrfh = kmalloc(sizeof(struct fh));
    ret =  _fh_create(O_WRONLY,stderr,stderrfh);
    if(ret != 0){
        return ret;
    }

	stderrfh->fd = STDERR_FILENO;
	fharray_add(fhs,stderrfh,NULL);

    fharray_setsize(fhs,MAX_FD);	
	
	return 0;

	/* Initialization of stdin, out and err filehandlers complete */
}

struct fh * _get_fh(int fd, struct fharray* fhs){
    if(fd < 0 || fd > MAX_FD){
        return NULL;
    }

    return fharray_get(fhs,fd);
}

/* Create a file handle */
static int _fh_create(int flag, struct vnode **file, struct fh *handle){

    KASSERT(file != NULL);

    /* W , R , RW */
	if ( 
		((flag & O_RDONLY) && (flag & O_WRONLY)) ||
		((flag & O_RDWR) && ((flag & O_RDONLY) || (flag & O_WRONLY)))
	) {
        handle = NULL;
		return 1;
	}

    handle->flag = flag;
    handle->fh_seek = 0;
    handle->fh_vnode = file;

    return 0;
}

static 
int _fh_allotfd(struct fharray *fhs){
    int idx = 3;
    for(idx = 3;idx < MAX_FD; idx++){
        if(fharray_get(fhs,idx) == NULL){
            break;
        }
    }

    // if MAX_FD is returned then fh table is full
    return idx;
}
