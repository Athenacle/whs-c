#include "whs/entity.h"
#include "whs-internal.h"

using namespace whs;

/// Request functions

/**
 * @brief Construct a new Restful Http Request:: Restful Http Request object
 *
 */
RestfulHttpRequest::RestfulHttpRequest()
{
    _cookies = _params = _queries = nullptr;
    _body = nullptr;
    _method = _bodySize = 0;
}

/**
 * @brief Construct a new Restful Http Request:: Restful Http Request object
 * Move Constructor
 * @param req
 */
RestfulHttpRequest::RestfulHttpRequest(RestfulHttpRequest&& req)
{
    _cookies = req._cookies;
    _params = req._params;
    _queries = req._queries;
    _body = req._body;
    _headers.swap(req._headers);
    _baseURL.swap(req._baseURL);
    _method = req._method;
    req._queries = req._params = req._cookies = nullptr;
}
/**
 * @brief Destroy the Restful Http Request:: Restful Http Request object
 *
 */
RestfulHttpRequest::~RestfulHttpRequest()
{
    if (_cookies)
        delete _cookies;
    if (_params)
        delete _params;
    if (_queries)
        delete _queries;
    if (_body)
        delete[](_body);
}
/**
 * @brief swap utility for RestfulHttpRequest
 *
 * @param req another RestfulHttpRequest for swapping
 */
void RestfulHttpRequest::swap(RestfulHttpRequest& req)
{
    std::swap(_cookies, req._cookies);
    std::swap(_queries, req._queries);
    std::swap(_params, req._params);
    std::swap(_method, req._method);
    std::swap(_body, req._body);

    process_data.swap(req.process_data);
    _headers.swap(req._headers);
    _baseURL.swap(req._baseURL);
}

/**
 * @brief check and get Query in RestfulHttpRequest
 *
 * @param qname query name
 * @param out   query value. Note: Please check return value before accessing to this parameter
 * @return true success
 * @return false query `qname' not found in queries. `out' parameter is undefined
 */
bool RestfulHttpRequest::getQuery(const std::string& qname, std::string& out) const
{
    if (_queries) {
        const auto& found = _queries->find(qname);
        if (found != _queries->end()) {
            out = found->second;
            return true;
        }
    }
    return false;
}

/**
 * @brief get URL Query count
 *
 * @return int
 */
int RestfulHttpRequest::getQueryCount() const
{
    if (_queries)
        return _queries->size();
    else
        return 0;
}

/**
 * @brief put new URL query key-value pair into RestfulHttpRequest
 *
 * @param first KEY
 * @param second VALUE
 */
void RestfulHttpRequest::emplaceQuery(std::string&& first, std::string&& second)
{
    if (_queries == nullptr) {
        _queries = utils::createMap();
    }
    _queries->emplace(first, second);
}

/**
 * @brief remove Param from Request.
 *
 * @param name param name for removing.
 */
void RestfulHttpRequest::removeParam(std::string&& name)
{
    assert(_params != nullptr);
    auto find = _params->find(name);
    if (find != _params->end()) {
        _params->erase(find);
    }
}
/**
 * @brief get param count
 *
 * @return size_t
 */
size_t RestfulHttpRequest::getParamsCount() const
{
    if (_params == nullptr)
        return 0;
    else
        return _params->size();
}
/**
 * @brief get param key-value pair
 *
 * @param p param name
 * @param v param value. Note: Please check return value before accessing to this parameter.
 * @return true param whose name is `p' found.
 * @return false not found
 */
bool RestfulHttpRequest::getParam(const std::string& p, std::string& v) const
{
    if (_params == nullptr) {
        return false;
    } else {
        const auto& find = _params->find(p);
        if (find == _params->end()) {
            return false;
        } else {
            v = find->second;
            return true;
        }
    }
}
/**
 * @brief add param key-value pair
 *
 * @param name param name
 * @param value param value
 */
void RestfulHttpRequest::addParam(const std::string& name, const std::string& value)
{
    if (_params == nullptr) {
        _params = utils::createMap();
    }
    auto find = _params->find(name);
    if (find == _params->end()) {
        _params->emplace((std::string(name)), std::string(value));
    }
}

/**
 * @brief get Http Header
 *
 * @param h header name
 * @param v header value. Note: Please check return value before accessing this parameter.
 * @return true
 * @return false
 */
bool RestfulHttpRequest::getHeader(const std::string& h, std::string& v)
{
    auto find = _headers.find(h);
    if (find == _headers.end()) {
        return false;
    } else {
        v = find->second;
        return true;
    }
}
