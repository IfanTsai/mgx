#if !defined(__MGX_SOCKET_H__)
#define __MGX_SOCKET_H__

#include <vector>
#include <queue>
#include <list>
#include <map>
#include <unordered_set>
#include <atomic>
#include <cstdint>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstring>
#include "mgx_comm.h"
#include "mgx_thread.h"

#define DEFAULT_LISTEN_PORT  100000
#define DEFAULT_WORKER_CONNS 1024
#define DEFAULT_RECY_CONN_WAIT_TIME 80

#define MGX_LISTEN_BACKLOG 500
#define MGX_MAX_EVENTS     500

class Mgx_socket;

enum class EPOLL_ES_MOD_ACTION {
    FULL_COVERAGE = -1,
    REMOVE,
    ADD,
    IGNORE,
};

typedef struct _mgx_conn mgx_conn_t, *pmgx_conn_t;

typedef struct {
    int port;
    int fd;
    pmgx_conn_t pconn;
} mgx_listen_skt_t, *pmgx_listen_skt_t;

struct _mgx_conn {
    typedef std::function<void(pmgx_conn_t)> mgx_event_handler_t;

    int fd;     /* connection socket fd */
    pmgx_listen_skt_t listen_skt;
    unsigned int instance:1;
    uint64_t m_cur_seq;
    mgx_event_handler_t r_handler;
    mgx_event_handler_t w_handler;
    uint32_t   events;
    struct sockaddr    conn_sockaddr;
    pthread_mutex_t m_mutex;

    /* recv */
    unsigned char recv_stat;        /* receiving status of the current connection */
    char hdr_buf[PKG_HDR_BUF_SIZE];
    char *precv_buf;
    unsigned int rest_recv_size;
    char *precv_mem_addr;

    /* send */
    char *psend_buf;
    unsigned int rest_send_size;
    char *psend_mem_addr;
    std::atomic<int> throw_send_cnt;

    time_t in_recy_time;
    time_t last_ping_time;

    void get_one_to_use();
    void put_one_to_free();
};

typedef struct {
    pmgx_conn_t pconn;
    uint64_t    cur_seq;
#ifdef USE_HTTP
    uint        http_method;
    char*       uri;
#endif
    // ...
} mgx_msg_hdr_t, *pmgx_msg_hdr_t;

class Mgx_socket
{
public:
    Mgx_socket();
    virtual ~Mgx_socket();

public:
    ssize_t     m_pkg_hdr_size = sizeof(mgx_pkg_hdr_t);
    ssize_t     m_msg_hdr_size = sizeof(mgx_msg_hdr_t);

    virtual bool init();
    virtual void th_msg_process_func(char *buf);
    void epoll_init();
    bool epoll_process_events(int timeout);
    bool epoll_oper_event(int fd, uint32_t e_type, uint32_t flag, EPOLL_ES_MOD_ACTION mod_action, pmgx_conn_t pconn);

protected:
    void send_msg(char *send_buf);
    /* mgx_socket_conn.cpp */
    void insert_recy_conn_queue(pmgx_conn_t c);
    /* mgx_socket_request.cpp */
    ssize_t recv_process(pmgx_conn_t c, char *buf, ssize_t buf_size);

private:
    int m_listen_cnt = 1;
    int m_worker_conns = DEFAULT_WORKER_CONNS;
    std::vector<pmgx_listen_skt_t> m_listen_skts;
    int m_epoll_fd;
    struct epoll_event m_events[MGX_MAX_EVENTS];

    /* connection */
    std::queue<pmgx_conn_t> m_pconns_queue;       /* connection pool */
    std::queue<pmgx_conn_t> m_pconns_free_queue;  /* free connections in connection pool */
    std::unordered_set<pmgx_conn_t> m_recy_conn_set;    /* recovery connections in connection pool */
    std::atomic<int> m_total_conns_cnt;         /* the number of connection pool */
    std::atomic<int> m_free_conns_cnt;          /* the number of free connections */
    std::atomic<int> m_total_recy_conns_cnt;    /* the number of recovery connections */
    int m_recy_conn_wait_time = DEFAULT_RECY_CONN_WAIT_TIME;
    pthread_mutex_t m_conn_mutex;
    pthread_mutex_t m_recy_queue_mutex;
    Mgx_thread *m_recy_thread;

    /* send */
    std::list<char *> m_send_list;
    std::atomic<int> m_send_list_cnt;
    sem_t m_send_queue_sem;
    pthread_mutex_t m_send_queue_mutex;
    Mgx_thread *m_send_thread;

    /* heartbeat */
    int m_enabled_heartbeat;
    int m_heart_wait_time;
    std::multimap<time_t, pmgx_msg_hdr_t> m_timer_queue;
    pthread_mutex_t m_timer_que_mutex;
    std::atomic<int> m_timer_que_size;
    time_t  m_timer_que_head_time;
    Mgx_thread *m_monitor_timer_thread;

    /* mgx_socket.cpp */
    void read_conf();
    bool open_listen_skts();
    void close_listen_skts();
    void set_nonblock(int listenfd);
    void send_msg_th_init();
    void send_msg_th_func();
    ssize_t send_uninterrupt(pmgx_conn_t c, char *buf, ssize_t size);

    /* mgx_socket_conn.cpp */
    void conn_pool_init();
    void conn_pool_destroy();
    pmgx_conn_t get_conn(int sock_fd);
    void free_conn(pmgx_conn_t c);
    void close_conn(pmgx_conn_t c);
    void recy_conn_th_func();

    /* mgx_socket_accept.cpp */
    void event_accept(pmgx_conn_t pconn_listen);

    /* mgx_socket_request.cpp */
    void send_msg_handler(pmgx_conn_t c);
    virtual void _read_request_handler(pmgx_conn_t c);
    void read_request_handler(pmgx_conn_t c);
    void read_request_handler_process_v1(pmgx_conn_t c);  /* process package header */
    void read_request_handler_process_v2(pmgx_conn_t c);  /* process package body */

    /* mgx_socket_time.cpp */
    void heart_timer_init();
    void add_to_timer_queue(pmgx_conn_t pconn);
    time_t get_earliest_time();
    pmgx_msg_hdr_t get_over_time_timer(time_t cur_time);
    void monitor_timer_th_func();
    void delete_from_timer_queue(pmgx_conn_t pconn);
};

#endif  // __MGX_SOCKET_H__
