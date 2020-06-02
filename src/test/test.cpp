
#include "gtest/gtest.h"

#include "whs/whs.h"
#include "whs-internal.h"
#include "parser.h"

using namespace whs;
using namespace std;
using namespace route;

namespace
{
    char req_1[] =
        "GET /index.html?first=a&second=b&third=c HTTP/1.1\r\n"
        "Host: www.baidu.com\r\n"
        "Connection: keep-alive\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "DNT: 1\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/80.0.3987.100 Safari/537.36\r\n"
        "Sec-Fetch-Dest: document\r\n"
        "Accept: "
        "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/"
        "*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
        "Sec-Fetch-Site: none\r\n"
        "Sec-Fetch-Mode: navigate\r\n"
        "Sec-Fetch-User: 1\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Accept-Language: zh,en-US;q=0.9,en;q=0.8,zh-CN;q=0.7,und;q=0.6,zh-TW;q=0.5\r\n"
        "Cookie: D_HOME=1; H_PS_PSSID=30748_1462_21126_26350_30717; BD_UPN=123353\r\n"
        "\r\n";

    char res_1[] =
        "HTTP/1.1 200 OK\r\n"
        "Bdqid: 0xdeadbeef\r\n"
        "Cache-Control: private\r\n"
        "Connection: keep-alive\r\n"
        "Content-Encoding: gzip\r\n"
        "Content-Length: 0\r\n"
        "Content-Type: text/html;charset=utf-8\r\n"
        "Date: Wed, 19 Feb 2020 12:08:41 GMT\r\n"
        "Expires: Wed, 19 Feb 2020 12:08:24 GMT\r\n"
        "Server: BWS/1.1\r\n"
        "\r\n";

#ifdef ENABLE_EXCEPTIONS
    char parserException_1[] =  // HPE_INVALID_CONSTANT
        "GET / IHTTP/1.0\r\n"
        "\r\n";
    char parserException_2[] =  // HPE_INVALID_VERSION
        "GET / HTTP/01.1\r\n"
        "\r\n";
#endif

    constexpr unsigned int hashCode(const char *str)
    {
        uint32_t ret = 0xa102abcd;
        for (; *str; ++str) {
            ret = ret * 31 + *str;
        }
        return ret;
    }

    const map<
        string,
        tuple<http_method, vector<tuple<string, uint32_t, http_method, bool, map<string, string>>>>>
        routerTest = {
#define EXPAND(str) str, hashCode(str)

            {"/",
             {HTTP_GET, {{EXPAND("/"), HTTP_GET, true, {}}, {EXPAND("/"), HTTP_POST, false, {}}}}},

            {"/a",
             {HTTP_GET,
              {{EXPAND("/a/b"), HTTP_GET, false, {}},
               {EXPAND("/a"), HTTP_GET, true, {}},
               {EXPAND("/aa"), HTTP_GET, false, {}},
               {EXPAND("/ab"), HTTP_GET, false, {}},
               {EXPAND("/a"), HTTP_POST, false, {}}}}},

            {"/a/b/c",
             {HTTP_GET,
              {{EXPAND("/a/b/c"), HTTP_GET, true, {}},
               {EXPAND("/a/b/cc"), HTTP_GET, false, {}},
               {EXPAND("/a/b"), HTTP_GET, false, {}},
               {EXPAND("/a/b/c/d"), HTTP_GET, false, {}},
               {EXPAND("/a/b/c"), HTTP_POST, false, {}}}}},

            {"/p/{a}/c",
             {HTTP_GET,
              {{EXPAND("/p/a/c"), HTTP_GET, true, {{"a", "a"}}},
               {EXPAND("/p/b/c"), HTTP_GET, true, {{"a", "b"}}},
               {EXPAND("/p/c/c"), HTTP_POST, false, {}},
               {EXPAND("/p/b/c/d"), HTTP_GET, false, {}}}}},

            {"/p/{a:abc+}/b",
             {HTTP_GET,
              {{EXPAND("/p/a/b/c"), HTTP_GET, false, {}},
               {EXPAND("/p/ab/b"), HTTP_GET, false, {}},
               {EXPAND("/p/abc/b"), HTTP_GET, true, {{"a", "abc"}}},
               {EXPAND("/p/abcc/b"), HTTP_GET, true, {{"a", "abcc"}}}}}},


            {"/p/{a:[0-9]{1,3}}/b",
             {HTTP_GET,
              {{EXPAND("/p/a/b"), HTTP_GET, false, {}},
               {EXPAND("/p/1/b"), HTTP_GET, true, {{"a", "1"}}},
               {EXPAND("/p/11/b"), HTTP_GET, true, {{"a", "11"}}},
               {EXPAND("/p/111/b"), HTTP_GET, true, {{"a", "111"}}},
               {EXPAND("/p/1111/b"), HTTP_GET, false, {}}}}},


            {"/p/{a:[0-9]{1,4}}/{b:b[0-9]*}/{c}/{d:d*}/e",
             {HTTP_POST,
              {{EXPAND("/p/1/b0/c/dd/e"), HTTP_GET, false, {}},
               {EXPAND("/p/22/b11/c/ddd/e"),
                HTTP_POST,
                true,
                {{"a", "22"}, {"b", "b11"}, {"c", "c"}, {"d", "ddd"}}},
               {EXPAND("/p/333/b222/cc/d/e"),
                HTTP_POST,
                true,
                {{"a", "333"}, {"b", "b222"}, {"c", "cc"}, {"d", "d"}}},
               {EXPAND("/p/4444/b/ccc/e/e"), HTTP_GET, false, {}}}}}
#undef EXPAND
    };

    struct TestStatus {
        int shouldAccess;
        int actualAccess;
        http_method method;
        string myPath;
        map<string, string> myParam;
        TestStatus(const string &p, const map<string, string> &param) : myPath(p), myParam(param)
        {
            shouldAccess = actualAccess = 0;
            method = (http_method)0;
        }
    };


    //map<string, shared_ptr<TestStatus>> accessStatus;

    void buildAccess(map<string, shared_ptr<TestStatus>> &m,
                     vector<tuple<string, bool, http_method>> &testPath,
                     const string &prefix)
    {
        for (const auto &p : routerTest) {
            const auto &t = p.second;

            const auto &[_, init] = t;

            for (auto &i : init) {
                const auto &[s, hc, method, access, params] = i;
                auto url = prefix;
                url.append(s);

                const auto &[itor, status] = m.emplace(url, new TestStatus(s, params));
                testPath.emplace_back(make_tuple(url, access, method));
                if (status) {
                    itor->second->method = method;
                    itor->second->shouldAccess = access ? 1 : 0;
                }
            }
        }
    }

    bool _testFunc(map<string, shared_ptr<TestStatus>> *smap, Request &req, Response &)
    {
        auto &p = req.getBaseURL();
        auto &status = smap->operator[](p);
        ++status->actualAccess;
        EXPECT_EQ(req.getParamsCount(), status->myParam.size()) << req.getBaseURL();
        EXPECT_EQ(req.getMethod(), status->method);
        for (auto &p : status->myParam) {
            string get;
            EXPECT_TRUE(req.getParam(p.first, get)) << req.getBaseURL();
            EXPECT_STREQ(get.c_str(), p.second.c_str()) << req.getBaseURL();
        }

        return true;
    }

    class TestMiddleware : public Middleware
    {
        using funct = std::function<bool(Request &, Response &)>;
        funct func;

    public:
        TestMiddleware(funct f) : func(f) {}

        virtual bool operator()(Request &req, Response &resp) const THROWS override
        {
            return func(req, resp);
        }
    };

    void buildHttpRouteBuilder(vector<tuple<string, bool, http_method>> &requests,
                               HttpRouteBuilder &builder,
                               map<string, shared_ptr<TestStatus>> *status,
                               const string &prefix)
    {
        buildAccess(*status, requests, prefix);
        using namespace placeholders;
        builder.withURLPrefix(prefix);
        for (auto &tcase : routerTest) {
            string p = prefix;
            auto &path = tcase.first;
            auto &meta = tcase.second;
            int method = get<0>(meta);
            builder.use<TestMiddleware>(method, path, bind(_testFunc, status, _1, _2));
        }
    }
}  // namespace

#ifdef ENABLE_EXCEPTIONS
#define TEST_EXPR(after)                                                        \
    do {                                                                        \
        if (access) {                                                           \
            EXPECT_NO_THROW({ pipe.feed(request, res); }) << after;             \
        } else {                                                                \
            EXPECT_THROW({ pipe.feed(request, res); }, HttpException) << after; \
        }                                                                       \
    } while (false)

#else
#define TEST_EXPR(after)                       \
    do {                                       \
        auto status = pipe.feed(request, res); \
        EXPECT_EQ(access, status) << after;    \
    } while (false)
#endif

#define RTEST(prefix)                                          \
    do {                                                       \
        vector<tuple<string, bool, http_method>> req;          \
        map<string, shared_ptr<TestStatus>> status;            \
                                                               \
        HttpRouteBuilder builder;                              \
                                                               \
        buildHttpRouteBuilder(req, builder, &status, prefix);  \
        auto router = new HttpRouter(move(builder));           \
                                                               \
        Response res;                                          \
                                                               \
        Pipeline pipe;                                         \
        pipe.addMiddleware((router));                          \
        for (auto &path : req) {                               \
            Request request;                                   \
            const auto &[url, access, method] = path;          \
            request.setBaseURL(url);                           \
            request.setMethod(method);                         \
                                                               \
            TEST_EXPR(http_method_str(method) << "->" << url); \
        }                                                      \
    } while (false)


TEST(http, routerWithPrefix)
{
    string prefix = "/some/prefix/test";
    RTEST(prefix);
}

TEST(http, routerWithoutPrefix)
{
    RTEST("");
}
#undef RTEST

#ifdef ENABLE_EXCEPTIONS
TEST(http, parserException)
{
    HttpParser p;
    ASSERT_THROW(p.readFromNetwork(parserException_1, sizeof(parserException_1) - 1),
                 HttpParserException);

    ASSERT_THROW(
        {
            HttpParser parser;
            try {
                parser.readFromNetwork(res_1, sizeof(res_1) - 1);
            } catch (const HttpParserException &e) {
                EXPECT_EQ(e.getErrorCode(), HPE_INVALID_METHOD);
                throw e;
            }
        },
        HttpParserException);

    ASSERT_THROW(
        {
            HttpParser parser;
            try {
                parser.readFromNetwork(parserException_2, sizeof(parserException_2) - 1);
            } catch (const HttpParserException &e) {
                char *out;
                size_t size;
                EXPECT_TRUE(e.buildResponse(out, size));
                char buffer[1024];
                snprintf(buffer,
                         1024,
                         "Bad Request: %s",
                         http_errno_description(static_cast<http_errno>(e.getErrorCode())));
                EXPECT_STREQ(buffer, out);
                delete[](out);
                throw e;
            }
        },
        HttpParserException);
}
#endif

TEST(http, responseBuilder)
{
    auto sortString = [](std::string &str) -> void { std::sort(str.begin(), str.end()); };
    RestfulHttpResponse res;
    res.status(HTTP_STATUS_OK);
    res.addHeader("Bdqid", "0xdeadbeef");
    res.addHeader("Cache-Control", "private");
    res[utils::CommonHeader::Connection] = "keep-alive";
    res["Content-Encoding"] = "gzip";
    res[std::make_pair("Content-Type", "text/html;charset=utf-8")];
    res[std::make_pair("Date", "Wed, 19 Feb 2020 12:08:41 GMT")];
    res[std::make_pair("Expires", "Wed, 19 Feb 2020 12:08:24 GMT")];
    res["Server"] = "BWS/1.1";
    char *out;
    size_t size;
    res.toBytes(&out, size);
    EXPECT_EQ(size, sizeof(res_1) - 1);

    string outstr(out, size);
    string resstr(res_1);

    sortString(outstr);
    sortString(resstr);

    EXPECT_EQ(outstr, resstr);
    delete[] out;
}

TEST(http, parserBaseTest)
{
    HttpParser p;

    ASSERT_TRUE(p.readFromNetwork(req_1, sizeof(req_1) - 1));

    RestfulHttpRequest &req = p.getCurrentRequest();
    ASSERT_EQ(req.getMethod(), HTTP_GET);
    ASSERT_EQ(req.getHeaderCount(), 15);
    ASSERT_STREQ(req.getBaseURL().c_str(), "/index.html");
    ASSERT_EQ(req.getQueryCount(), 3);

    std::string query;
    ASSERT_EQ(req.getQueryCount(), 3);

    ASSERT_TRUE(req.getQuery("first", query));
    ASSERT_STREQ(query.c_str(), "a");

    ASSERT_TRUE(req.getQuery("second", query));
    ASSERT_STREQ(query.c_str(), "b");

    ASSERT_TRUE(req.getQuery("third", query));
    ASSERT_STREQ(query.c_str(), "c");

    ASSERT_FALSE(req.getQuery("fourth", query));
}

TEST(http, utilsSplitURL)
{
    string t1 = "/a/b/c";
    vector<string> part;

    utils::splitURL(part, t1);
    ASSERT_EQ(part.size(), 3u);
    ASSERT_TRUE(part[0] == "a");
    ASSERT_TRUE(part[1] == "b");
    ASSERT_TRUE(part[2] == "c");

    part.clear();
    string t2 = "/{a:[0-9]*bc}/abc/{c}/{d:^a+$}";
    utils::splitURL(part, t2);
    ASSERT_EQ(part.size(), 4u);
    ASSERT_TRUE(part[0] == "{a:[0-9]*bc}");
    ASSERT_TRUE(part[1] == "abc");
    ASSERT_TRUE(part[2] == "{c}");
    ASSERT_TRUE(part[3] == "{d:^a+$}");

    part.clear();
    string t3 = "/";
    utils::splitURL(part, t3);
    ASSERT_EQ(part.size(), 0u);
}

TEST(http, parseQueryString)
{
    string t1 = "?first=1&second=2&third=three&fourth=4&fifth=";
    map<string, string> dict;
    ASSERT_TRUE(utils::parseQueryString(t1, dict));
    ASSERT_EQ(dict.size(), 5u);
    ASSERT_EQ(dict["first"], "1");
    ASSERT_EQ(dict["fifth"], "");
}