#if !defined(__MGX_THREAD_H__)
#define __MGX_THREAD_H__

#include <functional>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

class Mgx_thread
{
    typedef std::function<void ()> Thread_callback;

    pthread_t m_pthread_id = -1;
    pid_t m_tid = -1;
    bool m_isrunning = false;
    Thread_callback m_callback;
    std::string m_name;

    static void *run(void *);

public:
    Mgx_thread(Thread_callback call_back, std::string &&name="mgx_thread");
    ~Mgx_thread();

    int start();
    void join();
    bool is_running() const { return m_isrunning; };
    pid_t get_tid() const { return m_tid; };
    const std::string& get_name() const {return m_name; };
};


#endif // __MGX_THREAD_H__
