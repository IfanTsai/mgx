#include "mgx_coroutine.h"

long Mgx_coroutine::cnt = 0;

Mgx_coroutine::Mgx_coroutine(co_func_t func, void *arg)
: m_func(std::move(func)), m_arg(arg)
{
    m_scheduler = Mgx_coroutine_scheduler::get_instance();

    m_stack_size = MGX_CO_STACK_SIZE;
    int ret = posix_memalign(&m_stack, getpagesize(), m_stack_size);
    MGX_ASSERT(ret == 0, "posix_memalign error");

    void **stack = (void **)(m_stack + m_stack_size);
    stack[-1] = nullptr;

    m_ctx = new mgx_ctx_t;
    m_ctx->rsp = static_cast<void *>(stack) - (sizeof(void *) * 2);
    m_ctx->rip = (void *) _exec;

    Mgx_coroutine::cnt++;
    m_id = Mgx_coroutine::cnt;
    m_status = COROUTINE_STATUS::READY;

    m_scheduler->push_back_ready_list(this);
}

Mgx_coroutine::~Mgx_coroutine()
{
    delete m_ctx;
    free(m_stack);
}

void Mgx_coroutine::_exec(void *arg)
{
    Mgx_coroutine *co = static_cast<Mgx_coroutine *>(arg);
    co_func_t func = co->get_func();
    void *func_arg = co->get_func_arg();
    func(func_arg);

    co->set_status(COROUTINE_STATUS::EXITED);
    Mgx_coroutine_scheduler *scheduler = Mgx_coroutine_scheduler::get_instance();
    co->_switch(co->get_ctx(), scheduler->get_ctx());
}

/*
 * %rdi: this, %rsi: cur_ctx, %rdx: new_ctx
 * save current context to parameter_1: cur_ctx, switch context to parameter_2: new_ctx
 */
void Mgx_coroutine::_switch(mgx_ctx_t *cur_ctx, mgx_ctx_t *new_ctx)
{
    __asm__ (
    "       movq %rsp, 0(%rsi)      # save stack pointer     \n"
    "       movq %rbp, 8(%rsi)      # save frame pointer     \n"
    "       movq (%rsp), %rax                                \n"
    "       movq %rax, 16(%rsi)     # save pc pointer        \n"
    "       movq %rbx, 24(%rsi)     # save rbx, r12-r15      \n"
    "       movq %r12, 32(%rsi)                              \n"
    "       movq %r13, 40(%rsi)                              \n"
    "       movq %r14, 48(%rsi)                              \n"
    "       movq %r15, 56(%rsi)                              \n"
    "       movq 56(%rdx), %r15                              \n"
    "       movq 48(%rdx), %r14                              \n"
    "       movq 40(%rdx), %r13     # restore rbx, r12-r15   \n"
    "       movq 32(%rdx), %r12                              \n"
    "       movq 24(%rdx), %rbx                              \n"
    "       movq 8(%rdx), %rbp      # restore frame pointer  \n"
    "       movq 0(%rdx), %rsp      # restore stack pointer  \n"
    "       movq 16(%rdx), %rax     # restore pc pointer     \n"
    "       movq %rax, (%rsp)       # pc pointer push stack  \n"
    "       ret                                              \n"
    );
}

void Mgx_coroutine::yield(bool push_ready_list)
{
    if (push_ready_list) {
        m_scheduler->push_back_ready_list(this);
        m_status = COROUTINE_STATUS::READY;
    } else {
        m_status = COROUTINE_STATUS::WAITING;
    }
    _switch(m_ctx, m_scheduler->get_ctx());
}

bool Mgx_coroutine::resume()
{
    m_scheduler->set_current_coroutine(this);
    m_status = COROUTINE_STATUS::RUNNING;
    _switch(m_scheduler->get_ctx(), m_ctx);
    m_scheduler->set_current_coroutine(nullptr);
    return m_status != COROUTINE_STATUS::EXITED;
}

void Mgx_coroutine::msleep(int ms)
{
    long now_ms = m_scheduler->get_now_ms();
    long sleep_ms = now_ms + ms;
    m_status = COROUTINE_STATUS::SLEEPING;
    m_scheduler->insert_sleep_rbtree(sleep_ms, this);
    _switch(m_ctx, m_scheduler->get_ctx()); 
    m_scheduler->remove_first_sleep_rbtree();
}

Mgx_coroutine_scheduler* Mgx_coroutine::get_schduler() {
    return m_scheduler;
}

void Mgx_coroutine::set_wait_fd(int fd)
{
    m_wait_fd = fd;
}

int Mgx_coroutine::get_wait_fd() 
{
    return m_wait_fd;
}

void Mgx_coroutine::set_status(COROUTINE_STATUS status)
{
    m_status = status;
}

COROUTINE_STATUS Mgx_coroutine::get_status()
{
    return m_status;
}

co_func_t Mgx_coroutine::get_func()
{
    return m_func;
}

void *Mgx_coroutine::get_func_arg()
{
    return m_arg;
}

uint64_t Mgx_coroutine::get_id()
{
    return m_id;
}

mgx_ctx_t *Mgx_coroutine::get_ctx()
{
    return m_ctx;
}