#include <unistd.h>
#include <sys/syscall.h>
#include <cstdlib>
#include <cmath>
#include "mgx_conf.h"
#include "mgx_log.h"
#include "mgx_cosocket.h"

#define PORT            8081
#define CONNECT_TIMEOUT 2000
#define TEST_COUNT      10240

void client(void *arg)
{
    Mgx_cosocket sock;
    sock.socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in seraddr = { 0 };
    seraddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &seraddr.sin_addr);
    seraddr.sin_port = htons(PORT);

    int ret = sock.connect((const struct sockaddr *)&seraddr, sizeof(seraddr), CONNECT_TIMEOUT);
    MGX_ASSERT(ret == 0, strerror(errno));

    const char *str = "hello";
    sock.send(str, strlen(str), 0);

    sock.close();
}

int main(void)
{
    for (int i = 0; i < TEST_COUNT; i++) {
        new Mgx_coroutine(client, nullptr);  // delete by scheduler when coroutine function run finished
    }

    for (;;) {
        Mgx_coroutine_scheduler::get_instance()->schedule();
    }



    return 0;
}
