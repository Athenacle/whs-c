
#ifndef WHS_INTERNAL_H
#define WHS_INTERNAL_H

#include "config.h"

#include "whs.h"

#include <http_parser.h>
#include <cstring>
#include <cassert>
#include <unordered_map>

#ifndef UNIX_HAVE_PTHREAD_H
#include <thread>
#endif

#ifdef HAVE_LIBPCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
struct pcre2_real_code_8;
using regex_type = pcre2_real_code_8 *;
#else
#include <regex>
using regex_type = std::regex;
#endif
/**
 * @brief whs namespace
 *
 */

namespace whs
{
    class HttpParser;
    class Client;

    namespace route
    {
        using MP = MiddlewarePointer;

        struct HttpRouteEndNode : public HttpRouteNode {
            int method;
            MP func;

            virtual const MP &getRoute(Request &, const char *, const char *) const THROWS override;

            HttpRouteEndNode(const MP &p) : func(p) {}
        };

        struct HttpRouteParamNode : public HttpRouteNode {
            const std::string _paramName;
            const utils::regex *_regex;

            virtual ~HttpRouteParamNode();

            HttpRouteParamNode(const std::string &pn, utils::regex &&regex);
            virtual const MP &getRoute(Request &, const char *, const char *) const THROWS override;
        };
    }  // namespace route

    namespace utils
    {
        inline bool isParam(const std::string &s)
        {
            return s.find_first_of('{') == 0 && s.find_last_of('}') == s.length() - 1;
        }

        void format_time(std::string &);
        void format_time(const struct tm *tm, std::string &);

        bool parseParam(const std::string &, std::string &, regex &);
        bool parseQueryString(const std::string &, std::map<std::string, std::string> &);

        inline char *dup_memory(const void *buffer, size_t size)
        {
            auto ret = new char[size];
            memcpy(ret, buffer, size);
            return ret;
        }

#ifdef UNIX_HAVE_PTHREAD_H
        class thread
        {
        };
#else
        using thread = std::thread;
#endif
        /**
         * @brief mutex: mutex implementation.
         *
         */
        class mutex
        {
            void *data;

        public:
            mutex();
            ~mutex();

            /**
             * @brief lock the mutex.
             *
             * If running on pthread plateforms, this will call pthread_mutex_lock
             *
             */
            void lock();

            /**
             * @brief unlock the mutex.
             * If running on pthread plateforms, this will call pthread_mutex_unlock
             */
            void unlock();
        };

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

    class StaticFileServer : public Middleware
    {
        static constexpr uint8_t MD5_DIGEST_LENGTH = 16;
        static constexpr uint8_t ETAG_LENGTH = 32;
        struct file {
            static uint32_t major_time;
            const char *mime;
            uint32_t size;
            uint32_t last_save_time;
            char etag[ETAG_LENGTH + 1];

            time_t get_save_time() const
            {
                time_t ret = major_time;
                return ret << 32 | last_save_time;
            }
            file(const std::string &);
            void calc_etag(const std::string &);
        };
        int fd;
        utils::mutex m;

        std::string path;
        std::string prefix;
        void start_thread();

        void list_files();
        std::unordered_map<uint64_t, file> files;

    public:
        StaticFileServer(StaticFileServer &&);
        StaticFileServer(const std::string &, const std::string &);
        virtual ~StaticFileServer();

        bool start();
        void stop();

        virtual bool operator()(Request &, Response &) const THROWS override;
    };
}  // namespace whs

#endif