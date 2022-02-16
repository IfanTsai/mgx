#include "mgx_socket.h"
#include "mgx_log.h"
#include "mgx_comm.h"
#include "mgx_mutex.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <errno.h>

void mgx_conn_t::get_one_to_use()
{
    m_cur_seq++;
    recv_stat = PKG_HDR_START;
    precv_buf = hdr_buf;
    rest_recv_size = sizeof(mgx_pkg_hdr_t);
    precv_mem_addr = psend_mem_addr = nullptr;
    throw_send_cnt = 0;
    last_ping_time = time(nullptr);
}
void mgx_conn_t::put_one_to_free()
{
    m_cur_seq++;
    fd = -1;

    if (precv_mem_addr) {
        delete[] precv_mem_addr;
        precv_mem_addr = nullptr;
    }

    if (psend_mem_addr) {
        delete[] psend_mem_addr;
        psend_mem_addr = nullptr;
    }
}

void Mgx_socket::conn_pool_init()
{
    pmgx_conn_t pconn;

    for (int i = 0; i < m_worker_conns; i++) {
        pconn = new mgx_conn_t();
        pconn->get_one_to_use();
        m_pconns_queue.push(pconn);
        m_pconns_free_queue.push(pconn);
    }
    m_free_conns_cnt = m_total_conns_cnt = m_worker_conns;

    /* thread used to delayed recovery connections */
    m_recy_thread = new Mgx_thread(std::bind(&Mgx_socket::recy_conn_th_func, this), "mgx_recy_th");
    int err = m_recy_thread->start();
    if (err != 0) {
        mgx_log(MGX_LOG_STDERR, "recy thread start error: %s", strerror(err));
        exit(1);
    }
}

pmgx_conn_t Mgx_socket::get_conn(int sock_fd)
{
    Mgx_mutex mutex(&m_conn_mutex);

    pmgx_conn_t pconn = nullptr;
    if (m_free_conns_cnt > 0) {
        pconn = m_pconns_free_queue.front();
        m_pconns_free_queue.pop();
        pconn->get_one_to_use();
        m_free_conns_cnt--;
        pconn->fd = sock_fd;

        uintptr_t instance = pconn->instance;
        pconn->instance = !instance;
    } else {
        pconn = new mgx_conn_t();
        pconn->get_one_to_use();
        m_pconns_queue.push(pconn);
        m_total_conns_cnt++;
        pconn->fd = sock_fd;
    }

    return pconn;
}

void Mgx_socket::conn_pool_destroy()
{
    pmgx_conn_t pconn;

    while (!m_pconns_queue.empty()) {
        pconn = m_pconns_queue.front();
        m_pconns_queue.pop();
        delete pconn;
    }

    while (!m_pconns_free_queue.empty())
        m_pconns_free_queue.pop();

    if (m_recy_thread) {
        m_recy_thread->join();
        delete m_recy_thread;
    }
}

void Mgx_socket::free_conn(pmgx_conn_t c)
{
    Mgx_mutex mutex(&m_conn_mutex);

    c->put_one_to_free();
    m_pconns_free_queue.push(c);
    m_free_conns_cnt++;
}

/* delayed recovery */
void Mgx_socket::insert_recy_conn_queue(pmgx_conn_t c)
{
    if (m_enabled_heartbeat) {
        delete_from_timer_queue(c);
    }

    if (close(c->fd) < 0)
        mgx_log(MGX_LOG_ALERT, "close fd %d error: %s", c->fd, strerror(errno));

    Mgx_mutex mutex(&m_recy_queue_mutex);
    if (m_recy_conn_set.find(c) != m_recy_conn_set.end())
        return;

    c->in_recy_time = time(nullptr);
    c->m_cur_seq++;

    m_recy_conn_set.insert(c);
    //m_recy_conn_set.push_back(c);
    m_total_recy_conns_cnt++;
    mgx_log(MGX_LOG_DEBUG, "insert_recy_conn_queue called");
}

/* immediate recovery */
void Mgx_socket::close_conn(pmgx_conn_t c)
{
    if (close(c->fd) < 0) {
        mgx_log(MGX_LOG_ALERT, "close fd %d error: %s", c->fd, strerror(errno));
    }
    free_conn(c);
}

void Mgx_socket::recy_conn_th_func()
{
    time_t cur_time;
    int err;
    pmgx_conn_t pconn;

    for (;;) {
        usleep(200 * 1000);
        if (m_total_recy_conns_cnt > 0) {
            auto it = m_recy_conn_set.begin();
            while (it != m_recy_conn_set.end()) {
                cur_time = time(nullptr);
                pconn = *it;
                if (cur_time >= pconn->in_recy_time + m_recy_conn_wait_time) {
                    err = pthread_mutex_lock(&m_recy_queue_mutex);
                    if (err != 0)
                        mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

                    m_total_recy_conns_cnt--;
                    it = m_recy_conn_set.erase(it);     /* erase can it = it + 1 */
                    mgx_log(MGX_LOG_DEBUG, "recy a connection, fd: %d", pconn->fd);
                    free_conn(pconn);

                    err = pthread_mutex_unlock(&m_recy_queue_mutex);
                    if (err != 0)
                        mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));
                }
            }
        }
    }
}
