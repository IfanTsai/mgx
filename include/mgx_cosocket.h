#ifndef __MGX_COSOCKET_H__
#define __MGX_COSOCKET_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mgx_coroutine.h"

class Mgx_cosocket {
private:
    int m_sockfd;
    Mgx_coroutine_scheduler *m_sch;

    int set_socket_opt(int sockfd);
public:
    Mgx_cosocket();
    int socket(int domain, int type, int protocol);
    int close(int sockfd);
    int close();
    /* server */
    int bind(struct sockaddr *my_addr, socklen_t addrlen);
    int listen(int backlog);
    int accept(struct sockaddr *addr, socklen_t *addrlen);
    ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    ssize_t send(int sockfd, const void *buf, size_t len, int flags);
    /* client */
    int connect(const struct sockaddr *addr, socklen_t addrlen, unsigned long timeout = 0);
    ssize_t recv(void *buf, size_t len, int flags);
    ssize_t send(const void *buf, size_t len, int flags);
};

#endif
