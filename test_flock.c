#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("Opening lock file...\n");
    int fd = open("test.lock", O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("open"); return 1; }
    
    printf("Parent locking...\n");
    if (flock(fd, LOCK_EX) == -1) { perror("flock"); return 1; }
    printf("Parent locked!\n");
    
    if (fork() == 0) {
        printf("Child unlocking...\n");
        if (flock(fd, LOCK_UN) == -1) { perror("flock un"); return 1; }
        printf("Child unlocked!\n");
        _exit(0);
    }
    
    wait(NULL);
    printf("Parent done.\n");
    return 0;
}
