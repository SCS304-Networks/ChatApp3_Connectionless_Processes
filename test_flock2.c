#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("Starting...\n");
    int fd = open("test.lock", O_CREAT | O_RDWR, 0666);
    if(flock(fd, LOCK_EX) == -1) perror("lock parent");
    
    pid_t p = fork();
    if (p == 0) {
        printf("Child unlocking...\n");
        if(flock(fd, LOCK_UN) == -1) perror("unlock child");
        printf("Child done.\n");
        _exit(0);
    } else if (p > 0) {
        wait(NULL);
        printf("Parent done.\n");
    } else {
        perror("fork");
    }
    return 0;
}
