#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include "mgx_conf.h"
#include "mgx_log.h"
#include "mgx_process_cycle.h"
#include "mgx_setproctitle.h"
#include "mgx_signal.h"
#include "mgx_daemon.h"
#include "mgx_logic_socket.h"
#include "mgx_thread_pool.h"
#include "mgx_crc32.h"

using namespace std;

char **g_argv;
int g_pid;
bool g_is_mgx_master;
Mgx_logic_socket g_mgx_socket;
Mgx_th_pool g_mgx_th_pool;

int main(int argc, char *argv[])
{
    g_argv = argv;
    g_pid = getpid();

    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    if (!mgx_conf->load("./mgx.conf")) {
        mgx_log_stderr("load config failed !!!");
        exit(1);
    }

    mgx_log_init();
    mgx_signal_init();
    if (!g_mgx_socket.init()) {
        mgx_log(MGX_LOG_STDERR, "socket init failed");
        exit(1);
    }

    if (mgx_conf->get_int(CONFIG_Daemon, 0)) {
        int res = mgx_daemon();
        if (res < 0) {
            mgx_log(MGX_LOG_STDERR, "mgx_daemon create error");
        } else if (res == 1) {
            return res;
        }
        /* this is a daemon */
    }

    g_is_mgx_master = true;
    mgx_process_cycle();

    return 0;
}
