#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int wait_for_user(void)
{
    printf("Press any key to continue...");
    return getchar();
}

int main(int argc, char **argv)
{
    int fd;
    int ret;
    char buf[4096];
    ssize_t len;

    if (argc != 2) {
        printf("usage: %s /path/to/file\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], 0);
    printf("=> Opened as fd %d\n", fd);

    wait_for_user();

    ret = unlink(argv[1]);
    printf("=> Called unlink(%s) = %d\n", argv[1], ret);

    printf("=> Running ls to show that hello.txt is unlinked:\n");
    system("ls /mnt/pantry");

    wait_for_user();

    len = read(fd, buf, sizeof(buf));
    if (len > 0) {
        printf("=> Read %d bytes:\n", len);
        fwrite(buf, 1, len, stdout);
    }

    wait_for_user();

    ret = close(fd);
    printf("=> close(fd) = %d\n", ret);

    return 0;
}
