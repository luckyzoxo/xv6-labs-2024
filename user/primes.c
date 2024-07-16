#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void trans_data(int lpipe[2], int rpipe[2], int first)
{
    int data;
    // read data from left pipe
    while (read(lpipe[0], &data, sizeof(int)) == sizeof(int))
    {
        // transmit indivisible data to right pipe
        if (data % first)
            write(rpipe[1], &data, sizeof(int));
    }
    close(lpipe[0]), close(rpipe[1]);
}

// recursive search for primes
void primes(int lpipe[2])
{
    close(lpipe[1]);
    int first;
    if (read(lpipe[0], &first, sizeof(int)) != sizeof(int))
        exit(0);
    printf("prime %d\n", first);
    int p[2]; // current pipe
    pipe(p);
    trans_data(lpipe, p, first);
    // do recursively
    if (fork() == 0)
        primes(p);
    else
    {
        close(p[0]);
        wait(0);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    // initial data
    for (int i = 2; i <= 35; i++)
        write(p[1], &i, sizeof(int));

    // child process
    if (fork() == 0)
        primes(p);
    // parent process
    else
    {
        close(p[0]);
        close(p[1]);
        wait(0);
    }
    exit(0);
}