#include "whs.h"
#include "whs-internal.h"
#include "fmt/format.h"

#ifdef ENABLE_LIBUV
#include <uv.h>
#endif

namespace wu = whs::utils;
using namespace whs;
using std::string;

whs::logger::Logger* whs::logger::whsLogger = nullptr;

namespace
{
    /**
     * @brief MergeDefaultCommonHeaders: merge some common headers shared between requests
     *
     */
    class MergeDefaultCommonHeaders : public Middleware
    {
    public:
        virtual bool operator()(Request&, Response& res) const THROWS override
        {
            using std::to_string;

            std::string now;
            wu::format_time(now);
            static constexpr char server[] = "whs/" WHS_VERSION;

            res.addHeader(wu::CommonHeader::Server, server);
            res.addHeader(wu::CommonHeader::XPoweredBy, server);

            res.addHeaderIfNotExists(wu::CommonHeader::CacheControl, "no-store");
            res.addHeader(wu::CommonHeader::Date, now);
            return true;
        }
    };
    /**
     * @brief NotFoundHandler: default NotFound-HttpStatus handler
     *
     */
    class NotFoundHandler : public Middleware
    {
    public:
        virtual bool operator()(Request& req, Response& resp) const THROWS override
        {
            string&& r = fmt::format("route to {} not found.", req.getBaseURL());
            resp.setBody(utils::dup_memory(r.c_str(), r.length()), r.length());
            resp.status(HTTP_STATUS_NOT_FOUND);
            resp.addHeader(whs::utils::CommonHeader::ContentType, "text/plain");
            return true;
        }
    };
#ifdef ENABLE_EXCEPTIONS
    class SystemErrorHandler : public Middleware
    {
    public:
        virtual bool operator()(Request& req, Response& resp) const THROWS override
        {
            string&& r = fmt::format("System fatal error {}.", req.getBaseURL());
            resp.setBody(utils::dup_memory(r.c_str(), r.length()), r.length());
            resp.status(HTTP_STATUS_INTERNAL_SERVER_ERROR);
            resp.addHeader(whs::utils::CommonHeader::ContentType, "text/plain");
            return true;
        }
    };
#endif
}  // namespace

Whs::Whs()
{
    notFound = nullptr;
#ifdef ENABLE_EXCEPTIONS
    systemError = nullptr;
#endif
    staticFile = nullptr;
}

Whs::Whs(route::HttpRouter&& router) : Whs()
{
    route.swap(router);
}

whs::TcpWhs::~TcpWhs()
{
    delete _sock;
}

bool whs::TcpWhs::init_sock()
{
    _sock = new sockaddr_in;
#ifdef ENABLE_LIBUV
    uv_ip4_addr(_host.c_str(), _port, _sock);
#endif
    return true;
}

bool TcpWhs::setup()
{
    return init_sock() && _setup() && init();
}

void Whs::processing_request(RestfulHttpRequest& req, RestfulHttpResponse& resp)
{
#ifdef ENABLE_EXCEPTIONS
    try {
        auto status = before.feed(req, resp);

        try {
            if (status) {
                route.operator()(req, resp);
            }
        } catch (const route::NotFoundException& e) {
            notFound->operator()(req, resp);
        }
        after.feed(req, resp);
    } catch (const HttpException& he) {
        char* buf;
        size_t size;
        he.buildResponse(buf, size);
        resp.setBody(buf, size);
        resp.status(he.getStatusCode());
    }
#else
    if (before.feed(req, resp)) {
        if (!route.operator()(req, resp)) {
            notFound->operator()(req, resp);
        }
        after.feed(req, resp);
    }
#endif
}

bool Whs::start()
{
    if (notFound == nullptr) {
        setNotFoundHandler<NotFoundHandler>();
    }
    after.addMiddleware<MergeDefaultCommonHeaders>();
#ifdef ENABLE_EXCEPTIONS
    systemError = new SystemErrorHandler;
#endif
    if (staticFile != nullptr) {
        before.addMiddleware(staticFile);
        reinterpret_cast<StaticFileServer*>(staticFile.get())->start();
    }
    if (logger::whsLogger != nullptr) {
        atexit([]() {
            if (logger::whsLogger) {
                delete logger::whsLogger;
            }
        });
    }
    return _start();
}

bool Whs::enable_static_file(const std::string& prefix, const std::string& local)
{
    auto sf = new StaticFileServer(prefix, local);
    this->staticFile.reset(sf);
    return true;
}