#include "mgx_io.h"
#include <sys/select.h>
#include <poll.h>

int mgx_poll(int fd, bool rd, bool wr, long timeout_ms)
{
    struct pollfd fds[1];

    fds[0].fd = fd;
    fds[0].events = POLLOUT;
    fds[0].revents = 0;

    if (rd)
        fds[0].events |= POLLIN;

    if (wr)
        fds[0].events |= POLLOUT;

    int rv = poll(fds, 1, timeout_ms);
    if (rv >= 0) {
        rv = 0;

        if (fds[0].revents & POLLIN)
            rv |= MGX_READ;

        if (fds[0].revents & POLLOUT)
            rv |= MGX_WRITE;
    }

    return rv;
}

int mgx_select(int fd, bool rd, bool wr, long timeout_ms)
{
    struct timeval tv = { 0 };
    tv.tv_usec = timeout_ms * 1000;

    fd_set *readfds = nullptr;
    fd_set *writefds = nullptr;

    if (rd) {
        readfds = new fd_set;
        FD_ZERO(readfds);
        FD_SET(fd, readfds);
    }

    if (wr) {
        writefds = new fd_set;
        FD_ZERO(writefds);
        FD_SET(fd, writefds);
    }

    int rv = ::select(fd + 1, readfds, writefds, nullptr, &tv);
    if (rv >= 0) {
        rv = 0;

        if (rd && FD_ISSET(fd, readfds))
            rv |= MGX_READ;
        
        if (wr && FD_ISSET(fd, writefds))
            rv |= MGX_WRITE;
    }

    delete(readfds);
    delete(writefds);

    return rv;
}