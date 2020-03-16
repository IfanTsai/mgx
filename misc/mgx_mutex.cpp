#include "mgx_mutex.h"

Mgx_mutex::Mgx_mutex(pthread_mutex_t *mutex)
{
    m_mutex = mutex;
    pthread_mutex_lock(m_mutex);
}

Mgx_mutex::~Mgx_mutex()
{
    pthread_mutex_unlock(m_mutex);
}