#include "mgx_process_cycle.h"
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include "mgx_conf.h"
#include "mgx_log.h"
#include "mgx_setproctitle.h"
#include "mgx_logic_socket.h"
#include "mgx_thread_pool.h"

extern char **g_argv;
extern bool g_is_mgx_master;
extern Mgx_socket *gp_mgx_socket;
extern Mgx_th_pool g_mgx_th_pool;
extern bool g_mgx_reap;
extern bool g_mgx_log_rotate;

static const char *MASTER_PROCESS_NAME = "mgx: master process";
static const char *WORKER_PROCESS_NAME = "mgx: worker process";

static inline void mgx_worker_process_init(int worker_nr)
{
    sigset_t set;
    sigemptyset(&set);
    /* worker process is ready to receive signals */
    if (sigprocmask(SIG_SETMASK, &set, nullptr) < 0) {
        mgx_log_stderr("worker %d sigprocmask error: %s", worker_nr, strerror(errno));
    }
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    int th_cnt = mgx_conf->get_int(CONFIG_WorkerThreadCount);
    if (!g_mgx_th_pool.create(th_cnt))  {
        mgx_log(MGX_LOG_STDERR, "thread pool create failed");
        exit(1);
    }

    gp_mgx_socket->epoll_init();
}

static void mgx_worker_process_cycle(int worker_nr, const char *process_name)
{
    if (!gp_mgx_socket->init()) {
        mgx_log(MGX_LOG_STDERR, "socket init failed");
        exit(1);
    }

    mgx_worker_process_init(worker_nr);
    mgx_setproctitle(process_name);
    mgx_log(MGX_LOG_NOTICE, "%s %P is running ...", process_name, getpid());

    for (;;) {
        gp_mgx_socket->epoll_process_events(-1);
        // ...
    }

    g_mgx_th_pool.destory_all();
}

static inline void mgx_spawn_process(int worker_nr, const char *process_name)
{
    pid_t pid = fork();
    if (pid < 0) {
        mgx_log_stderr("fork worker %d error: %s", worker_nr, strerror(errno));
        return;
    } else if (pid == 0) {
        g_is_mgx_master = false;
        mgx_worker_process_cycle(worker_nr, process_name);
    }
}

static void mgx_start_worker_process(int nums_worker)
{
    for (int i = 0; i < nums_worker; i++)
        mgx_spawn_process(i, WORKER_PROCESS_NAME);
}

void mgx_process_cycle()
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGWINCH);
    sigaddset(&set, SIGQUIT);

    /* not interrupted by signal during call to fork */
    if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1) {
        mgx_log_stderr("sigprocmask() failed: %s", strerror(errno));
    }

    char title[1024] = { 0 };
    strcat(title, MASTER_PROCESS_NAME);
    /* append argv list to title */
    for (int i = 0; g_argv[i]; i++) {
        strcat(title, " ");
        strcat(title, g_argv[i]);
    }
    mgx_move_env();
    mgx_setproctitle(title);
    mgx_log(MGX_LOG_NOTICE, "%s %P is running ...", title, getpid);

    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    int nums_worker = mgx_conf->get_int(CONFIG_WorkerProcesses, 1);
    mgx_start_worker_process(nums_worker);

    sigemptyset(&set);
    for (;;) {
        sigsuspend(&set);
        mgx_log(MGX_LOG_DEBUG, "signal received");

        if (g_mgx_reap) {
            g_mgx_reap = false;
            mgx_spawn_process(nums_worker++, WORKER_PROCESS_NAME);
            mgx_log(MGX_LOG_NOTICE, "restart subprocess");
        }

        if (g_mgx_log_rotate) {
            g_mgx_log_rotate = false;
            mgx_log_init();      /* log rotate by date */
            mgx_log(MGX_LOG_NOTICE, "log rotate by date");
        }
    }
}
