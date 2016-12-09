#include <array.h>
#include <vnode.h>
#include <kern/fcntl.h>

#define CONSOLE "con:"
#define READ O_RDONLY
#define WRITE O_WRONLY
#define RDWR O_RDWR
#define MAX_FD 1000

/* Modes a file handle can be opened in */
typedef enum {
    READ,	/* Read-only an arbitray location inside the file */
	WRITE,	/* Write only to an arbitrary location inside the file */
	RDWR	/* Read/Write to an arbitrary location inside the file */
} fhmode_t;

struct fh {
    uint32_t fd; /* Copy of the file descriptor integer,
            same as the position of the file handle 
            structure inside the file handle array */
    int flag; /* Mode this file handle has been opened in */
    off_t fh_seek; /* Current seek position inside the file */
    struct vnode **fh_vnode; /* The file object of this file */
}

DECLARRAY(fh,static __UNUSED inline);
DEFARRAY(fh,static __UNUSED inline);

int _fh_write(struct fh* handle, const void *buf, size_t nbytes, ssize_t ssize, int* ret);
struct *fh _fh_add(int flag, struct vnode *file, struct fharray *fhs, int* errno);
void _fhs_close(int fd, struct fharray *fhs);
int _fh_bootstrap(struct proc* process);