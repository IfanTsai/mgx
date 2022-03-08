#include "mgx_file.h"
#include <fstream>
#include <sstream>
#include <errno.h>

static ssize_t read_uninterrupt(int fd, char *buf, size_t size)
{
    for (;;) {
        ssize_t n = read(fd, buf, size);
        if (n < 0 && errno == EINTR)
            continue;

        return n;
    }
}

ssize_t must_read(int fd, char *buf, size_t size)
{
    size_t off = 0;

    while (off < size) {
        ssize_t read_size = read_uninterrupt(fd, buf + off, size - off);
        if (read_size <= 0)
            return read_size;

        off += read_size;
    }

    return off;
}

int read_file_all(const char *file_path, std::string &buf)
{
    std::ifstream is(file_path);
    if (!is.is_open())
        return -1;

    std::ostringstream ss;
    std::string tmp;
    while (std::getline(is, tmp))
        ss << tmp << std::endl;

    is.close();

    buf = ss.str();

    return 0;
}
