#include "gtest/gtest.h"

#include "whs/builder.h"
#include "whs/entity.h"
#include "whs/whs.h"

#include "whs-internal.h"

#include <http_parser.h>

using namespace whs;

namespace
{
    char req_1[] =
        "GET /some-path HTTP/1.1\r\n"
        "Host: www.baidu.com\r\n"
        "User-Agent: test/" WHS_VERSION
        "\r\n"
        "\r\n";

    constexpr char spStr[] = "this-is-some-path-handler";
    class SomePathHandler : public Middleware
    {
        virtual bool operator()(Request &, Response &res) const THROWS override
        {
            res.setBody(utils::dup_memory(spStr, sizeof(spStr) - 1), sizeof(spStr) - 1);
            res.status(HTTP_STATUS_OK);
            return true;
        }
    };
}  // namespace

TEST(whs, RawWhs)
{
    RawWhs r;
    PipelineBuilder a, b;
    route::HttpRouteBuilder rb;

    rb.use<SomePathHandler>(HTTP_GET, "/some-path");

    r.setup(&a, &rb, &b);
    r.init();
    r.setup();
    r.start();

    r.in(req_1, sizeof(req_1) - 1);
    char buf[512];
    size_t s = 512;
    ASSERT_NE(s, 0u);
    ASSERT_LE(s, 512u);
    r.out(buf, s);
    std::string out(buf, s);
    auto pos = out.find_last_of("\r\n\r\n");
    ASSERT_NE(pos, std::string::npos) << s;
    auto sub = out.substr(pos + 1);
    ASSERT_NE(sub.length(), 0u);
    ASSERT_EQ(sub, spStr);
}