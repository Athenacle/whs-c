#include "whs.h"
#include "whs-internal.h"

#include <cmath>

using namespace whs;
using namespace std;

namespace
{
    inline uint64_t pow10(int n)
    {
        uint64_t ret = 1;
        for (int i = 0; i < n; ++i) {
            ret = ret * 10;
        }
        return ret;
    }

    inline int getDigits(uint64_t value)
    {
        auto digits = 0;

        // avoid always call std::log10
        // this function is used in Response::build, its size will not be too large in most cases
        if ((value < 10)) {
            digits = 0;
        } else if ((value < 100)) {
            digits = 1;
        } else if ((value < 1000)) {
            digits = 2;
        } else if ((value < 10000)) {
            digits = 3;
        } else {
            digits = static_cast<int>(std::log10(value));
        }
        return digits;
    }

    const char* itoa(char* buf, uint64_t value, int& digits)
    {
        auto p = buf;
        auto v = value;
        if (digits <= 0) {
            digits = getDigits(value);
        }
        auto d = digits;

        for (; d >= 0; --d) {
            uint64_t base = pow10(d);
            *p = v / base + '0';
            v = v % base;
            ++p;
        }
        *p = 0;
        return buf;
    }

    const char* itoa(char* buf, uint64_t value)
    {
        int d = getDigits(value);
        itoa(buf, value, d);
        return buf;
    }

    size_t calcHttpResponseSize(const RestfulHttpResponse::ResponseHeaderMapType& hs,
                                size_t otherBytes)
    {
        size_t size = 0;

        size = 8 + 1 +  // HTTP/1.1<space>
               3 + 1 +  // status code<space>
               32 +     // status code string (The longest status code string is 31 bytes)
               2;       // \r\n

        for (const auto& entry : hs) {
            // <header field>: <header value>\r\n
            //               12               3 4
            size += entry.first.length() + entry.second.length() + 4;
        }
        size += 2;  // \r\n at end of HTTP package header
        if (otherBytes > 0) {
            size += (otherBytes + 2);  // body end \r\n
        }

        return size;
    }
}  // namespace


/// RestfulHttpResponse functions

RestfulHttpResponse::HeaderName::HeaderName(HeaderName&& h)
{
    store = h.store;
    assert(h.store != _Store::NONE);
    if (store == _Store::STRING) {
        name.hname = h.name.hname;
    } else {
        name.cheader = h.name.cheader;
    }
    h.store = _Store::NONE;
}


bool RestfulHttpResponse::HeaderName::operator<(const HeaderName& h) const
{
    assert(h.store != _Store::NONE);
    assert(store != _Store::NONE);
    if (h.store == store) {
        if (h.store == _Store::ENUM) {
            return name.cheader < h.name.cheader;
        } else {
            return *name.hname < (*h.name.hname);
        }
    } else {
        const auto& first = to_string();
        const auto& second = h.to_string();
        return first < second;
    }
}

void RestfulHttpResponse::setBody(const char* buf, size_t size)
{
    char buffer[24] = {0};

    addHeader(utils::CommonHeader::ContentType, "text/plain");
    auto& type = this->operator[](utils::CommonHeader::ContentType);
    if (type.empty()) {
        type = "text/plain";
    }

    addHeader(utils::CommonHeader::ContentLength, string(itoa(buffer, size)));
    addHeader(utils::CommonHeader::ContentEncoding, "identity");

    _body = buf;
    _bodySize = size;
}


#define _LINEEND                                                  \
    do {                                                          \
        (void)(p[0] = '\r'), (void)(p[1] = '\n'), (void)(p += 2); \
    } while (false)


void RestfulHttpResponse::toBytes(char** ptr, size_t& size)
{
    if (_bodySize == 0) {
        static char zero[] = "0";
        this->operator[](utils::CommonHeader::ContentLength) = zero;
    }
    auto allocSize = calcHttpResponseSize(_headers, _bodySize) + 5;

    char* p = *ptr = new char[(allocSize)];
    char* end = p + allocSize;

    {
        // http version
        const char http[] = "HTTP/1.1 ";

        memcpy(p, http, sizeof http - 1);
        p += sizeof(http) - 1;

        // status code;
        int first = _status / 100;
        int second = (_status % 100) / 10;
        int third = _status % 10;

        p[0] = first + '0', p[1] = second + '0', p[2] = third + '0', p[3] = ' ';
        p += 4;

        // status string

        for (const char* s = http_status_str(static_cast<enum http_status>(_status)); *s;
             ++p, ++s) {
            *p = *s;
        }

        _LINEEND;
    }

    for (const auto& entry : _headers) {
        const string& hname = entry.first;
        for (const char* name = hname.c_str(); *name; ++p, ++name) {
            *p = *name;
        }

        p[0] = ':', p[1] = ' ';
        p += 2;

        for (const char* value = entry.second.c_str(); *value; ++p, ++value) {
            *p = *value;
        }
        _LINEEND;
    }

    _LINEEND;

    assert(end - p > _bodySize);
    if (_bodySize > 0) {
        memcpy(p, _body, _bodySize);
        p += _bodySize;
        // _LINEEND;
    }

    size = p - *ptr;
}