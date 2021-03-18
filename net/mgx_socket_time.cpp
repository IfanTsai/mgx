#include "mgx_socket.h"
#include "mgx_mutex.h"
#include "mgx_log.h"

inline time_t Mgx_socket::get_earliest_time()
{
    return m_timer_queue.begin()->first;
}

void Mgx_socket::add_to_timer_queue(pmgx_conn_t pconn)
{
    time_t time_out = time(nullptr) + m_heart_wait_time;

    pmgx_msg_hdr_t pmsg_hdr = new mgx_msg_hdr_t;
    pmsg_hdr->pconn = pconn;
    pmsg_hdr->cur_seq = pconn->m_cur_seq;

    Mgx_mutex mutex(&m_timer_que_mutex);
    m_timer_queue.emplace(time_out, pmsg_hdr);
    m_timer_que_size++;
    m_timer_que_head_time = get_earliest_time();
}

void Mgx_socket::delete_from_timer_queue(pmgx_conn_t pconn)
{
    for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++) {
        if (it->second->pconn == pconn) {
            m_timer_queue.erase(it);
            m_timer_que_size--;
            break;
        }
    }

    if (m_timer_que_size > 0)
        m_timer_que_head_time = get_earliest_time();
}

void Mgx_socket::heart_timer_init()
{
    /* thread used to monitor timer queue */
    m_monitor_timer_thread = new Mgx_thread(std::bind(&Mgx_socket::monitor_timer_th_func, this), "mgx_timer_th");
    int err = m_monitor_timer_thread->start();
    if (err != 0) {
        mgx_log(MGX_LOG_STDERR, "heart timer thread create error: %s", strerror(err));
        exit(1);
    }
}

void Mgx_socket::monitor_timer_th_func()
{
    time_t cur_time, timer_que_head_time;
    int err;

    while (1) {
        if (m_timer_que_size > 0) {
            timer_que_head_time = m_timer_que_head_time;
            cur_time = time(nullptr);
            if (cur_time > timer_que_head_time) {
                std::queue<pmgx_msg_hdr_t> que;
                pmgx_msg_hdr_t pmsg_hdr;

                err = pthread_mutex_lock(&m_timer_que_mutex);
                if (err != 0)
                    mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

                while ((pmsg_hdr = get_over_time_timer(cur_time)))
                    que.push(pmsg_hdr);

                err = pthread_mutex_unlock(&m_timer_que_mutex);
                if (err != 0)
                    mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));

                while (!que.empty()) {
                    pmsg_hdr = que.front();
                    que.pop();
                    if (pmsg_hdr->cur_seq == pmsg_hdr->pconn->m_cur_seq) {
                        mgx_log(MGX_LOG_DEBUG, "time_duration: %d, wait_time: %d",
                            cur_time - pmsg_hdr->pconn->last_ping_time,  m_heart_wait_time );
                        if (cur_time - pmsg_hdr->pconn->last_ping_time > m_heart_wait_time * 3) {
                            mgx_log(MGX_LOG_DEBUG, "no heartbeat packet received after timeout");
                            insert_recy_conn_queue(pmsg_hdr->pconn);
                            /* insert_recy_conn_queue has done */
                            //m_timer_que_size--;
                        }
                    }
                    delete[] pmsg_hdr;
                }
            }
        }
        cur_time = time(nullptr);
        long sleep_time_us = (m_timer_que_head_time - cur_time) * 1000 * 1000;
        usleep(sleep_time_us > 0 ? sleep_time_us : 200 * 1000);
    }
}

pmgx_msg_hdr_t Mgx_socket::get_over_time_timer(time_t cur_time)
{
    if (!m_timer_que_size || cur_time < get_earliest_time())
        return nullptr;

    /* delete over timer */
    pmgx_msg_hdr_t pold_msg_hdr = m_timer_queue.begin()->second;
    m_timer_queue.erase(m_timer_queue.begin());
    m_timer_que_size--;

    /* add over timer */
    pmgx_msg_hdr_t pnew_msg_hdr = new mgx_msg_hdr_t;
    pnew_msg_hdr->pconn = pold_msg_hdr->pconn;
    pnew_msg_hdr->cur_seq = pold_msg_hdr->cur_seq;
    m_timer_queue.emplace(cur_time + m_heart_wait_time, pnew_msg_hdr);
    m_timer_que_size++;

    /* update m_timer_que_head_time */
    m_timer_que_head_time = get_earliest_time();

    return pold_msg_hdr;
}
