
#include "client.h"
#include "utils.h"
#include "parser.h"

using namespace whs;

#include <cassert>
#include <cmath>

using namespace std;
using namespace whs;
namespace hpcb = whs::httpParserCallbacks;

// assume the order of callbacks in struct http_parser_settings never change.
// PS1: note that on each HTTP Message incoming parse chain phrase, the order of callback functions
//      are same with processing procedure.
// PS2. according to compiler C++2a support, only MSVC>=19.21 accept this designated initializers
//      refers to: https://en.cppreference.com/w/cpp/compiler_support
static constexpr http_parser_settings settings = {.on_message_begin = hpcb::onMessageBegin,
                                                  .on_url = hpcb::onURL,
                                                  .on_status = hpcb::onStatus,
                                                  .on_header_field = hpcb::onHeaderField,
                                                  .on_header_value = hpcb::onHeaderValue,
                                                  .on_headers_complete = hpcb::onHeaderCompelete,
                                                  .on_body = hpcb::onBody,
                                                  .on_message_complete = hpcb::onMessageComplete,

                                                  .on_chunk_header = hpcb::onChunkerHeader,
                                                  .on_chunk_complete = hpcb::onChunkComplete};

namespace
{
    // parseQueryString: split URL queries into map
    //                   query parameter should begin at char_('?')
}  // namespace


namespace whs::httpParserCallbacks
{
    void setCurrent(HttpParser* hp, const vector<char>& cs)
    {
        hp->setCurrentHeaderField(string(cs.cbegin(), cs.cend()));
    }

    void setCurrentQuery(HttpParser* hp, const vector<char>& cs)
    {
        string value(cs.cbegin(), cs.cend());
        hp->setQuery(std::move(value));
    }

    int onMessageBegin(http_parser*)
    {
        return 0;
    }

    int onURL(http_parser* p, const char* at, size_t l)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        string url(at, l);
        const auto pos = url.find_first_of('?');
        if (pos == string::npos) {
            hp->setBaseURL(move(url));
            return 0;
        } else {
            hp->setBaseURL(url.substr(0, pos));
            string query(url.substr(pos));
            std::map<std::string, std::string> dict;
            auto status = utils::parseQueryString(query, dict);
            for (const auto& k : dict) {
                std::string name(k.first);
                std::string value(k.second);
                hp->current.emplaceQuery(std::move(name), std::move(value));
            }
            return status ? 0 : 1;
        }
    }

    int onStatus(http_parser*, const char*, size_t)
    // I am HTTP 'Request' parser, never should have a status field in http header
    {
        assert(false);
        return 1;
    }

    int onHeaderField(http_parser* p, const char* at, size_t l)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        string header(at, l);
        std::for_each(header.begin(), header.end(), [](char& c) { c = std::tolower(c); });
        hp->setCurrentHeaderField(std::move(header));
        return 0;
    }

    int onHeaderValue(http_parser* p, const char* at, size_t l)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        hp->setCurrentHeaderValue(string(at, l));
        return 0;
    }

    int onBody(http_parser* p, const char* at, size_t l)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        hp->_buf.write(at, l);
        hp->bodyLength += l;
        return 0;
    }

    int onHeaderCompelete(http_parser* p)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        hp->current.setMethod(p->method);
        return 0;
    }

    int onMessageComplete(http_parser* p)
    {
        HttpParser* hp = (HttpParser*)(p->data);
        hp->finishCurrentRequest();
        return 0;
    }

    int onChunkerHeader(http_parser*)
    {
        return 0;
    }

    int onChunkComplete(http_parser*)
    {
        return 0;
    }
}  // namespace whs::httpParserCallbacks


HttpParser::HttpParser(Client* c) : _client(c)
{
    parser.data = this;
    http_parser_init(&parser, HTTP_REQUEST);
    bodyLength = 0;
}

void HttpParser::finishCurrentRequest()
{
    if (bodyLength > 0) {
        char* buf = new char[(bodyLength + 1)];
        buf[bodyLength] = 0;
        _buf.read(buf, bodyLength);
        assert((size_t)_buf.gcount() == bodyLength);
        assert(_buf);
        current.setBody(buf, bodyLength);

        buf = nullptr;
        bodyLength = 0;
    }
    if (_client) {
        Response resp;
        _client->processing_request(current, resp);
        _client->write_response(resp);
    }
}

bool HttpParser::readFromNetwork(const char* buf, int size) THROWS
{
    bool status = (int)http_parser_execute(&parser, &settings, buf, size) == size;

    if (!status) {
        auto eno = HTTP_PARSER_ERRNO(&parser);
        auto msg = http_errno_description(static_cast<http_errno>(eno));
        throw HttpParserException(parser.http_errno, msg);
    }

    return status;
}
HttpParserException::~HttpParserException() {}

bool HttpParserException::buildResponse(char*& ptr, size_t& size) const
{
    static const char badRequest[] = "Bad Request: ";
    const int brSize = sizeof(badRequest) - 1;
    size = sizeof(badRequest) + _what.size();
    char* p = ptr = new char[(size + 1)];
    bzero(p, sizeof(char) * (size + 1));
    memcpy(p, badRequest, brSize);
    p += brSize;
    memcpy(p, _what.c_str(), _what.size());
    ptr[size] = '\0';
    return true;
}

bool route::NotFoundException::buildResponse(char*& ptr, size_t& size) const
{
    static const char notFound[] = "Not Route Found: ";
    static const int nfSize = sizeof(notFound) - 1;
    size = nfSize + _req.getBaseURL().length() + 20;
    char* p = ptr = new char[(size)];
    memcpy(p, notFound, nfSize);
    p += nfSize;

    for (auto method = http_method_str(static_cast<http_method>(_httpMethod)); *method;
         ++method, ++p) {
        *p = *method;
    }
    p[0] = ' ', p[1] = 't', p[2] = 'o', p[3] = ' ', p += 3;

    for (auto url = _req.getBaseURL().c_str(); *url; ++p, ++url) {
        *p = *url;
    }
    p[0] = '.', p[1] = 0, p += 1;
    size = p - ptr;
    return true;
}

void HttpParser::reset()
{
    http_parser_init(&parser, HTTP_REQUEST);
    bodyLength = 0;
    Request req;
    current.swap(req);
}


route::NotFoundException::NotFoundException(const Request& req, const std::string& url)
    : HttpException(std::string("Not Found exception: ")
                        .append(http_method_str(static_cast<http_method>(req.getMethod())))
                        .append(" to ")
                        .append(req.getBaseURL())),
      _url(url),
      _req(req)
{
    _statusCode = HTTP_STATUS_NOT_FOUND;
    _httpMethod = req.getMethod();
}
