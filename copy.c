#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd_src, fd_dst;
    char *buffer;
    size_t bufferSize;
    ssize_t bytesRead;

    if (argc != 4) {
        fprintf(stderr, "not enough arguments\n");
        exit(1);
    }

    bufferSize = (size_t)sscanf(argv[3],"%d",&n);

    buffer = malloc(bufferSize);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    fd_src = open(argv[1], O_RDONLY);
    if (fd_src < 0) {
        perror("open");
        free(buffer);
        exit(1);
    }

    fd_dst = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd_dst < 0) {
        perror("open");
        close(fd_src);
        free(buffer);
        exit(1);
    }

    while (bytesRead = read(fd_src, buffer, bufferSize), bytesRead > 0) {
        if (write(fd_dst, buffer, bytesRead) != bytesRead) {
            perror("write");
            close(fd_src);
            close(fd_dst);
            free(buffer);
            exit(1);
        }
    }

    if (bytesRead < 0) {
        perror("read");
        close(fd_src);
        close(fd_dst);
        free(buffer);
        exit(1);
    }

    if (close(fd_src) < 0) {
        perror("close source");
        close(fd_dst);
        free(buffer);
        exit(1);
    }
    if (close(fd_dst) < 0) {
        perror("close destination");
        free(buffer);
        exit(1);
    }

    free(buffer);
    return 0;
}
