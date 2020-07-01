#ifndef WHS_H
#define WHS_H

#include <whs/common.h>
#include <whs/builder.h>

#include <type_traits>
#include <map>
#include <deque>
#include <vector>
#include <algorithm>
#include <memory>


#ifdef ENABLE_LIBUV
struct uv_loop_s;
struct uv_async_s;
struct uv_tcp_s;
struct uv_buf_t;
struct uv_stream_s;
#endif

struct sockaddr_in;

#ifdef HAVE_LIBPCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
struct pcre2_real_code_8;
using regex_type = pcre2_real_code_8 *;
#else
#include <regex>
using regex_type = std::regex;
#endif

namespace whs
{
    namespace logger
    {
        class Logger
        {
        public:
            virtual void info(const std::string &what) = 0;
            virtual void error(const std::string &what) = 0;
            virtual void warning(const std::string &what) = 0;
            virtual void trace(const std::string &what) = 0;
            virtual void debug(const std::string &what) = 0;
            virtual ~Logger() {}
        };
        extern Logger *whsLogger;

        template <class L, class... Args>
        auto setLogger(const Args &&... args) ->
            typename std::enable_if<std::is_base_of<Logger, L>::value
                                        && !std::is_abstract<L>::value,
                                    Logger *>::type
        {
            if (whsLogger) {
                delete whsLogger;
            }
            whsLogger = new L(std::forward<Args>(args)...);
            return whsLogger;
        }

#define BUILD_LOGGER_FUNC(name)               \
    inline void name(const std::string &what) \
    {                                         \
        if (whsLogger)                        \
            whsLogger->name(what);            \
    }
        BUILD_LOGGER_FUNC(debug)
        BUILD_LOGGER_FUNC(warning)
        BUILD_LOGGER_FUNC(info)
        BUILD_LOGGER_FUNC(error)
        BUILD_LOGGER_FUNC(trace)
#undef BUILD_LOGGER_FUNC
    }  // namespace logger

    namespace utils
    {
        /**
         * @brief regular expression type
         * The regular expression grammar is PCRE2(https://www.pcre.org/)
         */
        class regex : noncopyable
        {
            regex_type re;
#ifdef HAVE_LIBPCRE2

            void init_pcre2();

            uint32_t ob;
            uint32_t nl;
            const unsigned char *name_table;
            uint32_t namecount;
            uint32_t name_entry_size;
#endif
        public:
            struct regex_group {
                std::vector<std::string> matches;
                std::map<std::string, std::string> named_matches;
            };
            using regex_group_match = std::vector<regex_group>;
            /**
             * @brief Construct a new EMPTY regex object
             *
             */
            regex();

            regex(regex &&);

            ~regex();

            void swap(regex &other);

            bool match(const char *test) const;

            bool match(const std::string &test) const
            {
                return match(test.c_str());
            }

            bool execute(const char *test, std::vector<regex_group> &group) const;

            bool execute(const std::string &test, std::vector<regex_group> &group) const
            {
                return execute(test.c_str(), group);
            }

            /**
             * @brief compile a string in to regex.
             * As `regex::regex()' is private, user cannot construct a regex with pattern,
             * using `compile' to construct it.
             * @param pattern regular expression string
             * @return regex*
             * @retval NULL: compile failed.
             * @retval otherwise: compile success.
             */
            static regex *compile(const char *pattern);

            /**
             * @brief compile a string in to regex.
             * compile `pattern' and put it into `re'
             * @param pattern regular expression string
             * @param re output regex struct
             * @return bool
             * @retval true compile success.
             * @retval false compile false.
             */
            static bool compile(const char *pattern, regex &re);
        };


    }  // namespace utils

    class HttpException
    {
    protected:
        int _httpMethod;
        int _statusCode;
        std ::string _what;

    public:
        HttpException(const std::string &_msg) : _what(_msg) {}

        HttpException() : HttpException("HttpException") {}

        int getStatusCode() const
        {
            return _statusCode;
        }

        virtual bool buildResponse(char *&, size_t &) const = 0;

        virtual ~HttpException() {}
    };

    namespace route
    {
        class HttpRouter;
    }  // namespace route

    class Whs
    {
        friend class Client;

        Pipeline *before;
        route::HttpRouter *route;
        Pipeline *after;

        Middleware *notFound;     // 404
        Middleware *systemError;  // 500
        Middleware *staticFile;

    protected:
        Whs();

        void processing_request(Request &req, Response &resp);

    protected:
        virtual bool _start() = 0;
        virtual bool _setup() = 0;

    public:
        virtual ~Whs();

        virtual void write(Client *, char *, size_t) = 0;
        virtual bool stop() = 0;
        virtual bool init() = 0;

        void setup(PipelineBuilder *, route::HttpRouteBuilder *, PipelineBuilder *);
        void setup();
        bool start();

        bool enable_static_file(const std::string &, const std::string &);

        template <class T, class... Args>
        auto setNotFoundHandler(Args &&... args) -> EnableIfMiddleType<T, void>
        {
            if (notFound != nullptr) {
                delete notFound;
            }
            notFound = new T(std::forward<Args>(args)...);
        }
    };

    class RawWhs : public Whs
    {
    protected:
        virtual bool _start() override;
        virtual bool _setup() override;

    public:
        virtual bool stop() override;
        virtual bool init() override;

        Client *c;

    public:
        RawWhs();
        virtual ~RawWhs();

        void in(const char *, size_t);
        void out(char *, size_t &);
        void reset();
        size_t readable_size();

        virtual void write(Client *, char *, size_t) override;
    };

    class TcpWhs : public Whs
    {
        bool init_sock();

    protected:
        uint16_t _port;
        std::string _host;
        struct sockaddr_in *_sock;

        bool setup_tcp();

    public:
        TcpWhs(std::string &host, uint16_t port) : Whs(), _port(port), _host(host) {}
        virtual ~TcpWhs();
    };

#ifdef ENABLE_LIBUV
    namespace utils
    {
        void uvConnectCB(uv_stream_s *, int flag);

        void uvAsyncStopCB(uv_async_s *);

        void uvReadCB(uv_stream_s *, ssize_t, const uv_buf_t *);
    }  // namespace utils

    class LibuvWhs : public TcpWhs
    {
        friend void utils::uvAsyncStopCB(uv_async_s *);
        friend void utils::uvConnectCB(uv_stream_s *, int);
        friend void utils::uvReadCB(uv_stream_s *, ssize_t, const uv_buf_t *);

        uv_loop_s *loop;
        uv_async_s *stop_async;
        uv_tcp_s *server;
        utils::mutex *m;
        bool externalLoop;

        virtual bool _setup() override;

        void stop_uv();

        virtual void write(Client *, char *, size_t) override;

    public:
        LibuvWhs(std::string &&host, uint16_t port);
        LibuvWhs(std::string &&host, uint16_t port, uv_loop_s *);

        virtual ~LibuvWhs();

        virtual bool _start() override;
        virtual bool stop() override;
        virtual bool init() override;
    };
#endif
}  // namespace whs

#endif