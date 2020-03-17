#if !defined(__MGX_THREAD_H__)
#define __MGX_THREAD_H__

#include <functional>
#include <pthread.h>

class Mgx_thread
{
    typedef std::function<void ()> Thread_callback;

    pthread_t m_tid = -1;
    bool m_isrunning = false;
    Thread_callback m_callback;

    static void *run(void *);

public:
    Mgx_thread(Thread_callback call_back);
    ~Mgx_thread();

    int start();
    void join();
	bool is_running();
};


#endif // __MGX_THREAD_H__
