#include "mgx_thread.h"

Mgx_thread::Mgx_thread(Thread_callback callback)
: m_callback(std::move(callback))
{

}

Mgx_thread::~Mgx_thread()
{
    if (m_isrunning)
        pthread_detach(m_tid);
}

int Mgx_thread::start()
{
    return pthread_create(&m_tid, nullptr, run, this);
}
void Mgx_thread::join()
{
    pthread_join(m_tid, nullptr);
	m_isrunning = false;
}

void *Mgx_thread::run(void *arg)
{
    Mgx_thread *pthis = static_cast<Mgx_thread*>(arg);
	pthis->m_isrunning = true;
    pthis->m_callback();

    return (void *) 0;
}

bool Mgx_thread::is_running()
{
	return m_isrunning;
}
