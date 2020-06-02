
#include "whs-internal.h"
#include "whs/whs.h"

#ifdef ENABLE_LIBUV
#include <uv.h>
#endif

using namespace whs;

TcpWhs::~TcpWhs()
{
    delete _sock;
}

bool TcpWhs::setup_tcp()
{
    return init_sock();
}

bool whs::TcpWhs::init_sock()
{
    _sock = new sockaddr_in;
#ifdef ENABLE_LIBUV
    uv_ip4_addr(_host.c_str(), _port, _sock);
#endif
    return true;
}
