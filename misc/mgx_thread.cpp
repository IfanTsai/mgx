#include "mgx_thread.h"

Mgx_thread::Mgx_thread(Thread_callback callback, std::string &&name)
: m_callback(std::move(callback)),
  m_name(name)
{

}

Mgx_thread::~Mgx_thread()
{
    if (m_isrunning)
        pthread_detach(m_pthread_id);
}

int Mgx_thread::start()
{
    return pthread_create(&m_pthread_id, nullptr, run, this);
}
void Mgx_thread::join()
{
    pthread_join(m_pthread_id, nullptr);
    m_isrunning = false;
}

void *Mgx_thread::run(void *arg)
{
    Mgx_thread *pthis = static_cast<Mgx_thread*>(arg);
    pthis->m_tid = syscall(SYS_gettid);
    pthread_setname_np(pthis->m_pthread_id, pthis->m_name.substr(0, 15).c_str());
    pthis->m_isrunning = true;
    pthis->m_callback();

    return (void *) 0;
}