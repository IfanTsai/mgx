#include "mgx_setproctitle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

extern char **environ;
extern char **g_argv;
static int g_env_size = 0;

void mgx_move_env()
{
    for (int i = 0; environ[i]; i++)
        g_env_size += strlen(environ[i]) + 1;

    char *mgx_env = new char[g_env_size];
    memset(mgx_env, 0, g_env_size);

    char *t_mgx_env = mgx_env;
    /* move env to new memory (mgx_env) */
    for (int i = 0; environ[i]; i++) {
        strcpy(t_mgx_env, environ[i]);
        environ[i] = t_mgx_env; /* environ[i] point to new memory */
        t_mgx_env += (strlen(environ[i]) + 1);
    }
}

void mgx_setproctitle(const char *title)
{
    /* argv_size only calc one time */
    static int argv_size = 0;
    if (argv_size == 0)
        for (int i = 0; g_argv[i]; i++)
            argv_size += (strlen(g_argv[i]) + 1);

    int title_size = strlen(title) + 1;
    if (title_size > g_env_size + argv_size) return;

    g_argv[1] = nullptr;      /* don't use argv */
    strcpy(g_argv[0], title); /* set title */
    /* clear the reset memory */
    memset(g_argv[0] + title_size, 0, g_env_size + argv_size - title_size);

    /* set main thread name */
    pthread_setname_np(pthread_self(), "mgx_main_th");
}