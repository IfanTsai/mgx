#if !defined(__MGX_MUTEX_H__)
#define __MGX_MUTEX_H__

#include <pthread.h>

class Mgx_mutex
{
public:
    Mgx_mutex(pthread_mutex_t *mutex);
    ~Mgx_mutex();
private:
    pthread_mutex_t *m_mutex;
};

#endif // __MGX_MUTEX_H__
