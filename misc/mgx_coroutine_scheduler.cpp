#include "mgx_coroutine.h"
#include "mgx_util.h"

Mgx_coroutine_scheduler *Mgx_coroutine_scheduler::m_instance = nullptr;

Mgx_coroutine_scheduler::Mgx_coroutine_scheduler()
{
    m_epoll_fd = epoll_create(1024);
    MGX_ASSERT(m_epoll_fd >= 0, std::string("epoll_create error: ") + strerror(errno));
    m_epoll_wait_default_timeout_ms = MGX_CO_SCHEDULER_EPOLL_WAIT_TIMEOUT;

    m_co_ready_list = new std::list<Mgx_coroutine *>();
    m_co_sleep_rbtree = new std::multimap<time_t, Mgx_coroutine *>();
    m_ctx = new mgx_ctx_t;

    m_cur_co = nullptr;    
}

Mgx_coroutine_scheduler::~Mgx_coroutine_scheduler()
{
    delete m_ctx;
    delete m_co_sleep_rbtree;
    delete m_co_ready_list;
    close(m_epoll_fd);
}

Mgx_coroutine_scheduler* Mgx_coroutine_scheduler::get_instance() 
{
    if (!m_instance)
        m_instance = new Mgx_coroutine_scheduler();

    return m_instance;
}

void Mgx_coroutine_scheduler::push_back_ready_list(Mgx_coroutine *co)
{
    m_co_ready_list->push_back(co);
}

void Mgx_coroutine_scheduler::erase_ready_list(Mgx_coroutine *co)
{
    m_co_ready_list->remove(co);
}

void Mgx_coroutine_scheduler::insert_sleep_rbtree(long ms, Mgx_coroutine *co)
{
    m_co_sleep_rbtree->emplace(ms, co);
}

void Mgx_coroutine_scheduler::remove_first_sleep_rbtree()
{
    m_co_sleep_rbtree->erase(m_co_sleep_rbtree->begin());
}


void Mgx_coroutine_scheduler::add_event_wait_epoll(Mgx_coroutine *co, uint32_t rw_event)
{
    int fd = co->get_wait_fd();
    struct epoll_event ev = { 0 };
    ev.events = EPOLLERR | EPOLLHUP | rw_event;
    ev.data.ptr = co;
    int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    MGX_ASSERT(ret == 0, strerror(errno));
}

void Mgx_coroutine_scheduler::remove_event_wait_epoll(Mgx_coroutine *co)
{
    int fd = co->get_wait_fd();
    struct epoll_event ev = { 0 };
    int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
    MGX_ASSERT(ret == 0, strerror(errno));
}

long Mgx_coroutine_scheduler::get_now_ms()
{
    struct timeval tv = { 0 };

    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

long Mgx_coroutine_scheduler::get_epoll_wait_timeout_ms()
{
    if (!m_co_sleep_rbtree->empty() && m_co_ready_list->empty()) {
        long sleep_ms = m_co_sleep_rbtree->begin()->first;
        long now_ms = get_now_ms();
        if (sleep_ms > now_ms) {
            long diff_ms = sleep_ms - now_ms;
            return std::min(diff_ms, m_epoll_wait_default_timeout_ms);
        }
    }

    return m_epoll_wait_default_timeout_ms;
}

void Mgx_coroutine_scheduler::schedule()
{
    /* no loop */

    /* sleep rbtree */
    if (!m_co_sleep_rbtree->empty()) {
        long now_ms = get_now_ms();
        long sleep_ms = m_co_sleep_rbtree->begin()->first;
        if (now_ms >= sleep_ms) {
            Mgx_coroutine *co = m_co_sleep_rbtree->begin()->second;
            if (!co->resume())
                delete co;
        }
    }

    /* ready queue */
    if (!m_co_ready_list->empty()) {
        Mgx_coroutine *co = m_co_ready_list->front();
        m_co_ready_list->erase(m_co_ready_list->begin());
        if (!co->resume())
            delete co;
    }

    /* wait rbtree */
    long timeout = get_epoll_wait_timeout_ms();
    int num_events = epoll_wait(m_epoll_fd, m_events, MGX_CO_MAX_EVENTS, static_cast<int>(timeout));
    if (num_events == -1)
        MGX_ASSERT(errno == EINTR, std::string("epoll_wait error: ") + strerror(errno));

    for (int i = 0; i < num_events; i++) {
        Mgx_coroutine *co = static_cast<Mgx_coroutine *>(m_events[i].data.ptr);
        uint32_t recv_events = m_events[i].events;
        if (recv_events & EPOLLERR) {
            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, co->get_wait_fd(), nullptr);
            continue;
        }
        if (!co->resume())
            delete co;
    }
}

mgx_ctx_t *Mgx_coroutine_scheduler::get_ctx() 
{
    return m_ctx;
}

void Mgx_coroutine_scheduler::set_current_coroutine(Mgx_coroutine *co)
{
    m_cur_co = co;
}

Mgx_coroutine *Mgx_coroutine_scheduler::get_current_coroutine()
{
    return m_cur_co;
}