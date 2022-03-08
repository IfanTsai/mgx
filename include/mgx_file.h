#ifndef __MGX_FILE_H__
#define __MGX_FILE_H__

#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

ssize_t must_read(int fd, char *buf, size_t size);
int read_file_all(const char *file_path, std::string &buf);

static inline ssize_t get_file_size(int fd)
{
    struct stat file_info;
    if (fstat(fd, &file_info) < 0)
        return -1;

    return file_info.st_size;
}

#endif // !__MGX_FILE_H__