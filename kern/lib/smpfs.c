#include <types.h>
#include <lib.h>
#include <smpfs.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/unistd.h>
#include <kern/errno.h>

static struct fh * _fh_create(uint32_t flag, struct vnode *file);
static int _fh_allotfd(struct fharray *fhs);

/* Create and add a file handle to the process file handler table */
int _fhs_add(uint32_t flag, struct vnode *file, struct fharray *fhs){
    uint32_t fd = _fh_allotfd(fhs);
    if(fd == MAX_FD){
        return EMFILE;
    }
    KASSERT(fharray_get(fhs,fd) == NULL);

    struct fh* handle = _fh_create(flag,file);
    if(handle == NULL){
        return ENOMEM;
    }

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
int _fh_bootstrap(struct proc* process){

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

/* Create a file handle */
static
struct fh * _fh_create(uint32_t flag, struct vnode *file){
    struct fh *handle;

    switch(flag){
        case O_RDONLY:
            handle->fh_mode = READ;
            break;
        case O_WRONLY:
            handle->fh_mode = WRITE;
            break;
        case O_RDWR:
            handle->fh_mode = RDWR;
            break;
        default:
            return NULL;
    }

    handle->fh_seek = 0;

    KASSERT(file != NULL);
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