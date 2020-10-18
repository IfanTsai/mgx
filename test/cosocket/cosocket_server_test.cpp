#include <unistd.h>
#include <cstdlib>
#include "mgx_conf.h"
#include "mgx_log.h"
#include "mgx_cosocket.h"

int g_pid;

struct ser_cli_pair {
    Mgx_cosocket *ser_sock;
    int cli_fd;
};

void server_reader(void *arg)
{
    ser_cli_pair *pair = (ser_cli_pair *) arg;
    Mgx_cosocket *sock = pair->ser_sock;
    int fd = pair->cli_fd;
    delete pair;
    for (;;) {
        char buf[1024] = { 0 };
        int ret = sock->recv(fd, buf, 1024, 0);
        if (ret > 0) {
            std::cout << buf;
            sock->send(fd, buf, 1024, 0);
        } else if (ret == 0) {
            sock->close(fd);
            break;
        }
    }
}

void server(void *arg)
{
    struct sockaddr_in seraddr;
    bzero(&seraddr, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = /*inet_addr(INADDR_ANY)*/ htonl(INADDR_ANY);
    seraddr.sin_port = htons(8081);

    Mgx_cosocket *sock = new Mgx_cosocket();
    sock->socket(AF_INET, SOCK_STREAM, 0);
    sock->bind((struct sockaddr *)&seraddr, sizeof(seraddr));
    sock->listen(500);
    int conn_cnt = 0;

    Mgx_coroutine_scheduler* sch = Mgx_coroutine_scheduler::get_instance();
    long t1 = sch->get_now_ms();
    for (;;) {
        ser_cli_pair *pair = new ser_cli_pair;
        struct sockaddr cliaddr = { 0 };
        socklen_t addrlen  = 0 ;
        int fd = sock->accept(&cliaddr, &addrlen);
        MGX_ASSERT(fd > 0, strerror(errno));
        pair->ser_sock = sock;
        pair->cli_fd = fd;
        new Mgx_coroutine(server_reader, (void *)pair);

        conn_cnt++;
        if (1000 == conn_cnt) {
            long t2 = sch->get_now_ms();
            printf("t2 - t1 = %ldms\n", t2 - t1);
            conn_cnt = 0;
            t1 = sch->get_now_ms();
        }
    }
}

int main(int argc, char *argv[])
{
    g_pid = getpid();

    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    if (!mgx_conf->load("../../mgx.conf")) {
        mgx_log_stderr("load config failed !!!");
        exit(1);
    }

    mgx_log_init();

    new Mgx_coroutine(server, (void *)0);  // delete by scheduler when coroutine function run finished 
    for (;;) {
        Mgx_coroutine_scheduler::get_instance()->schedule();
    }

    return 0;
}
