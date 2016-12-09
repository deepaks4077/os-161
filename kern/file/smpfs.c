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

static struct fh * _fh_create(int flag, struct vnode *file);
static int _fh_allotfd(struct fharray *fhs);
static struct fh * _get_fh(int fd, struct fharray* fhs);

int _fh_write(struct fh* handle, void* buf, size_t nbytes, int* ret){
    
    int errno;
    struct uio* uio;
    struct iovec* iov;
    iov->iov_base = buf;
    iov->iov_len = ssize;

    uio->uio_iov = iov;
    uio->uio_iovcnt = 1;
    uio->uio_offset = handle->fh_seek;
    uio->uio_resid = nbytes;
    uio->uio_segflag = UIO_USERSPACE;
    uio->uio_rw = UIO_WRITE;
    uio->uio_space = proc_getas();

    errno = VOP_WRITE(*handle->fh_vnode,uio);

    if(errno == 0){
        *ret = nbytes - uio->uio_resid;
        handle->fh_seek = handle->fh_seek + *ret;
    }

    return errno;
}


/* Create and add a file handle to the process file handler table */
struct *fh  _fh_add(int flag, struct vnode *file, struct fharray *fhs, int* errno){
    uint32_t fd = _fh_allotfd(fhs);
    if(fd == MAX_FD){
        *errno = EMFILE;
        return NULL;
    }
    KASSERT(fharray_get(fhs,fd) == NULL);

    struct fh* handle = _fh_create(flag,file);
    if(handle == NULL){
        *errno = ENOMEM;
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
    
    fharray_set(fhs, fd, NULL);
    KASSERT(fharray_get(fhs,fd) == NULL);
}

/* Bootstrap the file handler table by initializing it and adding console file handles */
int _fh_bootstrap(struct proc *process){

    DEBUG(DB_VFS, "Bootstrapping for process : %s\n", process->p_name);

    /* Initialize the file handle array of this process */
	fharray_init(process->p_fhs);
    fharray_setsize(process->p_fhs,MAX_FD);

    /* String variables initialized for passage to vfs_open */
    char* console_inp = "con:";
    char* console_out = kstrdup(console_inp);
    char* console_err = kstrdup(console_inp);

    /* Return variable */
    int ret = 0;

	/* Initialize the console files STDIN, STDOUT and STDERR */
	struct vnode *stdin;
	ret = vfs_open(console_inp,O_RDONLY,0,stdin);
    if(ret != 0){
        return ret;
    }
	
    struct fh *stdinfh = _fh_create(O_RDONLY,stdin);
	stdinfh->fd = STDIN_FILENO;
    fharray_add(process->p_fhs,stdinfh);

	struct vnode *stdout;
	ret = vfs_open(console_out,O_WRONLY,0,stdout);
	if(ret != 0){
        return ret;
    }

    struct fh *stdoutfh = _fh_create(O_WRONLY,stdout);
	stdoutfh->fd = STDOUT_FILENO;
    fharray_add(process->p_fhs,stdoutfh);

	struct vnode *stderr;
	ret = vfs_open(console_err,O_WRONLY,0,stderr);
	if(ret != 0){
        return ret;
    }
    struct fh *stderrfh = _fh_create(O_WRONLY,stderr);
	stderrfh->fd = STDERR_FILENO;
	fharray_add(process->p_fhs,stderrfh);

	/* Initialization of stdin, out and err filehandlers complete */
}

static
struct fh* _get_fh(int fd, struct fharray* fhs){
    if(fd < 0 || fd > MAX_FD){
        return NULL;
    }

    return fharray_get(curproc->p_fhs,fd);
}

/* Create a file handle */
static
struct fh * _fh_create(int flag, struct vnode *file, int* errno){

    KASSERT(file != NULL);
    struct fh *handle;

    /* W , R , RW */
	if ( 
		((flag & O_RDONLY) && (flag & O_WRONLY)) ||
		((flag & O_RDWR) && ((flag & O_RDONLY) || (flag & O_WRONLY)))
	) {
		*errno  = EINVAL;
		return NULL;
	}

    handle->flag = flag;
    handle->fh_seek = 0;
    handle->fh_vnode = file;

    return handle;
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