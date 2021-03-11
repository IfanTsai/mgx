#ifndef __MGX_HTTP_SOCKET_H__
#define __MGX_HTTP_SOCKET_H__

#include "mgx_socket.h"
#include "mgx_log.h"
#include <unordered_map>

#define CR    '\r'
#define LF    '\n'
#define CRLF  "\r\n"

#define METHOD_GET  (0x01)
#define METHOD_POST (0x02)

typedef struct _mgx_http_request {
    char recv_buf[1024 * 8];
    char *preqeuest_mem_addr;

    char *request_line = nullptr;
    char *request_head = nullptr;
    char *reqeust_body = nullptr;

    std::unordered_map<std::string, std::string> headers;
    uint method;
    char *http_version = nullptr;
    char *uri = nullptr;
    char *args = nullptr;
    char *url = nullptr;

    ~_mgx_http_request() {
        char *delete_res[] = {
            request_line, request_head, reqeust_body, uri
        };
        for (auto res: delete_res)
            if (res)
                delete[] res;
    }
} mgx_http_request_t, *pmgx_http_request_t;

typedef struct _mgx_http_response {

} mgx_http_response_t, *pmgx_http_response_t;

class Mgx_http_socket : public Mgx_socket
{
public:
    Mgx_http_socket();
    virtual ~Mgx_http_socket();

    virtual bool init();
    virtual void th_msg_process_func(char *buf);
    virtual void _wait_request_handler(pmgx_conn_t c);

    int http_read_request_line(pmgx_conn_t c, pmgx_http_request_t phttp_req);
    int http_parse_request_line(pmgx_http_request_t phttp_req);
    int http_read_request_head(pmgx_conn_t c, pmgx_http_request_t phttp_req);
    int http_parse_request_head(pmgx_http_request_t phttp_req);
    int http_read_request_body(pmgx_conn_t c, pmgx_http_request_t phttp_req);

    int http_write_response(pmgx_http_response_t phttp_res);
};



#endif