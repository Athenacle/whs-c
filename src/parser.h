#ifndef WHS_PARSER_H
#define WHS_PARSER_H

#include "whs/whs.h"
#include "whs/entity.h"

#include "whs-internal.h"
#include "utils.h"


namespace whs
{
    namespace httpParserCallbacks
    {
        int onMessageBegin(http_parser*);
        int onURL(http_parser* p, const char*, size_t);
        int onStatus(http_parser*, const char*, size_t);
        int onHeaderField(http_parser*, const char*, size_t);
        int onHeaderValue(http_parser*, const char*, size_t);
        int onBody(http_parser*, const char*, size_t);
        int onHeaderCompelete(http_parser*);
        int onMessageComplete(http_parser*);
        int onChunkerHeader(http_parser*);
        int onChunkComplete(http_parser*);

        void setCurrent(HttpParser*, const std::vector<char>&);
        void setCurrentQuery(HttpParser*, const std::vector<char>&);

    }  // namespace httpParserCallbacks

    class HttpParser
    {
        friend class Client;

        friend int httpParserCallbacks::onMessageBegin(http_parser*);
        friend int httpParserCallbacks::onURL(http_parser* p, const char*, size_t);
        friend int httpParserCallbacks::onStatus(http_parser*, const char*, size_t);
        friend int httpParserCallbacks::onHeaderField(http_parser*, const char*, size_t);
        friend int httpParserCallbacks::onHeaderValue(http_parser*, const char*, size_t);
        friend int httpParserCallbacks::onBody(http_parser*, const char*, size_t);
        friend int httpParserCallbacks::onHeaderCompelete(http_parser*);
        friend int httpParserCallbacks::onMessageComplete(http_parser*);
        friend int httpParserCallbacks::onChunkerHeader(http_parser*);
        friend int httpParserCallbacks::onChunkComplete(http_parser*);

        friend void httpParserCallbacks::setCurrent(HttpParser*, const std::vector<char>&);
        friend void httpParserCallbacks::setCurrentQuery(HttpParser*, const std::vector<char>&);

        size_t bodyLength;
        whsutils::MemoryBuffer _buf;
        http_parser parser;

        Client* _client;

        RestfulHttpRequest current;

        std::string currentHeaderField;

        void setQuery(std::string&& value)
        {
            current.emplaceQuery(std::move(currentHeaderField), std::move(value));
        }

        void setBaseURL(std::string&& value)
        {
            current.setBaseURL(value);
        }

    public:
        bool shouldCloseConnection() const
        {
            return http_should_keep_alive(&parser) == 0;
        }

        RestfulHttpRequest& getCurrentRequest()
        {
            return current;
        }

        void setCurrentHeaderValue(std::string&& v)
        {
            current.emplaceHeader(move(currentHeaderField), move(v));
        }

        void setCurrentHeaderField(std::string&& f)
        {
            currentHeaderField = f;
            std::transform(currentHeaderField.begin(),
                           currentHeaderField.begin(),
                           currentHeaderField.end(),
                           [](auto c) { return tolower(c); });
        }

        HttpParser(Client* = nullptr);

        bool readFromNetwork(const char*, int) THROWS;

        void finishCurrentRequest();

        void reset();
    };
}  // namespace whs

#endif