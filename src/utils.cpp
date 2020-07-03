#include "whs/entity.h"
#include "whs-internal.h"
#include "utils.h"

#include "fmt/format.h"

using std::string;
using namespace whs;
using whsutils::MemoryBuffer;

void MemoryBuffer::clear()
{
    _pool.clear();
    _gcount = _stored = 0;
}

size_t MemoryBuffer::write(const char *at, size_t size)
{
    auto remainSize = size;
    auto ptr = at;
    size_t wroteSize = 0;
    if (_pool.size() == 0) {
        do {
            _Storage s;
            wroteSize = s.write(ptr, remainSize);
            ptr += wroteSize;
            remainSize -= wroteSize;
            _pool.emplace_back(std::move(s));
            _stored += wroteSize;
        } while (remainSize > 0);
    } else {
        auto &tail = _pool.back();
        wroteSize = tail.write(ptr, remainSize);
        ptr += wroteSize;
        remainSize -= wroteSize;
        _stored += wroteSize;
        while (remainSize > 0) {
            _Storage s;
            wroteSize = s.write(ptr, remainSize);
            ptr += wroteSize;
            remainSize -= wroteSize;
            _pool.emplace_back(std::move(s));
            _stored += wroteSize;
        }
    }

#ifndef NDEBUG
    auto testfunc = [this]() -> bool {
        size_t store = 0;
        for (auto &_entry : _pool) {
            store += _entry.storedSize();
        }
        return store == _stored;
    };

    assert(testfunc());
#endif

    return wroteSize;
}

size_t MemoryBuffer::read(char *dest, size_t &size)
{
#ifndef NDEBUG
    auto testfunc = [this]() -> bool {
        size_t store = 0;
        for (auto &_entry : _pool) {
            store += _entry.storedSize();
        }
        return store == _stored;
    };

    assert(testfunc());
#endif

    _gcount = 0;
    size_t readSize = 0;
    auto ptr = dest;
    auto remain = size;
    for (auto begin = _pool.begin(); begin != _pool.end(); begin++) {
        readSize = begin->read(ptr, remain);
        remain -= readSize;
        ptr += readSize;
        _gcount += readSize;
        _stored -= readSize;
        if (remain == 0) {
            break;
        }
    }

    do {
        auto &front = _pool.front();
        if (front.storedSize() == 0) {
            _pool.pop_front();
        } else {
            break;
        }
    } while (_pool.size() > 0);

    return size;
}

size_t MemoryBuffer::_Storage::write(const char *at, size_t size)
{
    auto remain = remainSize();
    auto wrote = std::min(size, remain);
    memcpy(_end, at, wrote);
    _end += wrote;

    assert(_end <= _ptr + _pageSize);

    return wrote;
}

size_t MemoryBuffer::_Storage::read(char *dest, size_t size)
{
    auto stored = storedSize();
    auto readSize = std::min(size, stored);
    memcpy(dest, _begin, readSize);
    _begin += readSize;

    assert(_begin <= _end);
    assert(_end <= _ptr + _pageSize);

    return readSize;
}

namespace
{
    namespace __internal
    {
        utils::regex *paramRE = nullptr;
        utils::regex *queryRE = nullptr;
    }  // namespace __internal

    utils::regex *getQueryStringRegex()
    {
        if (__internal::queryRE == nullptr) {
            __internal::queryRE =
                utils::regex::compile("(\\?|\\&)(?<name>[^=]+)\\=(?<value>[^&]*)");
            assert(__internal::queryRE);
            atexit([]() { delete __internal::queryRE; });
        }
        return __internal::queryRE;
    }


    utils::regex *getParamParseRegex()
    {
        if (__internal::paramRE == nullptr) {
            __internal::paramRE =
                utils::regex::compile("^{(?<name>([[:alnum:]]|_)+)(:(?<regex>\\S+))?}$");
            assert(__internal::paramRE);
            atexit([]() {
                delete __internal::paramRE;
                __internal::paramRE = nullptr;
            });
        }
        return __internal::paramRE;
    }
}  // namespace


namespace whs::utils
{
    bool parseQueryString(const std::string &query, std::map<std::string, std::string> &dict)
    {
        auto re = getQueryStringRegex();
        utils::regex::regex_group_match matches;
        auto status = re->execute(query, matches);
        if (status) {
            for (auto &n : matches) {
                auto name = n.named_matches["name"];
                auto value = n.named_matches["value"];
                dict.emplace(move(name), move(value));
            }
        }
        return status;
    }


    bool parseParam(const std::string &param, std::string &paramName, utils::regex &reg)
    {
        assert(utils::isParam(param));
        auto re = getParamParseRegex();
        utils::regex::regex_group_match matches;
        auto status = re->execute(param, matches);
        if (status) {
            status = false;
            if (matches.size() != 1) {
                logger::error(
                    fmt::format("whs-core: parse param argument failed: bad format {}", param));
            } else {
                const auto &match = matches[0];
                auto name = match.named_matches.find("name");
                if (name == match.named_matches.end()) {
                    logger::error(fmt::format(
                        "whs-core: parse param argument failed: bad format {}, missing param name",
                        param));
                } else {
                    paramName = name->second;
                    auto re = match.named_matches.find("regex");
                    if (re == match.named_matches.end() || re->second.length() == 0) {
                        utils::regex::compile("^.*$", reg);
                        status = true;
                    } else {
                        status =
                            utils::regex::compile(fmt::format("^{}$", re->second).c_str(), reg);
                    }
                }
            }
        } else {
            logger::error(
                fmt::format("whs-core: parse param argument failed: bad format {}", param));
        }
        return status;
    }

    /**
     * @brief split URL path into vector of url path
     *  Example:
     *      URL  -> /a/b/c/d
     *      part -> {"a", "b", "c", "d"}
     * @param part  split result
     * @param url   url input
     * @return true
     * @return false
     */
    bool splitURL(std::vector<std::string> &part, const std::string &url)
    {
        if (url != "/") {
            std::string tmp = url;
            if (tmp.find('/') == 0) {
                tmp = tmp.substr(1);
            }
            size_t pos = 0;
            while (true) {
                pos = tmp.find('/');
                part.emplace_back(tmp.substr(0, pos));
                tmp.erase(0, pos + 1);
                if (pos == std::string::npos) {
                    break;
                }
            }
        }
        return true;
    }
}  // namespace whs::utils


#define _BuildHeader(ch, str) \
    {                         \
        CommonHeader::ch, str \
    }

#define _BuildHeaderString(ch) _BuildHeader(ch, #ch)

const std::string &whs::utils::mapCommonHeader(CommonHeader h)
{
    const static std::unordered_map<CommonHeader, string> _m = {
        _BuildHeaderString(Host),
        _BuildHeaderString(Connection),
        _BuildHeaderString(Accept),
        _BuildHeaderString(Date),
        _BuildHeaderString(Server),
        _BuildHeaderString(Expires),
        _BuildHeaderString(Etag),
        _BuildHeader(LastModified, "Last-Modified"),
        _BuildHeader(AcceptEncoding, "Accept-Encoding"),
        _BuildHeader(AcceptLanguage, "Accept-Language"),
        _BuildHeader(ContentEncoding, "Content-Encoding"),
        _BuildHeader(ContentType, "Content-Type"),
        _BuildHeader(CacheControl, "Cache-Control"),
        _BuildHeader(UserAgent, "User-Agent"),
        _BuildHeader(XApiVersion, "X-Api-Version"),
        _BuildHeader(XPoweredBy, "X-Powered-by"),
        _BuildHeader(ContentLength, "Content-Length")};
    auto f = _m.find(h);
    assert(f != _m.end());
    return f->second;
}