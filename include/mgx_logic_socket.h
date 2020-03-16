#if !defined(__MGX_LOGIC_SOCKET_H__)
#define __MGX_LOGIC_SOCKET_H__

#include "mgx_socket.h"

enum class PKG_TYPE {
    PING = 0,
    REGISTER,
    LOGIN,
};

class Mgx_logic_socket : public Mgx_socket
{
public:
    Mgx_logic_socket();
    virtual ~Mgx_logic_socket();

    virtual bool init();
    virtual void th_msg_process_func(char *buf);

    bool ping_handler(pmgx_msg_hdr_t pmsg_hdr, char *pkg_body, unsigned short pkg_body_size);
    bool register_handler(pmgx_msg_hdr_t pmsg_hdr, char *pkg_body, unsigned short pkg_body_size);
    bool login_handler(pmgx_msg_hdr_t pmsg_hdr, char *pkg_body, unsigned short pkg_body_size);

private:
    void send_msg_with_nobody(pmgx_msg_hdr_t pmsg_hdr, unsigned short pkg_type);
};

#endif // __MGX_LOGIC_SOCKET_H__
