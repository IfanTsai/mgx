#include "mgx_http_socket.h"
#include "mgx_thread_pool.h"
#include "mgx_string.h"

extern Mgx_th_pool g_mgx_th_pool;

Mgx_http_socket::Mgx_http_socket()
{

}

Mgx_http_socket::~Mgx_http_socket()
{

}

bool Mgx_http_socket::init()
{
    // ...
    return Mgx_socket::init();
}

void Mgx_http_socket::_wait_request_handler(pmgx_conn_t c)
{
    mgx_http_request_t http_req;
    ssize_t recv_size = http_read_request_line(c, &http_req);
    if (recv_size <= 0)
        return;

    http_parse_request_line(&http_req);

    recv_size = http_read_request_head(c, &http_req);
    if (recv_size <= 0)
        return;

    http_parse_request_head(&http_req);

    http_read_request_body(c, &http_req);

    if (http_req.method == METHOD_GET) {
        char *msg = new char[strlen(http_req.uri) + 1];
        strcpy(msg, http_req.uri);
        g_mgx_th_pool.insert_msg_to_queue_and_signal(msg);
    }
}

void Mgx_http_socket::th_msg_process_func(char *buf)
{
    mgx_log(MGX_LOG_DEBUG, "receive get: %s", buf);
}

int Mgx_http_socket::http_read_request_line(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{
    ssize_t recv_size;
    int len = 0;
    char ch;
    for (;;) {
        recv_size = recv_process(c, &ch, 1);
        if (recv_size <= 0) 
            return recv_size;
        if (ch == CR) {
            recv_size = recv_process(c, &ch, 1);
            if (recv_size <= 0) 
                return recv_size;
            if (ch == LF)
                break;
            else 
                phttp_req->recv_buf[len++] = CR;
        }
        phttp_req->recv_buf[len++] = ch;
    }

    char *tmp_buf = new char[len + 1];
    memcpy(tmp_buf, phttp_req->recv_buf, len);
    bzero(phttp_req->recv_buf, len);
    phttp_req->request_line = tmp_buf;

    return len;
}

int Mgx_http_socket::http_parse_request_line(pmgx_http_request_t phttp_req)
{
    char *request_line = phttp_req->request_line;
    if (!strncmp(request_line, "GET", strlen("GET"))) {
        phttp_req->method = METHOD_GET;
        char *p = strstr(request_line, "HTTP/");
        phttp_req->http_version = p;

        int url_len = p - request_line - strlen("GET") - 2;
        char *uri = new char[url_len + 1];
        strncpy(uri, request_line + strlen("GET") + 1, url_len);
        phttp_req->uri = uri;
    } 

    return 0;
}

int Mgx_http_socket::http_read_request_head(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{
    ssize_t recv_size;
    int len = 0;
    char ch;
    for (;;) {
        recv_size = recv_process(c, &ch, 1);
        if (recv_size <= 0) 
            return recv_size;
        if (ch == CR) {
            recv_size = recv_process(c, &ch, 1);
            if (recv_size <= 0)
                return recv_size;
            if (ch == LF) {
                char buf[2] = { 0 };
                recv(c->fd, buf, 2, MSG_PEEK);
                if (!strcmp(buf, CRLF)) {
                    recv(c->fd, buf, 2, 0);
                    break;
                }
            } else {
                phttp_req->recv_buf[len++] = CR;
            }
        }
        phttp_req->recv_buf[len++] = ch;
    }

    char *tmp_buf = new char[len + 1];
    memcpy(tmp_buf, phttp_req->recv_buf, len);
    bzero(phttp_req->recv_buf, len);
    phttp_req->request_head = tmp_buf;

    return len;
}

int Mgx_http_socket::http_parse_request_head(pmgx_http_request_t phttp_req)
{
    const char *delim = "\r\n";
    for (char *p = strtok(phttp_req->request_head, delim); p; p = strtok(nullptr, delim)) {
        std::string line(p);
        /* split by ':', get key and value */
        int index = line.find_first_of(':');
        std::string key = line.substr(0, index - 1);
        std::string val = line.substr(index + 1, line.size() - 1);
        
        /* put key and value in headers map */
        phttp_req->headers[strim(key)] = std::move(strim(val));
    }

    return 0;
}

int Mgx_http_socket::http_read_request_body(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{

    return 0;
}

int Mgx_http_socket::http_write_response(pmgx_http_response_t phttp_res)
{
    return 0;
}