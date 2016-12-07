#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <smpfs.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <spinlock.h>

struct *fh _getfh(int fd, struct fharray fhs){
    int idx = 0;
    int num = fharray_num(&fhs);

	while(idx < num){
		struct fh *handle = fharray_get(&fhs,idx);
		if(handle != NULL){
			if(handle->fd == fd){
				return handle;
			}
		}

		idx++;
	}

    return NULL;
}

int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *ssize){
    // check if fd exists in the process filesystem table
    // check if buf is an invalid pointer
    // is there free space to write to?
    // is the mode correct? = fd does not exist
    // hardware error

    // steps: 
        // get the file handle for this fd from the process
        // is the mode correct?
        // get the vnode of the file
        // write to the file

    KASSERT(buf !== NULL);

    struct fh *handle = fharray_get(curproc->p_fhs,fd);
    if(handle == NULL || handle->fh_mode == READ){
        return EBADF;
    }

    int ret;
    void* desc = kmalloc(1);
    ret = copyin((const_userptr_t)buf,desc,1);
    if(ret == EFAULT){
        //invalid status pointer
        *retval = EINVAL;
        ret = 1;
    }

    // check if there is free space on the disk to write to
    

    // create uio and send it to 

    return 0;

}