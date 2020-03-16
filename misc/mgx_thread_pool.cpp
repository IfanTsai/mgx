#include "mgx_thread_pool.h"
#include "mgx_log.h"
#include "mgx_logic_socket.h"
#include <cstring>
#include <unistd.h>

extern Mgx_logic_socket g_mgx_socket;

pthread_mutex_t Mgx_th_pool::m_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  Mgx_th_pool::m_cond = PTHREAD_COND_INITIALIZER;
bool Mgx_th_pool::m_shutdown = false;

Mgx_th_pool::Mgx_th_pool()
{
    m_running_cnt = 0;
}

Mgx_th_pool::~Mgx_th_pool()
{
    while (!m_msg_queue.empty()) {
        char *buf = m_msg_queue.front();
        m_msg_queue.pop();
        delete[] buf;
    }
}

bool Mgx_th_pool::create(int th_cnt)
{
    Thread_item *pitem = nullptr;
    int err;

    m_th_cnt = th_cnt;
    for (int i = 0; i < m_th_cnt; i++) {
        m_th_items.push_back(pitem = new Thread_item(this));
        err = pthread_create(&pitem->tid, nullptr, th_func, pitem);
        if (err != 0) {
            mgx_log(MGX_LOG_STDERR, "pthread_create error: %s", strerror(err));
            return false;
        }
    }

    /* Loop until all threads start running */
    for (auto it = m_th_items.begin(); it != m_th_items.end(); it++) {
        if (!(*it)->if_running) {
            it--;
            usleep(100 * 1000);  /* sleep 100ms */
        }
    }
    return true;
}

void *Mgx_th_pool::th_func(void *arg)
{
    Thread_item *pitem = (Thread_item*)arg;
    Mgx_th_pool *pthis = pitem->pthis;

    int err;
    char *msg_buf = nullptr;
    while (1) {
        err = pthread_mutex_lock(&m_mutex);   /* lock */
        if (err != 0)
            mgx_log(MGX_LOG_STDERR, "pthread_mutex_lock error: %s", strerror(err));

        /* Consumer */
        while (!pthis->m_msg_queue_size && !m_shutdown) {
            if (!pitem->if_running)
                pitem->if_running = true;
            pthread_cond_wait(&m_cond, &m_mutex); /* unlock, but wait */
        }

        if (m_shutdown) {
            err = pthread_mutex_unlock(&m_mutex);
            if (err != 0)
                mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));
            break;
        }

        msg_buf = pthis->m_msg_queue.front();
        pthis->m_msg_queue.pop();
        pthis->m_msg_queue_size--;

        err = pthread_mutex_unlock(&m_mutex);
        if (err != 0)
            mgx_log(MGX_LOG_STDERR, "pthread_mutex_unlock error: %s", strerror(err));

        pthis->m_running_cnt++;

        /* handle specific business */
        g_mgx_socket.th_msg_process_func(msg_buf);

        delete[] msg_buf;
        pthis->m_running_cnt--;
    }
    return (void *)0;
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

    for (auto it = m_th_items.begin(); it != m_th_items.end(); it++) {
        pthread_join((*it)->tid, nullptr);
    }

    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);

    for (auto it = m_th_items.begin(); it != m_th_items.end(); it++) {
        if (*it)
            delete *it;
    }
    m_th_items.clear();

    while (!m_msg_queue.empty()) {
        delete[] m_msg_queue.front();
        m_msg_queue.pop();
    }

    mgx_log(MGX_LOG_NOTICE, "thread pool destory !");
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