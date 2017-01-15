#ifndef _FILEHANDLER_H_
#define _FILEHANDLER_H_

#include <array.h>
#include <vnode.h>
#include <kern/fcntl.h>

#define CONSOLE "con:"
#define MAX_FD 1000

struct fh {
    uint32_t fd; /* Copy of the file descriptor integer,
            same as the position of the file handle 
            structure inside the file handle array */
    int flag; /* Mode this file handle has been opened in */
    off_t fh_seek; /* Current seek position inside the file */
    int refs; /* Reference counter of references to this file handle */
    
    struct lock *fh_lock; /* file handler lock */
    struct vnode **fh_vnode; /* The file object of this file */

    char* filename;
};

DECLARRAY(fh,static __UNUSED inline);
DEFARRAY(fh,static __UNUSED inline);

struct fh * _get_fh(int fd, struct fharray* fhs);
int _fh_open(struct fharray *handlers, char* path, int flags, int* ret);
int _fh_write(struct fh* handle, const void *buf, size_t nbytes, int* ret);
int _fh_read(struct fh* handle, const void *buf, size_t nbytes, int* ret);
void _fhs_close(int fd, struct fharray *fhs);
int _fh_bootstrap(struct fharray *fhs);

#endif /*_FILEHANDLER_H_*/
