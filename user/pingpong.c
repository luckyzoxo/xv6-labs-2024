#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd[2];

    // error condition
    if (pipe(fd) < 0)
    {
        fprintf(2, "Error: pipe(fd) error.\n");
        exit(1);
    }

    // child process
    if (fork() == 0)
    {
        char buf[1];
        read(fd[0], buf, 1);
        close(fd[0]);
        fprintf(0, "%d: received ping\n", getpid());
        write(fd[1], buf, 1);
    }
    // parent process
    else
    {
        char buf[1];
        buf[0] = 'A';
        write(fd[1], buf, 1);
        close(fd[1]);
        read(fd[0], buf, 1);
        fprintf(0, "%d: received pong\n", getpid());
    }
    exit(0);
}