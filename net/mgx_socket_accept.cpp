#include "mgx_socket.h"
#include "mgx_log.h"
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <unistd.h>

void Mgx_socket::event_accept(pmgx_conn_t pconn_listen)
{
    static int use_accept4 = 1;
    int conn_fd;
    struct sockaddr cliaddr = { 0 };
    socklen_t addrlen  = { 0 };

retry:
    if (use_accept4) {
        conn_fd = accept4(pconn_listen->fd, &cliaddr, &addrlen, SOCK_NONBLOCK);
    } else {
        conn_fd = accept(pconn_listen->fd, &cliaddr, &addrlen);
    }

    if (conn_fd < 0) {
        if (errno == EAGAIN) {
            mgx_log(MGX_LOG_WARN, "accept error: %s", strerror(errno));
            return;
        }
        int log_level = MGX_LOG_INFO;
        if (errno == ECONNABORTED) {
            log_level = MGX_LOG_ERR;
        } else if (errno == EMFILE || errno == ENFILE) {
            log_level = MGX_LOG_CRIT;
        }
        mgx_log(log_level, "accept error: %s", strerror(errno));
        if (use_accept4 && errno == ENOSYS) { /* don't suuport accept4 in current OS */
            use_accept4 = 0;
            goto retry;
        }
        return;
    }
    mgx_log(MGX_LOG_DEBUG, "conn_fd = %d", conn_fd);
    pmgx_conn_t pconn_new = get_conn(conn_fd);
    if (!pconn_new) {
        close(conn_fd);
        mgx_log(MGX_LOG_ALERT, "connection pool is full");
        return;
    }

    memcpy(&pconn_new->conn_sockaddr, &cliaddr, addrlen);

    if (!use_accept4)
        set_nonblock(conn_fd);

    pconn_new->r_handler = std::bind(&Mgx_socket::read_request_handler, this, std::placeholders::_1);
    pconn_new->w_handler = std::bind(&Mgx_socket::send_msg_handler, this, std::placeholders::_1);

    if (!epoll_oper_event(conn_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP,
                            EPOLL_ES_MOD_ACTION::IGNORE, pconn_new)) {
        close_conn(pconn_new);
        return;
    }

#ifndef USE_HTTP
    if (m_enabled_heartbeat) {
        add_to_timer_queue(pconn_new);
        mgx_log(MGX_LOG_DEBUG, "add_to_timer_queue called");
    }
#endif
}

