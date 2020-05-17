#include "mgx_signal.h"
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include "mgx_log.h"
#include "mgx_process_cycle.h"

extern int g_pid;
extern bool g_is_mgx_master;
static void default_handler(int signo, siginfo_t *info, void *ucontext);
static void mgx_process_get_status();
bool g_mgx_reap;
bool g_mgx_log_rotate;

typedef struct {
    int signo;
    const char *signame;
    void (*handler)(int, siginfo_t *, void *);
} mgx_signal_t;

mgx_signal_t signals[] = {
    { SIGUSR1, "SIGUSR1", default_handler },
    { SIGINT,  "SIGINT",  default_handler },
    { SIGTERM, "SIGTERM", default_handler },
    { SIGHUP,  "SIGHUP",  default_handler },
    { SIGCHLD, "SIGCHLD", default_handler },
    { SIGQUIT, "SIGQUIT", default_handler },
    { SIGIO,   "SIGIO",   default_handler },
    { SIGSYS,  "SIGSYS",  default_handler },
    { 0,       nullptr,   nullptr },
};

void mgx_signal_init()
{
    struct sigaction act;
    /* register signal handlers */
    for (int i = 0; signals[i].signo; i++) {
        memset(&act, 0, sizeof(act));
        if (signals[i].handler) {
            act.sa_flags = SA_SIGINFO;
            act.sa_sigaction = signals[i].handler;
        } else {
            act.sa_handler = SIG_IGN; /* signal ignore */
        }
        if (sigaction(signals[i].signo, &act, nullptr) < 0) {
            mgx_log_stderr("%d sigaction error", signals[i].signo);
            exit(1);
        }
    }
}

static void default_handler(int signo, siginfo_t *siginfo, void *ucontext)
{
    //mgx_log_stderr("pid:%d signal:%d received", g_pid, signo);
    mgx_signal_t signal = signals[0];
    for (int i = 0; signals[i].signo; i++) {
        if (signo == signals[i].signo) {
            break;
        }
    }

    if (g_is_mgx_master) {
        /* master process received signal */
        switch (signo) {
            case SIGCHLD:
				mgx_process_get_status(); /* waitpid */
                g_mgx_reap = true;
                break;
            case SIGUSR1:
                g_mgx_log_rotate = true;  /* log rotate by date */
                break;
        }
    } else {
        /* worker process received signal */
    }

	if (siginfo && siginfo->si_pid) {
		mgx_log(MGX_LOG_NOTICE, "signal %d(%s) received from %P", signo, signal.signame, siginfo->si_pid);
	} else {
		mgx_log(MGX_LOG_NOTICE, "signal %d(%s) received", signo, signal.signame);
	}
}

static void mgx_process_get_status()
{
	pid_t pid;
    int status, err, one = 0;

    for (;;) {
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0)
            return;

        if (pid == -1) {
            err = errno;

            if (err == EINTR) { /* call interrupted by a signal */
                continue;
            }

            if (err == ECHILD && one) { /* no child process */
                return;
            }

            /*
             * Solaris always calls the signal handler for each exited process
             * despite waitpid() may be already called for this process.
             *
             * When several processes exit at the same time FreeBSD may
             * erroneously call the signal handler for exited process
             * despite waitpid() may be already called for this process.
             */
            if (err == ECHILD) {
                mgx_log(MGX_LOG_INFO, "waitpid() failed: %s", strerror(err));
                return;
            }

            mgx_log(MGX_LOG_ALERT, "waitpid() failed: %s", strerror(err));
            return;
        }

        one = 1;

        if (WTERMSIG(status)) {
            mgx_log(MGX_LOG_ALERT, "%P exited on signal %d", pid, WTERMSIG(status));
        } else {
            mgx_log(MGX_LOG_NOTICE, "%P exited with code %d", pid, WEXITSTATUS(status));
        }
    }
}
