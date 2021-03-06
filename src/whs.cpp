#include "whs-internal.h"
#include "whs/entity.h"
#include "fmt/format.h"
#include "client.h"
#include "utils.h"

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
            if (!resp.isBodySet()) {
                string&& r = fmt::format("route to {} not found.", req.getBaseURL());
                resp.setBody(utils::dup_memory(r.c_str(), r.length()), r.length());
                resp.status(HTTP_STATUS_NOT_FOUND);
                resp.addHeader(whs::utils::CommonHeader::ContentType, "text/plain");
            }
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

Whs::~Whs()
{
    delete before;
    delete route;
    delete after;
    delete notFound;
    delete systemError;
}

Whs::Whs()
{
    route = nullptr;
    after = new Pipeline;
    before = new Pipeline;
    notFound = nullptr;
    systemError = nullptr;
    staticFile = nullptr;
}


void whs::Whs::setup(PipelineBuilder* bef, route::HttpRouteBuilder* r, PipelineBuilder* aft)
{
    if (aft != nullptr) {
        delete after;
        after = new Pipeline(*aft);
    }
    if (r != nullptr) {
        route = new route::HttpRouter(std::move(*r));
    }
    if (bef != nullptr) {
        delete before;
        before = new Pipeline(*bef);
    }
    this->setup();
}

void Whs::setup()
{
    this->_setup();
    this->init();
}

void Whs::processing_request(RestfulHttpRequest& req, RestfulHttpResponse& resp)
{
    try {
        auto status = before->feed(req, resp);
        if (!resp.isEnded()) {
            try {
                if (status) {
                    route->operator()(req, resp);
                }
            } catch (const route::NotFoundException& e) {
                notFound->operator()(req, resp);
            }
        }
        after->feed(req, resp);
    } catch (const HttpException& he) {
        char* buf;
        size_t size;
        he.buildResponse(buf, size);
        resp.setBody(buf, size);
        resp.status(he.getStatusCode());
    }
}

bool Whs::start()
{
    if (notFound == nullptr) {
        setNotFoundHandler<NotFoundHandler>();
    }
    after->addMiddleware<MergeDefaultCommonHeaders>();
    systemError = new SystemErrorHandler;
    if (staticFile != nullptr) {
        before->addMiddleware(staticFile);
        reinterpret_cast<StaticFileServer*>(staticFile)->start();
    }
    if (logger::whsLogger != nullptr) {
        atexit([]() {
            if (logger::whsLogger) {
                delete logger::whsLogger;
            }
        });
    }
    if (route == nullptr) {
        route::HttpRouteBuilder b;
        this->route = new route::HttpRouter(std::move(b));
    }
    return _start();
}

bool Whs::enable_static_file(const std::string& prefix, const std::string& local)
{
    auto sf = new StaticFileServer(prefix, local);
    this->staticFile = sf;
    return true;
}

RawWhs::RawWhs()
{
    auto mb = new whsutils::MemoryBuffer();
    c = new Client(this, mb);
}

RawWhs::~RawWhs()
{
    auto mb = reinterpret_cast<whsutils::MemoryBuffer*>(c->get_data());
    delete mb;
    delete c;
}

void RawWhs::write(Client*, char* buf, size_t s)
{
    auto mb = reinterpret_cast<whsutils::MemoryBuffer*>(c->get_data());
    mb->write(buf, s);
    delete[] buf;
}

void RawWhs::in(const char* buf, size_t s)
{
    c->read_from_network(s, buf);
}

void RawWhs::out(char* buf, size_t& s)
{
    auto mb = reinterpret_cast<whsutils::MemoryBuffer*>(c->get_data());
    auto rs = mb->stored();
    auto r = mb->read(buf, rs);
    s = r;
}

void RawWhs::reset()
{
    c->reset();
    auto mb = reinterpret_cast<whsutils::MemoryBuffer*>(c->get_data());
    mb->clear();
}

size_t RawWhs::readable_size()
{
    auto mb = reinterpret_cast<whsutils::MemoryBuffer*>(c->get_data());
    return mb->stored();
}

bool RawWhs::_start()
{
    return true;
}
bool RawWhs::_setup()
{
    return true;
}
bool RawWhs::stop()
{
    return true;
}
bool RawWhs::init()
{
    return true;
}