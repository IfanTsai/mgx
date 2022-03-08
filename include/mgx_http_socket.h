#ifndef __MGX_HTTP_SOCKET_H__
#define __MGX_HTTP_SOCKET_H__

#include "mgx_socket.h"
#include "mgx_log.h"
#include <unordered_map>

#define CR    '\r'
#define LF    '\n'
#define CRLF  "\r\n"

#define METHOD_GET     0x01
#define METHOD_HEAD    0x02
#define METHOD_POST    0x03
#define METHOD_PUT     0x04
#define METHOD_DELETE  0x05
#define METHOD_TRACE   0x06
#define METHOD_OPTIONS 0x07
#define METHOD_CONNECT 0x08

#define DEFAULT_INDEX_PATH  "./html/"

typedef struct _mgx_http_request {
    char recv_buf[1024 * 8];
    char *preqeuest_mem_addr;

    char *request_line = nullptr;
    char *request_head = nullptr;
    char *request_body = nullptr;

    std::unordered_map<std::string, std::string> headers;
    uint method;
    char *http_version = nullptr;
    char *uri = nullptr;
    char *args = nullptr;
    char *url = nullptr;

    ~_mgx_http_request() {
        char *delete_res[] = {
            request_line, request_head, request_body, uri
        };
        for (auto res: delete_res)
            if (res)
                delete[] res;
    }
} mgx_http_request_t, *pmgx_http_request_t;

typedef struct _mgx_http_response {
    const char *status_line;
    std::unordered_map<std::string, std::string> headers;
    char *response_body = nullptr;
    ~_mgx_http_response() {
        if (response_body)
            delete[] response_body;
    }
} mgx_http_response_t, *pmgx_http_response_t;

class Mgx_http_socket : public Mgx_socket
{
public:
    Mgx_http_socket();
    virtual ~Mgx_http_socket();

    virtual bool init();
    virtual void th_msg_process_func(char *buf);
    virtual void _read_request_handler(pmgx_conn_t c);

private:
    std::string m_index_path;
    std::unordered_map<std::string, const char *> m_mime_types;

    ssize_t http_read_request_line(pmgx_conn_t c, pmgx_http_request_t phttp_req);
    bool    http_parse_request_line(pmgx_http_request_t phttp_req);
    ssize_t http_read_request_head(pmgx_conn_t c, pmgx_http_request_t phttp_req);
    bool    http_parse_request_head(pmgx_http_request_t phttp_req);
    ssize_t http_read_request_body(pmgx_conn_t c, pmgx_http_request_t phttp_req);

    int http_write_response(pmgx_http_response_t phttp_res, pmgx_msg_hdr_t pmsg_hdr);

    void mime_types_init();
    const char *get_mime_type(const char *extension);
    const char *get_extension(const char *path);
};

#define HTTP_STATUS_LINE(CODE) HTTP_STATUS_LINE_##CODE

#define HTTP_STATUS_LINE_200   "200 OK"
#define HTTP_STATUS_LINE_201   "201 Created"
#define HTTP_STATUS_LINE_202   "202 Acceptedd"
#define HTTP_STATUS_LINE_203   "203 Non-Authoritative Information"
#define HTTP_STATUS_LINE_204   "204 No Content"
#define HTTP_STATUS_LINE_205   "205 Reset Content"
#define HTTP_STATUS_LINE_206   "206 Partial Content"
#define HTTP_STATUS_LINE_207   "207 Multi-Status"

#define HTTP_STATUS_LINE_300   "300 Multiple Choices"
#define HTTP_STATUS_LINE_301   "301 Moved Permanently"
#define HTTP_STATUS_LINE_302   "302 Moved Temporarily"
#define HTTP_STATUS_LINE_303   "303 See Other"
#define HTTP_STATUS_LINE_304   "304 Not Modified"
#define HTTP_STATUS_LINE_305   "305 Use Proxy"
#define HTTP_STATUS_LINE_306   "306 unused"
#define HTTP_STATUS_LINE_307   "307 Temporary Redirect"
#define HTTP_STATUS_LINE_308   "308 Permanent Redirect"

#define HTTP_STATUS_LINE_400   "400 Bad Request"
#define HTTP_STATUS_LINE_401   "401 Unauthorized"
#define HTTP_STATUS_LINE_402   "402 Payment Required"
#define HTTP_STATUS_LINE_403   "403 Forbidden"
#define HTTP_STATUS_LINE_404   "404 Not Found"
#define HTTP_STATUS_LINE_405   "405 Not Allowed"
#define HTTP_STATUS_LINE_406   "406 Not Acceptable"
#define HTTP_STATUS_LINE_407   "407 Proxy Authentication Required"
#define HTTP_STATUS_LINE_408   "408 Request Time-out"
#define HTTP_STATUS_LINE_409   "409 Conflict"
#define HTTP_STATUS_LINE_410   "410 Gone"
#define HTTP_STATUS_LINE_411   "411 Length Required"
#define HTTP_STATUS_LINE_412   "412 Precondition Failed"
#define HTTP_STATUS_LINE_413   "413 Request Entity Too Large"
#define HTTP_STATUS_LINE_414   "414 Request-URI Too Large"
#define HTTP_STATUS_LINE_415   "415 Unsupported Media Type"
#define HTTP_STATUS_LINE_416   "416 Requested Range Not Satisfiable"
#define HTTP_STATUS_LINE_417   "417 Expectation Failed"
#define HTTP_STATUS_LINE_418   "418 unused"
#define HTTP_STATUS_LINE_419   "419 unused"
#define HTTP_STATUS_LINE_420   "420 unused"
#define HTTP_STATUS_LINE_421   "421 Misdirected Request"
#define HTTP_STATUS_LINE_422   "422 Unprocessable Entity"
#define HTTP_STATUS_LINE_423   "423 Locked"
#define HTTP_STATUS_LINE_424   "424 Failed Dependency"
#define HTTP_STATUS_LINE_425   "425 unused"
#define HTTP_STATUS_LINE_426   "426 Upgrade Required"
#define HTTP_STATUS_LINE_427   "427 unused"
#define HTTP_STATUS_LINE_428   "428 Precondition Required"
#define HTTP_STATUS_LINE_429   "429 Too Many Requests"

#define HTTP_STATUS_LINE_500   "500 Internal Server Error"
#define HTTP_STATUS_LINE_501   "501 Not Implemented"
#define HTTP_STATUS_LINE_502   "502 Bad Gateway"
#define HTTP_STATUS_LINE_503   "503 Service Temporarily Unavailable"
#define HTTP_STATUS_LINE_504   "504 Gateway Time-out"
#define HTTP_STATUS_LINE_505   "505 HTTP Version Not Supported"
#define HTTP_STATUS_LINE_506   "506 Variant Also Negotiates"
#define HTTP_STATUS_LINE_507   "507 Insufficient Storage"
#define HTTP_STATUS_LINE_508   "508 unused"
#define HTTP_STATUS_LINE_509   "509 unused"
#define HTTP_STATUS_LINE_510   "510 Not Extended"

#endif
