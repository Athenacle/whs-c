#ifndef WHS_ENTITY_H_
#define WHS_ENTITY_H_

#include <whs/common.h>

#include <string>
#include <map>

using Map = std::map<std::string, std::string>;

namespace whs
{
    namespace utils
    {
        enum class CommonHeader {
            Host,
            Connection,
            CacheControl,
            UserAgent,
            Accept,
            AcceptEncoding,
            AcceptLanguage,
            ContentEncoding,
            LastModified,
            Etag,
            ContentType,
            ContentLength,
            Date,
            Expires,
            Server,
            XApiVersion,
            XPoweredBy
        };

        const std::string &mapCommonHeader(CommonHeader);

        inline Map *createMap()
        {
            return new Map;
        }
    }  // namespace utils

    class RestfulHttpRequest : private utils::noncopyable
    {
        friend HttpParser;

        Map *_cookies;

        // if we have a HTTP route as /usr/{name}/information
        // then we get an request: GET /user/tj/information
        // so, we have an param "name" = "tj"
        Map *_params;

        // queries: /index.html?first=a&second=b&third=c
        Map *_queries;

        const char *_body;

        std::string _baseURL;

        int _method;

        unsigned int _bodySize;

        Map _headers;

        RestfulHttpRequest(const RestfulHttpRequest &) = delete;

    public:
        RestfulHttpRequest();


        RestfulHttpRequest(RestfulHttpRequest &&);

        ~RestfulHttpRequest();

        void swap(RestfulHttpRequest &);

        int getHeaderCount() const
        {
            return _headers.size();
        }

        bool getQuery(utils::CommonHeader h, std::string &v) const
        {
            return getQuery(utils::mapCommonHeader(h), v);
        }

        bool getQuery(const std::string &qname, std::string &out) const;

        int getQueryCount() const;

        void setMethod(int m)
        {
            _method = m;
        }

        int getMethod() const
        {
            return _method;
        }

        void emplaceQuery(std::string &&, std::string &&);

        void setBody(char *buf, size_t size)
        {
            _bodySize = size;
            _body = buf;
        }

        void setBaseURL(const std::string &url)
        {
            _baseURL = url;
        }

        const std::string &getBaseURL() const
        {
            return _baseURL;
        }

        void setBaseURL(std::string &&url)
        {
            _baseURL = url;
        }

        void emplaceHeader(std::string &&f, std::string &&v)
        {
            _headers.emplace(f, v);
        }

        void removeParam(std::string &&name);

        size_t getParamsCount() const;

        bool getParam(const std::string &p, std::string &v) const;

        void addParam(const std::string &name, const std::string &value);

        bool getHeader(const std::string &h, std::string &v);
    };

    class RestfulHttpResponse
    {
        const char *_body;

        int _status;
        unsigned int _bodySize;

    public:
        class HeaderName
        {
            HeaderName(const HeaderName &) = delete;
            HeaderName &operator=(const HeaderName &) = delete;

            union _Name {
                std::string *hname;
                utils::CommonHeader cheader;
                _Name() {}
                ~_Name() {}
            };

            enum class _Store { STRING, ENUM, NONE };

            _Name name;
            _Store store;

        public:
            size_t length() const
            {
                const std::string &str = *this;
                return str.length();
            }

            const std::string &to_string() const
            {
                if (store == _Store::STRING) {
                    return *name.hname;
                } else {
                    return utils::mapCommonHeader(name.cheader);
                }
            }

            operator const std::string &() const
            {
                return to_string();
            }

            ~HeaderName()
            {
                if (store == _Store::STRING) {
                    delete name.hname;
                }
            }

            HeaderName(HeaderName &&h);

            explicit HeaderName(utils::CommonHeader ch)
            {
                store = _Store::ENUM;
                name.cheader = ch;
            }

            explicit HeaderName(std::string &&s)
            {
                name.hname = new std::string(s);
                store = _Store::STRING;
            }

            explicit HeaderName(const std::string &s)
            {
                name.hname = new std::string(s);
                store = _Store::STRING;
            }

            bool operator<(const HeaderName &h) const;

            struct less {
                bool operator()(const HeaderName &lhs, const HeaderName &rhs) const
                {
                    return lhs < rhs;
                }
            };
        };

        using ResponseHeaderMapType = std::map<HeaderName, std::string, HeaderName::less>;

    private:
        ResponseHeaderMapType _headers;

        using pair = std::pair<std::string, std::string>;

    public:
        ~RestfulHttpResponse()
        {
            if (_body) {
                delete[] _body;
            }
        }

        RestfulHttpResponse()
        {
            _body = nullptr;
            _bodySize = 0;
            _status = 200;
        }

        void toBytes(char **ptr, size_t &size);

        void addHeader(const std::string &field, const std::string &value)
        {
            _headers.emplace(field, value);
        }

        void addHeader(utils::CommonHeader h, const std::string &value)
        {
            _headers.emplace(h, value);
        }

        bool addHeaderIfNotExists(utils::CommonHeader h, const std::string &value)
        {
            HeaderName hn(h);
            auto f = _headers.find(hn);
            if (f == _headers.end()) {
                _headers.emplace(std::move(hn), value);
                return true;
            } else {
                return false;
            }
        }

        void status(int i)
        {
            _status = i;
        }

        std::string &operator[](utils::CommonHeader h)
        {
            HeaderName n(h);
            return _headers[std::move(n)];
        }

        std::string &operator[](const std::string &f)
        {
            HeaderName n(f);
            return _headers[std::move(n)];
        }

        void setBody(const char *buf, size_t size);

        auto operator[](pair p)
        {
            HeaderName n(p.first);

            return _headers.insert(std::make_pair(std::move(n), p.second));
        }
    };

    using Request = RestfulHttpRequest;
    using Response = RestfulHttpResponse;
}  // namespace whs

#endif