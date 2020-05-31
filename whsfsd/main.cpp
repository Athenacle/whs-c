#include "whs.h"
#include "whsfsd.h"

#include <unistd.h>

#include "spdlog/spdlog.h"

using namespace whs;
using namespace std;
using whs::route::HttpRouter;

class TestMiddleware : Middleware
{
    virtual bool operator()(RestfulHttpRequest &, RestfulHttpResponse &) const THROWS override
    {
        return true;
    };
};

#include <pthread.h>

void *run(void *wh)
{
    char buf[16];
    for (size_t i = 20; i > 0; i--) {
        sleep(1);
        int c = snprintf(buf, 16, "%ld\n", i);
        write(STDOUT_FILENO, buf, c);
    }
    reinterpret_cast<whs::TcpWhs *>(wh)->stop();

    return nullptr;
}
namespace
{
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
    route::HttpRouteBuilder builder;

    builder.use(HTTP_GET, "/apple", [](Request &, Response &resp) -> bool {
        static char buf[] = "hello world";
        resp.setBody(dup_memory(buf, sizeof(buf) - 1), sizeof(buf) - 1);
        return true;
    });

    route::HttpRouter r(std::move(builder));

    whs::LibuvWhs whs(std::move(r), "0.0.0.0", 12345u);
    whs::logger::setLogger<NLogger>();
    whs.setup();
    whs.enable_static_file("/", "/tmp/html");
    pthread_t th;

    pthread_create(&th, nullptr, run, &whs);

    whs.start();

    pthread_join(th, nullptr);
}