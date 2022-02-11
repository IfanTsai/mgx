#include "mgx_socket.h"
#include "mgx_conf.h"
#include "mgx_log.h"
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>

Mgx_socket::Mgx_socket()
{
    pthread_mutex_init(&m_conn_mutex, nullptr);
    pthread_mutex_init(&m_recy_queue_mutex, nullptr);
    pthread_mutex_init(&m_send_queue_mutex, nullptr);
    pthread_mutex_init(&m_timer_que_mutex, nullptr);

    m_total_conns_cnt = m_free_conns_cnt = m_total_recy_conns_cnt = 0;
    m_send_list_cnt = 0;
    m_timer_que_size = 0;
}

Mgx_socket::~Mgx_socket()
{
    for (auto it = m_listen_skts.begin(); it != m_listen_skts.end(); it++)
        delete *it;

    m_listen_skts.clear();
    pthread_mutex_destroy(&m_timer_que_mutex);
    pthread_mutex_destroy(&m_send_queue_mutex);
    pthread_mutex_destroy(&m_recy_queue_mutex);
    pthread_mutex_destroy(&m_conn_mutex);

    if (m_send_thread) {
        m_send_thread->join();
        delete m_send_thread;
    }

    conn_pool_destroy();
}

void Mgx_socket::read_conf()
{
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    m_listen_cnt = mgx_conf->get_int(CONFIG_ListenPortCount, 1);
    m_worker_conns = mgx_conf->get_int(CONFIG_WorkerConnections, DEFAULT_WORKER_CONNS);
    m_recy_conn_wait_time = mgx_conf->get_int(CONFIG_RecyConnectionWaitTime, DEFAULT_RECY_CONN_WAIT_TIME);
    m_enabled_heartbeat = mgx_conf->get_int(CONFIG_Heartbeat, 0);
    m_heart_wait_time = mgx_conf->get_int(CONFIG_HeartWaitTime, 30);
    m_heart_wait_time = m_heart_wait_time <= 5 ? 5 : m_heart_wait_time;
}

inline bool Mgx_socket::init()
{
    read_conf();
    return open_listen_skts();
}

bool Mgx_socket::open_listen_skts()
{
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    struct sockaddr_in seraddr;
    bzero(&seraddr, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = /*inet_addr(INADDR_ANY)*/ htonl(INADDR_ANY);

    char buf[1024] = { 0 };
    for (int i = 0; i < m_listen_cnt; i++) {
        /* socket */
        int listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd < 0) {
            mgx_log(MGX_LOG_STDERR, "socket %d error: %s", i, strerror(errno));
            return false;
        }

        int opt = 1;
        /* set socket SO_REUSEADDR */
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            mgx_log(MGX_LOG_STDERR, "setsockopt SO_REUSEADDR %d error: %s", i, strerror(errno));
            close(listenfd);
            return false;
        }

        /* set socket SO_REUSEPORT to solve thundering herd effect */
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            mgx_log(MGX_LOG_ERR, "setsockopt SO_REUSEPORT %d error: %s", i, strerror(errno));
        }

        set_nonblock(listenfd);  /* set socket io nonblock*/

        sprintf(buf, CONFIG_ListenPort"%d", i);
        int listen_port = mgx_conf->get_int(buf, DEFAULT_LISTEN_PORT);  /* get listen port*/
        seraddr.sin_port = htons(listen_port);

        /* bind */
        if (bind(listenfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) < 0) {
            mgx_log(MGX_LOG_STDERR, "bind %d error: %s", listen_port, strerror(errno));
            close(listenfd);
            return false;
        }

        /* listen */
        if (listen(listenfd, MGX_LISTEN_BACKLOG) < 0) {
            mgx_log(MGX_LOG_STDERR, "listen %d error: %s", listen_port, strerror(errno));
            close(listenfd);
            return false;
        }

        /* push mgx_listen_skt to m_listen_skts */
        pmgx_listen_skt_t mgx_listen_skt = new mgx_listen_skt_t;
        mgx_listen_skt->port = listen_port;
        mgx_listen_skt->fd   = listenfd;
        m_listen_skts.push_back(mgx_listen_skt);

        mgx_log(MGX_LOG_INFO, "listen %d succeed", listen_port);
    }
    return true;
}

void Mgx_socket::set_nonblock(int listenfd)
{
    int flags = fcntl(listenfd, F_GETFL);
    fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
#if 0 /* another way */
    int opt = 1;
    ioctl(listenfd, FIONBIN, &opt);
#endif
}

void Mgx_socket::close_listen_skts()
{
    for (unsigned int i = 0; i < m_listen_skts.size(); i++) {
        close(m_listen_skts[i]->fd);
        mgx_log(MGX_LOG_INFO, "close %d", m_listen_skts[i]->port);
    }
}

void Mgx_socket::send_msg_th_init()
{
    if (sem_init(&m_send_queue_sem, 0, 0) < 0) {
        mgx_log(MGX_LOG_STDERR, "sem_init(&m_send_queue_sem, 0, 0) error: %s", strerror(errno));
        exit(1);
    }

    /* thread used to process send message queue */
    m_send_thread = new Mgx_thread(std::bind(&Mgx_socket::send_msg_th_func, this), "mgx_send_th");
    int err = m_send_thread->start();
    if (err != 0) {
        mgx_log(MGX_LOG_STDERR, "send thread create error: %s", strerror(err));
        exit(1);
    }
}

void Mgx_socket::epoll_init()
{
    m_epoll_fd = epoll_create(m_worker_conns);
    if (m_epoll_fd < 0) {
        mgx_log(MGX_LOG_STDERR, "epoll_create error: %s", strerror(errno));
        exit(1);
    }

    conn_pool_init();

    send_msg_th_init();

#ifndef USE_HTTP
    if (m_enabled_heartbeat)
        heart_timer_init();
#endif

    for (auto it = m_listen_skts.begin(); it != m_listen_skts.end(); it++) {
        pmgx_conn_t c = get_conn((*it)->fd);
        if (!c) {
            mgx_log(MGX_LOG_STDERR, "get_conn fd:%d error", (*it)->fd);
            exit(1);
        }
        c->listen_skt = *it;
        (*it)->pconn = c;

        c->r_handler = std::bind(&Mgx_socket::event_accept, this, std::placeholders::_1);

        if (!epoll_oper_event((*it)->fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP,
                                EPOLL_ES_MOD_ACTION::IGNORE, c)) {
            close_conn(c);
            return;
        }
    }
}

/**
 * fd:            socket fd
 * e_type:        event types, eg. EPOLL_CTL_ADD
 * es:            events, eg. EPOLLIN | EPOLLRDHUP
 * mod_action:
 *    only valid if e_type is EPOLL_CTL_MOD,
 *    1 means adding event flag, 0 means removing event flag,
 *    and - 1 means full coverage
 * c:             a connection in connection pool
 */
bool Mgx_socket::epoll_oper_event(int fd, uint32_t e_type, uint32_t es,
                                    EPOLL_ES_MOD_ACTION mod_action, pmgx_conn_t c)
{
    struct epoll_event ev = { 0 };

    switch (e_type) {
        case EPOLL_CTL_ADD:
            c->events = ev.events = es;
            break;
        case EPOLL_CTL_MOD:
            if (mod_action == EPOLL_ES_MOD_ACTION::ADD)
                c->events |= es;
            else if (mod_action == EPOLL_ES_MOD_ACTION::REMOVE)
                c->events &= ~es;
            else if (mod_action == EPOLL_ES_MOD_ACTION::FULL_COVERAGE)
                c->events = es;
            ev.events = c->events;
            break;
        case EPOLL_CTL_DEL:
            break;
        default:
            return false;
    }

    ev.data.ptr = (void *)((uintptr_t)c | c->instance);
    if (epoll_ctl(m_epoll_fd, e_type, fd, &ev) < 0) {
        mgx_log(MGX_LOG_STDERR, "epoll_ctl (%d, %d, %d, %d) error: %s",
                            fd, e_type, es, static_cast<int>(mod_action), strerror(errno));
        return false;
    }
    return true;
}

bool Mgx_socket::epoll_process_events(int timeout)
{
    int num_events = epoll_wait(m_epoll_fd, m_events, MGX_MAX_EVENTS, timeout);

    if (num_events < 0) {
        mgx_log(MGX_LOG_INFO, "epoll_wait error: %s", strerror(errno));
        return errno == EINTR ? true : false;
    } else if (num_events == 0) {
        if (timeout == -1)
            return true;

        mgx_log(MGX_LOG_ALERT, "epoll_wait error: %s", strerror(errno));
        return false;
    }

    for (int i = 0; i < num_events; i++) {
        pmgx_conn_t c = (pmgx_conn_t) m_events[i].data.ptr;
        uintptr_t instance = (uintptr_t)c & 1;
        c = (pmgx_conn_t)((uintptr_t)c & (uintptr_t) ~1);

        if (c->fd == -1 || c->instance != instance) {
            /*
             * the stale event from a file descriptor
             * that was just closed in this iteration
             */
            mgx_log(MGX_LOG_DEBUG, "epoll stale event %p", c);
            continue;
        }

        uint32_t recv_events = m_events[i].events;
        /*
        if (recv_events & (EPOLLERR | EPOLLHUP))
             recv_events |= (EPOLLIN | EPOLLOUT);
        */

        if (recv_events & EPOLLIN) {
            c->r_handler(c);
        }

        if (recv_events & EPOLLOUT) {
            c->w_handler(c);
        }
    }
    return true;
}

void Mgx_socket::send_msg(char *send_buf)
{
    Mgx_mutex mutex(&m_send_queue_mutex);
    m_send_list.push_back(send_buf);
    m_send_list_cnt++;
    sem_post(&m_send_queue_sem);
}

void Mgx_socket::send_msg_th_func()
{
    int err;
    pmgx_msg_hdr_t pmsg_hdr = nullptr;
#ifndef USE_HTTP
    pmgx_pkg_hdr_t ppkg_hdr = nullptr;
#endif
    pmgx_conn_t pconn;

    for (;;) {
        sem_wait(&m_send_queue_sem);
        if (m_send_list_cnt > 0) {
            auto it = m_send_list.begin();
            while (it != m_send_list.end()) {
                err = pthread_mutex_lock(&m_send_queue_mutex);
                if (err != 0)
                    mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

                pmsg_hdr = (pmgx_msg_hdr_t)(*it);
#ifndef USE_HTTP
                ppkg_hdr = (pmgx_pkg_hdr_t)(*it + m_msg_hdr_size);
#endif
                pconn = pmsg_hdr->pconn;

                if (pconn->m_cur_seq != pmsg_hdr->cur_seq) {
                    m_send_list_cnt--;
                    it = m_send_list.erase(it);     /* erase can it = it + 1 */
                    delete[] (*it);
                    continue;
                }

                if (pconn->throw_send_cnt > 0) {
                    it++;
                    continue;
                }

                pconn->psend_buf = (*it) + m_msg_hdr_size;
#ifndef USE_HTTP
                pconn->rest_send_size = ppkg_hdr->pkg_size;
#else
                pconn->rest_send_size = strlen(*it + m_msg_hdr_size);
#endif

                mgx_log(MGX_LOG_DEBUG, "start to send %d data", pconn->rest_send_size);

                pconn->psend_mem_addr = *it;
                it = m_send_list.erase(it);     /* erase can it = it + 1 */
                m_send_list_cnt--;

                ssize_t send_size = send_uninterrupt(pconn, pconn->psend_buf, pconn->rest_send_size);

                if (send_size > 0) {
                    if (send_size == pconn->rest_send_size) {
                        pconn->throw_send_cnt = 0;

                        delete[] pconn->psend_mem_addr;
                        pconn->psend_mem_addr = nullptr;
                        mgx_log(MGX_LOG_DEBUG, "send over !!!");
                    } else {
                        mgx_log(MGX_LOG_DEBUG, "should send %d, but send %d", pconn->rest_send_size, send_size);
                        pconn->psend_buf += send_size;
                        pconn->rest_send_size -= send_size;
                        pconn->throw_send_cnt++;
                        if (!epoll_oper_event(pconn->fd, EPOLL_CTL_MOD, EPOLLOUT,
                                                EPOLL_ES_MOD_ACTION::ADD, pconn)) {
                            mgx_log(MGX_LOG_STDERR, "epoll_oper_event EPOLL_CTL_MOD error: %s", strerror(errno));
                        }
                    }
                } else if (send_size == 0) {
                    pconn->throw_send_cnt = 0;
                } else if (send_size == -1) {
                    pconn->throw_send_cnt++;
                    if (!epoll_oper_event(pconn->fd, EPOLL_CTL_MOD, EPOLLOUT,
                                            EPOLL_ES_MOD_ACTION::ADD, pconn)) {
                        mgx_log(MGX_LOG_STDERR, "epoll_oper_event EPOLL_CTL_MOD error: %s", strerror(errno));
                    }
                } else {
                    pconn->throw_send_cnt = 0;
                    delete[] pconn->psend_mem_addr;
                    pconn->psend_mem_addr = nullptr;
                }

                err = pthread_mutex_unlock(&m_send_queue_mutex);
                if (err != 0)
                    mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));

            }
        }
    }
}

ssize_t Mgx_socket::send_uninterrupt(pmgx_conn_t c, char *buf, ssize_t size)
{
    for (;;) {
        ssize_t n = send(c->fd, buf, size, 0);
        if (n >= 0)
            return n;
        else if (errno == EAGAIN)
            return -1;
        else if (errno == EINTR)
            continue;
        else
            return -2;
    }
}
