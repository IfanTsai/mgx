#include "mgx_logic_socket.h"
#include "mgx_log.h"
#include "mgx_crc32.h"
#include "mgx_mutex.h"
#include "mgx_string.h"

typedef bool (Mgx_logic_socket::*handler_t)(
    pmgx_msg_hdr_t pmsg_hdr,
    char *pkg_body,
    unsigned short pkg_body_size
);

static const handler_t pkg_handlers[] = {
    &Mgx_logic_socket::ping_handler,      /* 0 */
    &Mgx_logic_socket::register_handler,  /* 1 */
    &Mgx_logic_socket::login_handler,     /* 2 */
    nullptr,
};

#define PKG_HANDLERS_CNT ARRAY_SIZE(pkg_handlers)

Mgx_logic_socket::Mgx_logic_socket()
{

}

Mgx_logic_socket::~Mgx_logic_socket()
{

}

bool Mgx_logic_socket::init()
{
    // ...
    return Mgx_socket::init();
}

void Mgx_logic_socket::th_msg_process_func(char *buf)
{
    pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)buf;
    pmgx_pkg_hdr_t ppkg_hdr = (pmgx_pkg_hdr_t)(buf + m_msg_hdr_size);
    unsigned short pkg_size = ppkg_hdr->pkg_size;
    void *ppkg_body = nullptr;

    /* only package header, no body */
    if (pkg_size == m_pkg_hdr_size) {
        if (ppkg_hdr->crc32) {
            mgx_log(MGX_LOG_STDERR, "no body, but crc32 not zero");
            return;
        }
    } else {
        ppkg_body = buf + m_msg_hdr_size + m_pkg_hdr_size;
        unsigned int crc32 = Mgx_crc32::get_instance()->get_crc32((unsigned char *)ppkg_body, pkg_size - m_pkg_hdr_size);
        if (crc32 != ppkg_hdr->crc32) {
            mgx_log(MGX_LOG_STDERR, "crc32 error, just discard");
            return;
        }
    }

    pmgx_conn_t pconn = pmsg_hdr->pconn;

    if (pconn->m_cur_seq != pmsg_hdr->cur_seq) {
        return;
    }

    unsigned short pkg_type = ppkg_hdr->pkg_type;
    if (pkg_type >= PKG_HANDLERS_CNT) {
        mgx_log(MGX_LOG_STDERR, "pkg_type invalid");
        return;
    }

    if (!pkg_handlers[pkg_type]) {
        mgx_log(MGX_LOG_STDERR, "pkg_handlers[pkg_type] is NULL");
        return;
    }
    mgx_log(MGX_LOG_DEBUG, "pkg_type = %d", pkg_type);
    (this->*pkg_handlers[pkg_type])(pmsg_hdr, (char *)ppkg_body, pkg_size - m_pkg_hdr_size);
}

void Mgx_logic_socket::send_msg_with_nobody(pmgx_msg_hdr_t pmsg_hdr, unsigned short pkg_type)
{
    char *psend_buf = new char[m_msg_hdr_size + m_pkg_hdr_size];
    memcpy(psend_buf, pmsg_hdr, m_msg_hdr_size);
    pmgx_pkg_hdr_t psend_pkg_hdr = (pmgx_pkg_hdr_t)(psend_buf + m_msg_hdr_size);
    psend_pkg_hdr->pkg_type = pkg_type;
    psend_pkg_hdr->pkg_size = m_pkg_hdr_size;
    psend_pkg_hdr->crc32 =  0;
    send_msg(psend_buf);
}

bool Mgx_logic_socket::ping_handler(pmgx_msg_hdr_t pmsg_hdr,
                            char *pkg_body, unsigned short pkg_body_size)
{
    if (pkg_body_size != 0)
        return false;

    pmgx_conn_t pconn = pmsg_hdr->pconn;
    Mgx_mutex(&pconn->m_mutex);
    pconn->last_ping_time = time(nullptr);   /* update heart time */
    /* send heartbeat to client */
    send_msg_with_nobody(pmsg_hdr, static_cast<unsigned short>(PKG_TYPE::PING));

    mgx_log(MGX_LOG_DEBUG, "ping_handler called succeed");
    return true;
}

bool Mgx_logic_socket::register_handler(pmgx_msg_hdr_t pmsg_hdr,
                                char *pkg_body, unsigned short pkg_body_size)
{
    // ...
    mgx_log(MGX_LOG_DEBUG, "register_handler called succeed, msg: %s", pkg_body);

    pmgx_conn_t pconn = pmsg_hdr->pconn;
    Mgx_mutex(&pconn->m_mutex);

    // ...

    char send_pkg_body[65000] = "register succeed";
    char *psend_buf = new char[m_msg_hdr_size + m_pkg_hdr_size + sizeof(send_pkg_body)];
    memcpy(psend_buf, pmsg_hdr, m_msg_hdr_size);

    pmgx_pkg_hdr_t psend_pkg_hdr = (pmgx_pkg_hdr_t)(psend_buf + m_msg_hdr_size);
    psend_pkg_hdr->pkg_type = static_cast<unsigned short>(PKG_TYPE::REGISTER);
    psend_pkg_hdr->pkg_size = m_pkg_hdr_size + sizeof(send_pkg_body);
    psend_pkg_hdr->crc32 =  Mgx_crc32::get_instance()->get_crc32((unsigned char *)send_pkg_body, sizeof(send_pkg_body));

    memcpy((char *)psend_pkg_hdr + m_pkg_hdr_size, send_pkg_body, sizeof(send_pkg_body));

    send_msg(psend_buf);
    //send(pconn->fd, psend_pkg_hdr, psend_pkg_hdr->pkg_size, 0);

    // if (!epoll_oper_event(pmsg_hdr->pconn->fd, EPOLL_CTL_MOD, EPOLLOUT, 1, pmsg_hdr->pconn)) {
    //     insert_recy_conn_queue(pmsg_hdr->pconn);
    //     return false;
    // }
    return true;
}

bool Mgx_logic_socket::login_handler(pmgx_msg_hdr_t pmsg_hdr,
                            char *pkg_body, unsigned short pkg_body_size)
{
    // ...
    mgx_log(MGX_LOG_DEBUG, "login_handler called succeed, msg: %s", pkg_body);
    return true;
}
