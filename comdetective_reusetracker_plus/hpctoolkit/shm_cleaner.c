#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ipc.h>


static const char * shared_key = "/FALSE_SHARING_KEY";
int main() {
    int fd ;
    if ( (fd = shm_open(shared_key, O_RDWR | O_CREAT, 0666)) < 0 ) {
        printf("Failed to shm_open (%s), retval = %d", shared_key, fd);
        exit(-1);
    }
    if ( ftruncate(fd, /*1MB*/ 1L<<20) < 0 ) {
        printf("Failed to ftruncate()");
        exit(-1);
    }
    void * ptr = mmap(NULL,  /*1MB*/ 1L<<20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if(ptr == MAP_FAILED ) {
        printf("Failed to mmap() IPC_SharedData");
        exit(-1);
    }
    memset(ptr, 0, 1L<<20);
    shm_unlink((char *)shared_key);
    return 0;
}
