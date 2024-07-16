#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

int match(char *path, char *filename)
{
    char *p;
    // find the first character after the last '/'
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    if (strcmp(p, filename) == 0)
        return 1;
    else
        return 0;
}

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // cannot open
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // cannot describe
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_DEVICE:
    case T_FILE:
        if (match(path, filename))
            printf("%s\n", path);
        break;
    case T_DIR:
        // path too long
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            fprintf(2, "find: path too long\n");
            return;
        }
        strcpy(buf, path);
        // add '/' after path
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof de) == sizeof de)
        {
            if (de.inum == 0)
                continue;
            // avoid to recurse into "." and ".."
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            find(buf, filename);
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3) // invalid argument
    {
        printf("error! usage: find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}