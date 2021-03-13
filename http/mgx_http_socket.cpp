#include "mgx_http_socket.h"
#include "mgx_thread_pool.h"
#include "mgx_string.h"
#include "mgx_conf.h"
#include <fstream>
#include <sstream>

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
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    index_path = mgx_conf->get_string(CONFIG_IndexPath, DEFAULT_INDEX_PATH);

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
        char *tmp_buf = new char[m_msg_hdr_size + strlen(http_req.uri) + 1]();
        c->precv_mem_addr = tmp_buf;

        pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)tmp_buf;
        pmsg_hdr->pconn = c;
        pmsg_hdr->cur_seq = c->m_cur_seq;
        pmsg_hdr->http_method = METHOD_GET;
        tmp_buf += m_msg_hdr_size;
        strcpy(tmp_buf, http_req.uri);

        g_mgx_th_pool.insert_msg_to_queue_and_signal(c->precv_mem_addr);
    }
}

void Mgx_http_socket::th_msg_process_func(char *buf)
{
    mgx_log(MGX_LOG_DEBUG, "receive get: %s", buf + m_msg_hdr_size);
    pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)buf;
    buf += m_msg_hdr_size;

    mgx_http_response_t http_res;
    http_res.headers["Server"] = "Mgx http server";
    if (pmsg_hdr->http_method == METHOD_GET) {
        std::string static_file_path = index_path + buf;
        if (!strcmp(buf, "/"))
            static_file_path += "index.html";

        mgx_log(MGX_LOG_DEBUG, "static_file_path: %s", static_file_path.c_str());
        if (!access(static_file_path.c_str(), F_OK)) {
            std::string res_body;
            read_file_all(static_file_path.c_str(), res_body);

            http_res.status_line = "HTTP/1.1 " HTTP_STATUS_LINE(200);
            if (strstr(buf, ".css"))
                http_res.headers["Content-Type"] = "text/css";
            else if (strstr(buf, ".js"))
                http_res.headers["Content-Type"] = "application/javascript";
            else if (strstr(buf, ".jpeg") || strstr(buf, ".jpg"))
                http_res.headers["Content-Type"] = "image/jpeg";
            else
                http_res.headers["Content-Type"] = "text/html; charset=UTF-8";

            http_res.headers["Connection"] = "keep-alive";
            http_res.headers["Content-Length"] = std::to_string(res_body.size()).c_str();
            http_res.response_body = new char[res_body.size() + 1]();
            memcpy(http_res.response_body, res_body.c_str(), res_body.size());
        } else {
            std::string res_body;
            if (strcmp(buf, "/favicon.ico"))
                read_file_all("./http/html/404.html", res_body);

            http_res.status_line = "HTTP/1.1 " HTTP_STATUS_LINE(404);
            http_res.headers["Content-Type"] = "text/html; charset=UTF-8";
            http_res.headers["Connection"] = "keep-alive";
            http_res.headers["Content-Length"] = std::to_string(res_body.size()).c_str();
            http_res.response_body = new char[res_body.size() + 1]();
            memcpy(http_res.response_body, res_body.c_str(), res_body.size());
        }
    }

    http_write_response(&http_res, pmsg_hdr);
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

    char *tmp_buf = new char[len + 1]();
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
        char *uri = new char[url_len + 1]();
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

    char *tmp_buf = new char[len + 1]();
    memcpy(tmp_buf, phttp_req->recv_buf, len);
    bzero(phttp_req->recv_buf, len);
    phttp_req->request_head = tmp_buf;

    return len;
}

int Mgx_http_socket::http_parse_request_head(pmgx_http_request_t phttp_req)
{
    const char *delim = CRLF;
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

int Mgx_http_socket::http_write_response(pmgx_http_response_t phttp_res, pmgx_msg_hdr_t pmsg_hdr)
{
    std::string response = phttp_res->status_line;
    if (response.compare(response.size() - 2 - 1, 2, CRLF))
        response += CRLF;

    for (auto it = phttp_res->headers.begin(); it != phttp_res->headers.end(); it++) {
        std::string header = it->first + ": " + it->second + CRLF;
        response += header;
    }
    response += CRLF;

    response += phttp_res->response_body;

    char *psend_buf = new char[m_msg_hdr_size + response.size() + 1]();
    memcpy(psend_buf, pmsg_hdr, m_msg_hdr_size);
    memcpy(psend_buf + m_msg_hdr_size, response.c_str(), response.size());

    send_msg(psend_buf);

    return 0;
}

int Mgx_http_socket::read_file_all(const char *file_path, std::string &buf)
{
    std::ifstream is(file_path);
    if (!is.is_open())
        return -1;

    std::ostringstream ss;
    std::string tmp;
    while (std::getline(is, tmp))
        ss << tmp << std::endl;

    is.close();

    buf = ss.str();

    return 0;
}
