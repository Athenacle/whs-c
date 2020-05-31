
#include "whs.h"
#ifdef ENABLE_LIBUV

#include <functional>
#include <uv.h>
#include "client.h"
#include "whs-internal.h"
#include "fmt/format.h"

using namespace whs;
using namespace whs::logger;

using uv = whs::LibuvWhs;
using ust = uv_stream_t;
using utt = uv_tcp_t;


namespace
{
    struct two {
        LibuvWhs *server;
        Client *client;
        uv_tcp_t *tcp;
    };
    void uvAllocCB(uv_handle_t *, size_t, uv_buf_t *buf)
    {
        buf->len = 512;
        buf->base = new char[buf->len];
    }
    void uvCloseCB(uv_handle_t *h)
    {
        auto twos = reinterpret_cast<two *>(h->data);
        delete twos->client;
        delete twos;
        delete reinterpret_cast<uv_tcp_t *>(h);
    }
}  // namespace

namespace whs::utils
{
    void uvReadCB(uv_stream_s *client, ssize_t nread, const uv_buf_t *buf)
    {
        auto twos = reinterpret_cast<two *>(client->data);
        debug(fmt::format("whs-uv: [read] on read cb. read size: {}", nread));
        if (nread < 0) {
            if (nread == UV_EOF) {
                debug("whs-uv: [read] EOF");
            } else if (nread == UV_ECONNRESET) {
                debug("whs-uv: [read] Connection Reset");
            } else {
                error(fmt ::format("whs-uv: [read] failed: {}", uv_err_name(nread)));
            }
            uv_read_stop(client);
            uv_close((uv_handle_t *)client, uvCloseCB);
        } else if (nread == 0) {
            // empty body
        } else {
            twos->client->read_from_network(nread, buf->base);
        }
        delete[] buf->base;
    }

    void uvConnectCB(uv_stream_s *server, int flag)
    {
        auto p = reinterpret_cast<uv *>(server->data);

        if (flag < 0) {
            warning(fmt::format("whs-uv: [connect] new connection error {}", uv_strerror(flag)));
            return;
        }
        auto client = new uv_tcp_t;
        uv_tcp_init(p->loop, client);

        auto twos = new two;
        twos->server = p;
        auto c = new Client(p, twos);
        twos->client = c;
        twos->tcp = client;
        client->data = twos;

        if (auto err = uv_accept(server, reinterpret_cast<ust *>(client)) == 0) {
            uv_read_start(reinterpret_cast<ust *>(client), uvAllocCB, uvReadCB);
        } else {
            warning(fmt::format("whs-uv: [accept] error: {}", uv_strerror(err)));
            delete client;
        }

        debug(fmt::format("whs-uv: on connect cb. flag {}", flag));
    }

    void uvAsyncStopCB(uv_async_t *async)
    {
        reinterpret_cast<uv *>(async->data)->stop_uv();
    }

}  // namespace whs::utils

bool uv::_setup()
{
    if (!externalLoop) {
        uv_loop_init(loop);
    }
    int status = uv_tcp_init(loop, server);
    if (status != 0) {
        error(fmt::format("uv_tcp_init failed: {}", uv_strerror(status)));
        return false;
    }
    status = uv_tcp_init(loop, server);
    if (status != 0) {
        error(fmt::format("uv_tcp_init on server socket failed: {}", uv_strerror(status)));
        return false;
    }
    debug("whs: libuv backend setup success");
    uv_async_init(loop, stop_async, utils::uvAsyncStopCB);
    return true;
}

bool uv::init()
{
    int status = uv_tcp_bind(server, reinterpret_cast<struct sockaddr *>(_sock), 0);
    if (!status) {
        debug("whs: libuv backend bind success");
        status = uv_listen(reinterpret_cast<uv_stream_t *>(server), 0, utils::uvConnectCB);
        if (!status) {
            info(fmt::format("whs: libuv backend is listening on {}:{}", _host, _port));
        } else {
            error(fmt::format("whs: libuv backend listen on {}:{} failed: {}",
                              _host,
                              _port,
                              uv_strerror(status)));
        }
    } else {
        error(fmt::format(
            "whs: libuv backend bind on {}:{} failed: {}", _host, _port, uv_strerror(errno)));
    }
    return status;
}

void uv::stop_uv()
{
    m->lock();
    uv_read_stop(reinterpret_cast<ust *>(server));
    if (!externalLoop) {
        uv_stop(loop);
        uv_walk(
            loop, [](uv_handle_t *h, void *) { uv_close((uv_handle_t *)h, nullptr); }, nullptr);
        uv_loop_close(loop);
    }
    m->unlock();
    info("whs: libuv backend stopped.");
}

bool uv::stop()
{
    uv_async_send(stop_async);
    return true;
}

bool uv::_start()
{
    debug("whs: libuv backend start.");
    uv_run(loop, UV_RUN_DEFAULT);
    m->lock();
    m->unlock();

    return true;
}

uv::LibuvWhs(route::HttpRouter &&router, std::string &&host, uint16_t port, uv_loop_s *l)
    : TcpWhs(std::move(router), host, port)
{
    loop = l;
    server = new uv_tcp_s;
    stop_async = new uv_async_t;
    stop_async->data = this;
    server->data = this;
    m = new utils::mutex;
    externalLoop = true;
}

uv::LibuvWhs(route::HttpRouter &&router, std::string &&host, uint16_t port)
    : LibuvWhs(std::move(router), std::move(host), port, new uv_loop_s)
{
    externalLoop = false;
}

uv::~LibuvWhs()
{
    delete m;
    delete stop_async;
    delete server;
    if (!externalLoop) {
        delete loop;
    }
}

using thr = std::tuple<Client *, uv_buf_t *>;

void uv::write(Client *c, char *buf, size_t size)
{
    auto twos = reinterpret_cast<two *>(c->get_data());
    auto tcp = twos->tcp;
    auto w = new uv_write_t;
    auto uvbuf = new uv_buf_t;
    uvbuf->base = buf;
    uvbuf->len = size;
    auto b = new thr;
    auto pair = std::make_tuple(c, uvbuf);
    b->swap(pair);
    w->data = b;
    uv_write(w, reinterpret_cast<ust *>(tcp), uvbuf, 1, [](uv_write_t *req, int) {
        auto d = reinterpret_cast<thr *>(req->data);
        thr tmp;
        d->swap(tmp);
        auto c = std::get<0>(tmp);
        auto buf = std::get<1>(tmp);
        delete[] buf->base;
        delete buf;
        delete req;
        delete d;
        if (c->connection_should_close()) {
            auto twos = reinterpret_cast<two *>(c->get_data());
            auto tcp = twos->tcp;
            auto shutdown = new uv_shutdown_t;
            shutdown->data = tcp;
            uv_shutdown(shutdown, reinterpret_cast<ust *>(tcp), [](uv_shutdown_t *shut, int) {
                auto tcp = reinterpret_cast<uv_handle_t *>(shut->data);
                uv_close(tcp, uvCloseCB);
                delete shut;
            });
        }
    });
}

#endif