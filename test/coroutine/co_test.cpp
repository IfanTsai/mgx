#include <unistd.h>
#include <cstdlib>
#include <vector>
#include "mgx_conf.h"
#include "mgx_log.h"
#include "mgx_coroutine.h"

int g_pid = getpid();

#define USE_COROUTINE_SCHEDULER

#define DEFINE_FUNC(NUM)   \
    void func##NUM(void *arg)  \
    {  \
        Mgx_coroutine_scheduler *sch = Mgx_coroutine_scheduler::get_instance(); \
        Mgx_coroutine *co = sch->get_current_coroutine(); \
        for (;;) { \
            char *str = (char *)arg; \
            printf("======> co%d: %s\n", NUM, str);\
            /*co->yield();*/  \
            long t1 = sch->get_now_ms(); \
            co->msleep(NUM * 1000); \
            long t2 = sch->get_now_ms(); \
            printf("co%d sleep %ldms\n", NUM, t2 - t1); \
        } \
    }

DEFINE_FUNC(1)
DEFINE_FUNC(2)
DEFINE_FUNC(3)
DEFINE_FUNC(4)
DEFINE_FUNC(5)

int main(int argc, char *argv[])
{
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    if (!mgx_conf->load("../../mgx.conf")) {
        mgx_log_stderr("load config failed !!!");
        exit(1);
    }
    mgx_log_init();

#ifdef USE_COROUTINE_SCHEDULER
    new Mgx_coroutine(func1, (void *)"1111");
    new Mgx_coroutine(func2, (void *)"2222");
    new Mgx_coroutine(func3, (void *)"3333");
    new Mgx_coroutine(func4, (void *)"4444");
    new Mgx_coroutine(func5, (void *)"5555");
#else
    Mgx_coroutine *co1 = new Mgx_coroutine(func1, (void *)"1111");
    Mgx_coroutine *co2 = new Mgx_coroutine(func2, (void *)"2222");
    Mgx_coroutine *co3 = new Mgx_coroutine(func3, (void *)"3333");
    Mgx_coroutine *co4 = new Mgx_coroutine(func4, (void *)"4444");
    Mgx_coroutine *co5 = new Mgx_coroutine(func5, (void *)"5555");
#endif

    for (;;) {
#ifdef USE_COROUTINE_SCHEDULER
        Mgx_coroutine_scheduler::get_instance()->schedule();
#else
        co1 && !co1->resume() && (co1 = nullptr);
        co2 && !co2->resume() && (co2 = nullptr);
        co3 && !co3->resume() && (co3 = nullptr);
        co4 && !co4->resume() && (co4 = nullptr);
        co5 && !co5->resume() && (co5 = nullptr);
#endif
    }
    return 0;
}
