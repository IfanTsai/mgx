#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include "mgx_comm.h"
#include "mgx_log.h"
#include "mgx_socket.h"
#include "mgx_mutex.h"
#include "mgx_thread_pool.h"

extern Mgx_th_pool g_mgx_th_pool;

void Mgx_socket::_read_request_handler(pmgx_conn_t c)
{
    ssize_t recv_size = recv_process(c, c->precv_buf, c->rest_recv_size);
    if (recv_size <= 0)
        return;

    switch (c->recv_stat) {
        case PKG_HDR_START:
            if (recv_size == m_pkg_hdr_size) {
                /*
                 * Once received a complete package header,
                 * so with the processing package header
                 */
                read_request_handler_process_v1(c);
            } else {
                c->recv_stat = PKG_HDR_RECEIVING;  /* next status */
                c->precv_buf += recv_size;
                c->rest_recv_size -= recv_size;
            }
            break;
        case PKG_HDR_RECEIVING:
            if (recv_size == c->rest_recv_size) {
                /*
                 * The packet header has been received,
                 * so with the processing package header
                 */
                read_request_handler_process_v1(c);
            } else {
                c->precv_buf += recv_size;
                c->rest_recv_size -= recv_size;
            }
            break;
        case PKG_BODY_START:
            if (recv_size == c->rest_recv_size) {
                /*
                 * The package header and body are all received,
                 * just process this package
                 */
                read_request_handler_process_v2(c);
            } else {
                c->recv_stat = PKG_BODY_RECEIVING;  /* next status */
                c->precv_buf += recv_size;
                c->rest_recv_size -= recv_size;
            }
            break;
        case PKG_BODY_RECEIVING:
            if (recv_size == c->rest_recv_size) {
                /*
                 * The package header and body are all received,
                 * just process this package
                 */
                read_request_handler_process_v2(c);
            } else {
                c->precv_buf += recv_size;
                c->rest_recv_size -= recv_size;
            }
            break;
        default:
            break;
    }
}

void Mgx_socket::read_request_handler(pmgx_conn_t c)
{
    _read_request_handler(c);
}

ssize_t Mgx_socket::recv_process(pmgx_conn_t c, char *buf, ssize_t buf_size)
{
    ssize_t n = recv(c->fd, buf, buf_size, 0);
    if (n == 0) { /* a socket has performed an orderly shutdown */
        insert_recy_conn_queue(c);
        return -1;
    } else if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            mgx_log(MGX_LOG_STDERR, "recv error: %s", strerror(errno));
            return -1;
        } else if (errno == EINTR) {
            mgx_log(MGX_LOG_STDERR, "recv error: %s", strerror(errno));
            return -1;
        } else if (errno == ECONNRESET) {
            // connection reset by peer
        } else {
            mgx_log(MGX_LOG_STDERR, "recv error: %s", strerror(errno));
        }
        close_conn(c);
        return -1;
    }

    return n;
}

void Mgx_socket::read_request_handler_process_v1(pmgx_conn_t c)
{
    pmgx_pkg_hdr_t ppkg_hdr = (pmgx_pkg_hdr_t)c->hdr_buf;

    unsigned short pkg_size = ppkg_hdr->pkg_size;
    if (pkg_size < m_pkg_hdr_size || pkg_size > (PKG_MAX_SIZE - 1000)) {
        /**
         * The total length is less than the head length or the total length is too long,
         * I think it is a wrong package.
         * So the receiving state should be restored to the initial state.
         */
        c->recv_stat = PKG_HDR_START;
        c->precv_buf = c->hdr_buf;
        c->rest_recv_size = m_pkg_hdr_size;
        mgx_log(MGX_LOG_STDERR, "received wrong package");
    }  else {
        char *tmp_buf = new char[m_msg_hdr_size + pkg_size];
        c->precv_mem_addr = tmp_buf;

        pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)tmp_buf;
        pmsg_hdr->pconn = c;
        pmsg_hdr->cur_seq = c->m_cur_seq;
        tmp_buf += m_msg_hdr_size;
        memcpy(tmp_buf, ppkg_hdr, m_pkg_hdr_size);
        if (pkg_size == m_pkg_hdr_size) {
            /* no package body */
            read_request_handler_process_v2(c);
        } else {
            c->recv_stat = PKG_BODY_START;  /* next status */
            c->precv_buf = tmp_buf + m_pkg_hdr_size;
            c->rest_recv_size = pkg_size - m_pkg_hdr_size;
        }
    }
}

void Mgx_socket::read_request_handler_process_v2(pmgx_conn_t c)
{
    /* let the thread pool handle the received message */
    g_mgx_th_pool.insert_msg_to_queue_and_signal(c->precv_mem_addr);

    /* restore initial state */
    c->precv_mem_addr = nullptr;
    c->recv_stat = PKG_HDR_START;
    c->precv_buf = c->hdr_buf;
    c->rest_recv_size = m_pkg_hdr_size;
}

void Mgx_socket::th_msg_process_func(char *buf)
{
    /* Involving business logic, let subprocesses do it */
}

void Mgx_socket::send_msg_handler(pmgx_conn_t c)
{
    ssize_t send_size = send_uninterrupt(c, c->psend_mem_addr, c->rest_send_size);
    if (send_size > 0) {
        if (send_size == c->rest_send_size) {
            if (!epoll_oper_event(c->fd, EPOLL_CTL_MOD, EPOLLOUT,
                                    EPOLL_ES_MOD_ACTION::REMOVE, c)) {
                mgx_log(MGX_LOG_STDERR, "epoll_oper_event EPOLL_CTL_MOD error: %s", strerror(errno));
            }
            mgx_log(MGX_LOG_DEBUG, "send over by epoll !!!");
        } else {
            c->psend_buf += send_size;
            c->rest_send_size -= send_size;
            return;
        }
    } else if (send_size == -1) {
        /* impossible */
        mgx_log(MGX_LOG_STDERR, "send_msg_handler send_size = -1");
        return;
    }

    sem_post(&m_send_queue_sem);

    c->throw_send_cnt = 0;
    delete[] c->psend_mem_addr;
    c->psend_mem_addr = nullptr;
}
