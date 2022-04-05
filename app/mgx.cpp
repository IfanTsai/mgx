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
#include "mgx_http_socket.h"
#include "mgx_thread_pool.h"
#include "mgx_crc32.h"

char **g_argv;
bool g_is_mgx_master;
Mgx_th_pool g_mgx_th_pool;

Mgx_socket *gp_mgx_socket;

int main(int argc, char *argv[])
{
    g_argv = argv;

    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    if (!mgx_conf->load("./mgx.conf")) {
        mgx_log_stderr("load config failed !!!");
        exit(1);
    }

    mgx_log_init();
    mgx_signal_init();

    if (mgx_conf->get_int(CONFIG_Daemon, 0)) {
        int res = mgx_daemon();
        if (res < 0) {
            mgx_log(MGX_LOG_STDERR, "mgx_daemon create error");
        } else if (res == 1) {
            close(STDERR_FILENO);

            return 0;
        }
        /* this is a daemon */
    }

#ifdef USE_HTTP
    gp_mgx_socket = new Mgx_http_socket();
#else
    gp_mgx_socket = new Mgx_logic_socket();
#endif

    g_is_mgx_master = true;
    mgx_process_cycle();

    return 0;
}
