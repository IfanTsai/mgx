#include "mgx_http_socket.h"
#include "mgx_thread_pool.h"
#include "mgx_string.h"
#include "mgx_conf.h"
#include "mgx_file.h"

extern Mgx_th_pool g_mgx_th_pool;

Mgx_http_socket::Mgx_http_socket()
{
    mime_types_init();
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();
    m_index_path = mgx_conf->get_string(CONFIG_IndexPath, DEFAULT_INDEX_PATH);
}

Mgx_http_socket::~Mgx_http_socket()
{

}

bool Mgx_http_socket::init()
{
    // ...

    return Mgx_socket::init();
}

void Mgx_http_socket::_read_request_handler(pmgx_conn_t c)
{
    mgx_http_request_t http_req;

    if (http_read_request_line(c, &http_req) <= 0)
        return;

    if (!http_parse_request_line(&http_req))
        return;

    if (http_read_request_head(c, &http_req) <= 0)
        return;

    if (!http_parse_request_head(&http_req))
        return;

    if (http_read_request_body(c, &http_req) <= 0)
        return;

    uint tmp_buf_size = m_msg_hdr_size + 1;
    if (http_req.request_body)
        tmp_buf_size += strlen(http_req.request_body);

    char *tmp_buf = new char[tmp_buf_size]();
    c->precv_mem_addr = tmp_buf;

    pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)tmp_buf;
    pmsg_hdr->pconn = c;
    pmsg_hdr->cur_seq = c->m_cur_seq;
    pmsg_hdr->uri = new char[strlen(http_req.uri) + 1]();
    strcpy(pmsg_hdr->uri, http_req.uri);

    if (http_req.method == METHOD_GET) {
        pmsg_hdr->http_method = METHOD_GET;
    } else if (http_req.method == METHOD_POST) {
        pmsg_hdr->http_method = METHOD_POST;
        tmp_buf += m_msg_hdr_size;
        strcpy(tmp_buf, http_req.request_body);
    } else {

    }

    g_mgx_th_pool.insert_msg_to_queue_and_signal(c->precv_mem_addr);

    c->precv_mem_addr = nullptr;
}

void Mgx_http_socket::th_msg_process_func(char *buf)
{
    pmgx_msg_hdr_t pmsg_hdr = (pmgx_msg_hdr_t)buf;
    const char *request_body = buf + m_msg_hdr_size;
    char *uri = pmsg_hdr->uri;
    int fd = -1;

    mgx_http_response_t http_res;
    http_res.headers["Server"] = "Mgx http server";
    http_res.headers["Connection"] = "keep-alive";
    if (pmsg_hdr->http_method == METHOD_GET) {
        std::string static_file_path = m_index_path + uri;
        if (!strcmp(uri, "/"))
            static_file_path += "index.html";

        fd = open(static_file_path.c_str(), O_RDONLY);
        if (fd >= 0) {
            ssize_t file_size = get_file_size(fd);
            if (file_size < 0) {
                mgx_log(MGX_LOG_ERR, "get file %s size error: %s", static_file_path.c_str(), strerror(errno));
                goto out;
            }

            http_res.status_line = "HTTP/1.1 " HTTP_STATUS_LINE(200);
            http_res.headers["Content-Type"] = get_mime_type(get_extension(uri));
            http_res.headers["Content-Length"] = std::to_string(file_size);
            http_res.response_body = new char[file_size + 1]();
            if (must_read(fd, http_res.response_body, file_size) < 0) {
                mgx_log(MGX_LOG_ERR, "read file %s error: %s", static_file_path.c_str(), strerror(errno));
                goto out;
            }
        } else {
            std::string res_body;
            if (strcmp(uri, "/favicon.ico"))
                read_file_all("./http/html/404.html", res_body);

            http_res.status_line = "HTTP/1.1 " HTTP_STATUS_LINE(404);
            http_res.headers["Content-Type"] = "text/html; charset=UTF-8";
            http_res.headers["Content-Length"] = std::to_string(res_body.size());
            http_res.response_body = new char[res_body.size() + 1]();
            memcpy(http_res.response_body, res_body.c_str(), res_body.size());
        }
    } else if (pmsg_hdr->http_method == METHOD_POST) {
        mgx_log(MGX_LOG_DEBUG, "request_body: %s", request_body);
        std::string res_body = request_body;  /* echo for test */

        http_res.status_line = "HTTP/1.1 " HTTP_STATUS_LINE(200);
        http_res.headers["Content-Type"] = get_mime_type(".json");
        http_res.headers["Content-Length"] = std::to_string(res_body.size()).c_str();
        http_res.response_body = new char[res_body.size() + 1]();
        memcpy(http_res.response_body, res_body.c_str(), res_body.size());
    }

    http_write_response(&http_res, pmsg_hdr);

out:
    if (fd >= 0)
        close(fd);
    delete[] uri;
}

ssize_t Mgx_http_socket::http_read_request_line(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{
    ssize_t len = 0;
    char ch;
    for (;;) {
        ssize_t recv_size = recv_process(c, &ch, 1);
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

bool Mgx_http_socket::http_parse_request_line(pmgx_http_request_t phttp_req)
{
    char *request_line = phttp_req->request_line;

    char method_str[16] = { 0 };
    if (!strncmp(request_line, "GET", strlen("GET"))) {
        strcpy(method_str, "GET");
        phttp_req->method = METHOD_GET;
    } else if (!strncmp(request_line, "POST", strlen("POST"))) {
        strcpy(method_str, "POST");
        phttp_req->method = METHOD_POST;
    } else {
        return false;
    }

    char *p = strstr(request_line, "HTTP/");
    phttp_req->http_version = p;

    int url_len = p - request_line - strlen(method_str) - 2;
    char *uri = new char[url_len + 1]();
    strncpy(uri, request_line + strlen(method_str) + 1, url_len);
    phttp_req->uri = uri;

    return true;
}

ssize_t Mgx_http_socket::http_read_request_head(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{
    ssize_t len = 0;
    char ch;
    for (;;) {
        ssize_t recv_size = recv_process(c, &ch, 1);
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

bool Mgx_http_socket::http_parse_request_head(pmgx_http_request_t phttp_req)
{
    const char *delim = CRLF;
    for (char *p = strtok(phttp_req->request_head, delim); p; p = strtok(nullptr, delim)) {
        std::string line(p);
        /* split by ':', get key and value */
        int index = line.find_first_of(':');
        std::string key = line.substr(0, index);
        std::string val = line.substr(index + 1, line.size() - key.size());

        /* put key and value in headers map */
        phttp_req->headers[strim(key)] = std::move(strim(val));
    }

    return true;
}

ssize_t Mgx_http_socket::http_read_request_body(pmgx_conn_t c, pmgx_http_request_t phttp_req)
{
    if (!phttp_req->headers.count("Content-Length"))
        return 1;

    uint content_len = stoi(phttp_req->headers["Content-Length"]);
    mgx_log(MGX_LOG_DEBUG, "Content-Length: %d", content_len);

    phttp_req->request_body = new char[content_len + 1]();

    ssize_t off = 0;
    while (off < content_len) {
        ssize_t recv_size = recv_process(c, phttp_req->request_body + off, content_len - off);
        if (recv_size <= 0)
            return recv_size;
        off += recv_size;
    }

    return content_len;
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

inline void Mgx_http_socket::mime_types_init()
{
    m_mime_types = {
        { ".html", "text/html" },
        { ".htm",  "text/html" },
        { ".xml",  "text/xml" },
        { ".txt",  "text/plain" },
        { ".css",  "text/css" },
        { ".js",   "application/javascript" },
        { ".json", "application/json" },
        { ".zip",  "application/zip" },
        { ".jpeg", "image/jpeg" },
        { ".jpg",  "image/jpeg" },
        { ".png",  "image/png" },
        { ".gif",  "image/gif" },
        { ".ico",  "image/x-icon" },
        { ".bmp",  "image/x-ms-bmp" },
    };
}

inline const char *Mgx_http_socket::get_mime_type(const char *extension)
{
    return m_mime_types[extension];
}

const char *Mgx_http_socket::get_extension(const char *path)
{
    const char *p = path + strlen(path);
    while (*p != '.' && p != path)
        p--;

    return p != path ? p : ".html";
}
