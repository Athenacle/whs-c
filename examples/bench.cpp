#define ENABLE_LIBUV
#include "whs/whs.h"
#include "whs/builder.h"
#include "whs/entity.h"

#include <unistd.h>
#include <http_parser.h>
#include <cstring>
#include <signal.h>

using namespace whs;
using namespace std;
using whs::route::HttpRouter;

extern whs::LibuvWhs *w;

inline char *dup_memory(const void *buffer, size_t size)
{
    auto ret = new char[size];
    memcpy(ret, buffer, size);
    return ret;
}

class TestMiddleware : public Middleware
{
    virtual bool operator()(RestfulHttpRequest &, RestfulHttpResponse &resp) const THROWS override
    {
        const char ret[] = "Hello World.";
        resp.setBody(dup_memory(ret, sizeof(ret)), sizeof(ret));
        resp.addHeader(utils::CommonHeader::ContentType, "text/plain");
        return true;
    };
};

void parent_sigint(int)
{
    w->stop();
}

using sigfunc = void (*)(int);

void setup_sig(int sig, sigfunc func)
{
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(sig, &act, &oact);
}

whs::LibuvWhs *w = nullptr;

int main()
{
    route::HttpRouteBuilder builder;

    builder.use<HTTP_GET, TestMiddleware>("/bench");

    w = new LibuvWhs("0.0.0.0", 12345u);
    w->setup(nullptr, &builder, nullptr);
    setup_sig(SIGINT, parent_sigint);
    w->start();
    delete w;
}