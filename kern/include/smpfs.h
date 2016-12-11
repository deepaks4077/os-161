#include <array.h>
#include <proc.h>
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
    struct vnode **fh_vnode; /* The file object of this file */
};

DECLARRAY(fh,static __UNUSED inline);
DEFARRAY(fh,static __UNUSED inline);

struct fh * _get_fh(int fd, struct fharray* fhs);
int _fh_write(struct fh* handle, const void *buf, size_t nbytes, int* ret);
struct fh * _fh_add(int flag, struct vnode *file, struct fharray *fhs, int* errno);
void _fhs_close(int fd, struct fharray *fhs);
int _fh_bootstrap(struct proc *process);
