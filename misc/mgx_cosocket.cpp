#include "mgx_cosocket.h"

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

int Mgx_cosocket::connect(const struct sockaddr *addr, socklen_t addrlen)
{
    return ::connect(m_sockfd, addr, addrlen);
}

ssize_t Mgx_cosocket::recv(void *buf, size_t len, int flags)
{
    return recv(m_sockfd, buf, len, flags);
}

ssize_t Mgx_cosocket::send(const void *buf, size_t len, int flags)
{
    return send(m_sockfd, buf, len, flags);
}
