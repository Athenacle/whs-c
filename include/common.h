
#ifndef WHS_COMMON_H_
#define WHS_COMMON_H_

#ifdef ENABLE_LIBUV
struct uv_loop_s;
struct uv_async_s;
struct uv_tcp_s;
struct uv_buf_t;
struct uv_stream_s;
#endif

#include <whs/whs_config.h>

#ifdef __GNUG__
#include <cxxabi.h>
#define WHS_HAVE_GNUG_CXX_ABI
#endif
#define THROWS noexcept(false)

struct sockaddr_in;

namespace whs
{
    class HttpParser;
    class TcpServer;
    class Pipeline;
    class Client;

    namespace logger
    {
        class Logger;
        extern Logger *whsLogger;

    }  // namespace logger

    namespace utils
    {
        struct _base {
        };

        inline const char *demangle(const char *what)
        {
#ifdef WHS_HAVE_GNUG_CXX_ABI
            int status = 0;
            return abi::__cxa_demangle(what, 0, 0, &status);
#endif
            return nullptr;
        }

        class noncopyable : _base
        {
        private:
            noncopyable(const noncopyable &) = delete;
            noncopyable &operator=(const noncopyable &) = delete;

        protected:
            constexpr noncopyable() = default;
            ~noncopyable() = default;
        };

        /**
         * @brief regular expression type
         * The regular expression grammar is PCRE2(https://www.pcre.org/)
         */
        class regex;

        class mutex;

        enum class CommonHeader;

    }  // namespace utils

    class HttpException;

    class RestfulHttpRequest;

    class RestfulHttpResponse;

    using Request = RestfulHttpRequest;
    using Response = RestfulHttpResponse;

    class Middleware;

    using MiddlewarePointer = Middleware *;

    class PipelineBuilder;

    class Pipeline;

    namespace route
    {
        class HttpRouter;
    }  // namespace route

    class Whs;

    class TcpWhs;

    class ResponseBodyType
    {
    public:
        virtual void toBytes(char *&, size_t &) const = 0;
        virtual ~ResponseBodyType() {}
    };

#ifdef ENABLE_LIBUV
    class LibuvWhs;
#endif
}  // namespace whs

#endif