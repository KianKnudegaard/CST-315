#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define CHUNK_SIZE 4096
#define SIGNATURE ".test_virus_active"

void scan_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return;

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, CHUNK_SIZE)) > 0) {
        if (memmem(buffer, bytes_read, SIGNATURE, strlen(SIGNATURE))) {
            printf("Warning: file %s is infected!\n", path);
            close(fd);
            return;
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        exit(1);
    }

    DIR *dir = opendir(argv[1]);
    if (!dir) {
        perror("opendir");
        exit(1);
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], entry->d_name);
        scan_file(path);
    }
    closedir(dir);
    return 0;
}