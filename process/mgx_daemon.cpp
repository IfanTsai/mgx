#include "mgx_daemon.h"
#include "mgx_log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <cstdlib>

int mgx_daemon()
{
    pid_t pid = fork();
    if (pid < 0) {
        mgx_log(MGX_LOG_EMERG, "fork error: %s", strerror(errno));
        return -1;
    } else if (pid > 0) {
        /* parent */
        return 1;
    }

    if (setsid() < 0) {
        mgx_log(MGX_LOG_EMERG, "setsid error: %s", strerror(errno));
        exit(1);
    }

    umask(0);

    /* let pid != sid, then never get the control terminal */
    // if (fork() != 0)
    //     return 1;

    for (int i = 0; i < 3; i++)
        close(i);

    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    return 0;
}
