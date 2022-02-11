#include "mgx_thread_pool.h"
#include "mgx_log.h"
#include "mgx_logic_socket.h"
#include "mgx_http_socket.h"
#include <cstring>
#include <unistd.h>

extern Mgx_socket *gp_mgx_socket;

Mgx_th_pool::Mgx_th_pool()
{
    m_running_cnt = 0;
}

Mgx_th_pool::~Mgx_th_pool()
{
    destory_all();
}

bool Mgx_th_pool::create(int th_cnt)
{
    m_th_cnt = th_cnt;
    for (int i = 0; i < m_th_cnt; i++) {
        Mgx_thread *thread = new Mgx_thread(std::bind(&Mgx_th_pool::th_func, this),
                                            "mgx_th_pool_" + std::to_string(i));
        m_threads.emplace_back(std::move(thread));
        int err = m_threads[i]->start();
        if (err != 0) {
            mgx_log(MGX_LOG_STDERR, "thread pool %d create error: %s", i, strerror(err));
            return false;
        }
    }

    /* Loop until all threads start running */
    for (auto it = m_threads.begin(); it != m_threads.end(); it++) {
        if (!(*it)->is_running()) {
            it--;
            usleep(100 * 1000);  /* sleep 100ms */
        }
    }
    return true;
}

void Mgx_th_pool::th_func()
{
    char *msg_buf = nullptr;
    for (;;) {
        int err = pthread_mutex_lock(&m_mutex);   /* lock */
        if (err != 0)
            mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

        /* Consumer */
        while (!m_msg_queue_size && !m_shutdown)
            pthread_cond_wait(&m_cond, &m_mutex); /* unlock, but wait */

        if (m_shutdown) {
            err = pthread_mutex_unlock(&m_mutex);
            if (err != 0)
                mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));
            break;
        }

        msg_buf = m_msg_queue.front();
        m_msg_queue.pop();
        m_msg_queue_size--;

        err = pthread_mutex_unlock(&m_mutex);
        if (err != 0)
            mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));

        m_running_cnt++;

        /* handle specific business */
        gp_mgx_socket->th_msg_process_func(msg_buf);

        delete[] msg_buf;
        m_running_cnt--;
    }
}

void Mgx_th_pool::destory_all()
{
    if (m_shutdown == true)
        return;
    m_shutdown = true;

    int err = pthread_cond_broadcast(&m_cond);
    if (err != 0) {
        mgx_log(MGX_LOG_STDERR, "pthread_cond_broadcast error: %s", strerror(err));
        return;
    }

    for (auto it = m_threads.begin(); it != m_threads.end(); it++)
        (*it)->join();

    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);

    for (auto it = m_threads.begin(); it != m_threads.end(); it++) {
        if (*it)
            delete *it;
    }
    m_threads.clear();

    while (!m_msg_queue.empty()) {
        delete[] m_msg_queue.front();
        m_msg_queue.pop();
    }

    mgx_log(MGX_LOG_NOTICE, "thread pool destroy !");
}

void Mgx_th_pool::signal_to_th()
{
    /* Producer */
    int err = pthread_cond_signal(&m_cond);
    if (err != 0)
        mgx_log(MGX_LOG_STDERR, "pthread_cond_signal error: %s", strerror(err));

    /*
     * Remind that WorkerThreadCount in the mgx.conf may need to be adjusted
     */
    if (m_running_cnt == m_th_cnt) {
        time_t cur_time = time(nullptr);
        if (cur_time - last_time > 10) {
            last_time = cur_time;
            mgx_log(MGX_LOG_ALERT, "threads in thread pool not enough !");
        }
    }
}

void Mgx_th_pool::insert_msg_to_queue_and_signal(char *buf)
{
    int err = pthread_mutex_lock(&m_mutex);
    if (err != 0)
        mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

    m_msg_queue.push(buf);
    m_msg_queue_size++;

    err = pthread_mutex_unlock(&m_mutex);
    if (err != 0)
        mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));

    signal_to_th();
}
