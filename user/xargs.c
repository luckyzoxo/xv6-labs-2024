#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
#define MAXLEN 512

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "Error: too less parameters for xargs!\n");
        exit(1);
    }

    char buf[MAXLEN];
    char *p = buf;
    char *x_argv[MAXARG];
    int argv_len = 0;
    for (int i = 1; i < argc; i++)
        x_argv[i - 1] = argv[i];
    int rsz = sizeof(char); // read size
    while (rsz == sizeof(char))
    {
        int start = 0, end = 0;
        argv_len = argc - 1;
        while (1)
        {
            rsz = read(0, p, sizeof(char));
            if (++end >= MAXLEN)
            {
                fprintf(2, "Error: arguments too long!\n");
                exit(1);
            }
            if (*p == ' ' || *p == '\n' || rsz != sizeof(char))
            {
                *p = '\0';
                x_argv[argv_len++] = &buf[start];
                start = end;
                if (argv_len >= MAXARG)
                {
                    fprintf(2, "Error: too many arguments for xargs!\n");
                    exit(1);
                }
                if (*p == '\n' || rsz != sizeof(char))
                    break;
            }
            p++;
        }
        if (fork() == 0)
            exec(argv[1], x_argv);
        else
            wait(0);
    }
    exit(0);
}