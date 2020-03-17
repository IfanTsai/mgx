#if !defined(__MGX_THREAD_POOL_H__)
#define __MGX_THREAD_POOL_H__

#include <pthread.h>
#include <atomic>
#include <vector>
#include <queue>
#include "mgx_thread.h"

class Mgx_th_pool
{
public:
    Mgx_th_pool();
    ~Mgx_th_pool();

    bool create(int th_cnt);
    void destory_all();
    void insert_msg_to_queue_and_signal(char *buf);

private:
    void th_func();
    void signal_to_th();

    static pthread_mutex_t m_mutex;
    static pthread_cond_t  m_cond;

    static bool m_shutdown;
    int m_th_cnt;
    std::queue<char *> m_msg_queue;
    std::queue<char *>::size_type m_msg_queue_size;
    std::atomic<int> m_running_cnt;
    std::vector<Mgx_thread *> m_threads;

    time_t last_time = 0;
};

#endif // __MGX_THREAD_POOL_H__
