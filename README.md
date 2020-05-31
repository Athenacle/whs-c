# whs-c
Simple Http Server

## Example
```cpp
#include "whs.h"

#include "spdlog/spdlog.h"

using namespace whs;
using namespace std;
using whs::route::HttpRouter;

class TestMiddleware : public Middleware
{
    virtual bool operator()(RestfulHttpRequest &, RestfulHttpResponse &) const THROWS override
    {
        return true;
    };
};

namespace
{

    inline char* dup_memory(const void* buffer, size_t size)
    {
        auto ret = new char[size];
        memcpy(ret, buffer, size);
        return ret;
    }

#define BUILD_LOG(level)                                 \
    virtual void level(const std::string &what) override \
    {                                                    \
        spdlog::level(what);                             \
    }
    class NLogger : public logger ::Logger
    {
    public:
        BUILD_LOG(info)
        BUILD_LOG(error)
        BUILD_LOG(trace)
        BUILD_LOG(debug)

        virtual void warning(const std::string &what) override
        {
            spdlog::warn(what);
        }
        NLogger()
        {
#ifndef NDEBUG
            spdlog::set_level(spdlog::level::debug);
#else
            spdlog::set_level(spdlog::level::info);
#endif
        }
    };
}  // namespace

int main()
{
    route::HttpRouter::HttpRouteBuilder builder;

    builder.use(HTTP_GET, "/apple", [](Request &, Response &resp) -> bool {
        static char buf[] = "hello world";
        resp.setBody(dup_memory(buf, sizeof(buf) - 1), sizeof(buf) - 1);
        return true;
    });

    builder.use<HTTP_GET, TestMiddleware>("/");

    route::HttpRouter r(std::move(builder));

    whs::LibuvWhs whs(std::move(r), "0.0.0.0", 12345u);
    whs::logger::setLogger<NLogger>();
    whs.setup();

    whs.start();
}
```