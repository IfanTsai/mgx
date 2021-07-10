#ifndef __MGX_COROUTINE_H__
#define __MGX_COROUTINE_H__ 

#include <map>
#include <list>
#include <functional>
#include <sys/epoll.h>
#include <algorithm>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include "mgx_util.h"

#define MGX_CO_MAX_EVENTS (500)
#define MGX_CO_STACK_SIZE (16 * 1024)
#define MGX_CO_SCHEDULER_EPOLL_WAIT_TIMEOUT (200)

typedef struct {
#ifdef __x86_64__
    void *rsp;
    void *rbp;
    void *rip;
    void *rbx;
    void *r12;
    void *r13;
    void *r14;
    void *r15;
#elif __aarch64__
    void *sp;
    void *x29; // fp
    void *x30; // lr
    void *x19;
    void *x20;
    void *x21;
    void *x22;
    void *x23;
    void *x24;
    void *x25;
    void *x26;
    void *x27;
    void *x28;
#else
    #error "Not implement in this architecture yet !"
#endif
} mgx_ctx_t;

enum class COROUTINE_STATUS {
    READY = 0,
    WAITING,
    SLEEPING,
    RUNNING,
    EXITED,
};

typedef std::function<void(void *)> co_func_t;
class Mgx_coroutine_scheduler;

class Mgx_coroutine {
private:
    static long cnt;
    Mgx_coroutine_scheduler *m_scheduler;
    uint64_t m_id;
    COROUTINE_STATUS m_status;
    void *m_stack;
    uint64_t m_stack_size;
    co_func_t m_func;
    void *m_arg;
    mgx_ctx_t *m_ctx;
    int m_wait_fd;

    void _switch(mgx_ctx_t *cur_ctx, mgx_ctx_t *new_ctx);
    static void _exec(void *arg);

public:
    Mgx_coroutine(co_func_t func, void *arg);
    ~Mgx_coroutine();

    void yield(bool push_ready_list = true);
    bool resume();
    void msleep(int ms);
    
    Mgx_coroutine_scheduler* get_schduler();
    void set_wait_fd(int fd);
    int get_wait_fd();
    void set_status(COROUTINE_STATUS status);
    COROUTINE_STATUS get_status();
    co_func_t get_func();
    void *get_func_arg();
    uint64_t get_id();
    mgx_ctx_t *get_ctx();
};

class Mgx_coroutine_scheduler {
private:
    Mgx_coroutine_scheduler();
    static Mgx_coroutine_scheduler *m_instance;

public:
    ~Mgx_coroutine_scheduler();
    static Mgx_coroutine_scheduler* get_instance();

private:
    int m_epoll_fd;
    struct epoll_event m_events[MGX_CO_MAX_EVENTS];
    long m_epoll_wait_default_timeout_ms;
    Mgx_coroutine *m_cur_co;
    mgx_ctx_t *m_ctx;
    std::list<Mgx_coroutine *> *m_co_ready_list;
    std::multimap<long, Mgx_coroutine *> *m_co_sleep_rbtree;

    long get_epoll_wait_timeout_ms();

public:
    void schedule();
    long get_now_ms();
    void push_back_ready_list(Mgx_coroutine *co);
    void erase_ready_list(Mgx_coroutine *co);
    void insert_sleep_rbtree(long ms, Mgx_coroutine *co);
    void remove_first_sleep_rbtree();
    void add_event_wait_epoll(Mgx_coroutine *co, uint32_t rw_event);
    void remove_event_wait_epoll(Mgx_coroutine *co);
    mgx_ctx_t *get_ctx();
    void set_current_coroutine(Mgx_coroutine *co);
    Mgx_coroutine *get_current_coroutine();
};

#endif //__MGX_COROUTINE_H__ 
