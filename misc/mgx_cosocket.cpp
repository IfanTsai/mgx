#include "mgx_cosocket.h"
#include "mgx_comm.h"
#include "mgx_io.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

Mgx_cosocket::Mgx_cosocket()
{
    m_sch = Mgx_coroutine_scheduler::get_instance();
}

int Mgx_cosocket::set_socket_opt(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL);
    int ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0) {
        ::close(sockfd);
        return ret;
    }

    int opt = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    if (ret < 0)
        ::close(sockfd);

    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
        ::close(sockfd);

    return ret;
}

int Mgx_cosocket::socket(int domain, int type, int protocol)
{
    m_sockfd = ::socket(domain, type, protocol);
    if (m_sockfd < 0)
        return m_sockfd;

    return set_socket_opt(m_sockfd);
}

int Mgx_cosocket::close()
{
    return ::close(m_sockfd);
}

int Mgx_cosocket::close(int sockfd)
{
    return ::close(sockfd);
}

int Mgx_cosocket::bind(struct sockaddr *my_addr, socklen_t addrlen)
{
    return ::bind(m_sockfd, my_addr, addrlen);
}

int Mgx_cosocket::listen(int backlog)
{
    return ::listen(m_sockfd, backlog);
}

int Mgx_cosocket::accept(struct sockaddr *addr, socklen_t *addrlen)
{
    Mgx_coroutine *co = m_sch->get_current_coroutine();
    int sockfd = -1;
    for (;;) {
        sockfd = ::accept(m_sockfd, addr, addrlen);
        if (sockfd >= 0)
            break;
        if (EAGAIN == errno) {
            co->set_wait_fd(m_sockfd);
            m_sch->add_event_wait_epoll(co, EPOLLIN);
            co->yield(false);
            m_sch->remove_event_wait_epoll(co);
        } else {
            return sockfd;
        }
    }
    int ret = set_socket_opt(sockfd);
    if (ret < 0)
        return ret;
    return sockfd;
}

ssize_t Mgx_cosocket::recv(int sockfd, void *buf, size_t len, int flags)
{
    Mgx_coroutine *co = m_sch->get_current_coroutine();

    ssize_t ret = -1;
    for (;;) {
        ret = ::recv(sockfd, buf, len, flags);
        if (ret < 0) {
            if (EAGAIN == errno) {
                co->set_wait_fd(sockfd);
                m_sch->add_event_wait_epoll(co, EPOLLIN);
                co->yield(false);
                m_sch->remove_event_wait_epoll(co);
            } else {
                return ret;
            }
        } else {
            break;
        }
    }

    return ret;
}

ssize_t Mgx_cosocket::send(int sockfd, const void *buf, size_t len, int flags)
{
    Mgx_coroutine *co = m_sch->get_current_coroutine();

    ssize_t ret = -1;
    for (;;) {
        ret = ::send(sockfd, buf, len, flags);
        if (ret > 0)
            break;
        if (EAGAIN == errno) {
            co->set_wait_fd(sockfd);
            m_sch->add_event_wait_epoll(co, EPOLLOUT);
            co->yield(false);
            m_sch->remove_event_wait_epoll(co);
        } else {
            return ret;
        }
    }

    return ret;
}

//#define CONNECT_TIMEOUT_SELECT
//#define CONNECT_TIMEOUT_POLL

int Mgx_cosocket::connect(const struct sockaddr *addr, socklen_t addrlen, unsigned long timeout)
{
    int ret = ::connect(m_sockfd, addr, addrlen);
    /*
     * when call connect() on a non-blocking socket,
     * get EINPROGRESS instead of EAGIN before connection handshake completed.
     */
    if (EINPROGRESS != errno)
        return ret;

    Mgx_coroutine *co = m_sch->get_current_coroutine();
    if (timeout == 0) {
        co->set_wait_fd(m_sockfd);
        m_sch->add_event_wait_epoll(co, EPOLLOUT);
        co->yield(false);
        m_sch->remove_event_wait_epoll(co);
    } else {
#if defined(CONNECT_TIMEOUT_SELECT)
        ret = mgx_select(m_sockfd, false, true, timeout);
#elif defined(CONNECT_TIMEOUT_POLL)
        ret = mgx_poll(m_sockfd, false, true, timeout);
#else
        co->msleep(timeout);
        ret = mgx_select(m_sockfd, false, true, 0);
#endif
        if (ret == 0)
            goto _timeout;
        else if (ret < 0)
            return ret;
    }

    ::getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &ret, (socklen_t *)&addrlen);
    if (ret == 0)
        return 0;

_timeout:
    errno = ETIMEDOUT;

    return -1;
}

ssize_t Mgx_cosocket::recv(void *buf, size_t len, int flags)
{
    return recv(m_sockfd, buf, len, flags);
}

ssize_t Mgx_cosocket::send(const void *buf, size_t len, int flags)
{
    return send(m_sockfd, buf, len, flags);
}
