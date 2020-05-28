
#ifndef WHS_CLIENT_H
#define WHS_CLIENT_H
#include "whs.h"
#include "whs-internal.h"

#include "parser.h"


#include <http_parser.h>

#ifdef ENABLE_LIBUV
#include <uv.h>
#endif

using namespace whs;
using whs::TcpServer;

namespace whs
{
    class Client
    {
        friend class HttpParser;

        HttpParser parser;

    protected:
        void *data;
        Whs *whs;

        void processing_request(Request &, Response &);

    public:
        void write_response(Response &);
        void read_from_network(ssize_t, char *);
        bool connection_should_close()
        {
            return parser.shouldCloseConnection();
        }
        Whs *get_whs() const
        {
            return whs;
        }
        void *get_data()
        {
            return data;
        }
        ~Client(){};
        Client(Whs *me) : parser(this), whs(me) {}
        Client(Whs *me, void *d) : parser(this), data(d), whs(me) {}
    };
}  // namespace whs

#endif