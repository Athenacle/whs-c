#include "gtest/gtest.h"

#include "whs-internal.h"
#include "whs/builder.h"
#include "whs/entity.h"

using namespace whs;

namespace
{
    class PipelineTest : public Middleware
    {
        int add;

    public:
        PipelineTest(int a) : add(a) {}

        virtual bool operator()(Request&, Response& resp) const THROWS override
        {
            int i = resp.status();
            resp.status(i + add);
            return true;
        }
    };
}  // namespace

TEST(builder_test, pipelinebuilder)
{
    PipelineBuilder b;
    b.addMiddleware<PipelineTest>(1);
    b.addMiddleware<PipelineTest>(2);
    b.addMiddleware<PipelineTest>(3);
    b.addMiddleware<PipelineTest>(4);

    Pipeline p(b);

    Request req;
    Response resp;
    resp.status(0);

    p.feed(req, resp);

    ASSERT_EQ(resp.status(), 10);  // 0 + 1 + 2 + 3 + 4
}

TEST(entity, request_process_data)
{
    Request req;
    req.put_processing_data("apple", &req);
    ASSERT_EQ(req.get_processing_data("apple"), &req);
    ASSERT_EQ(req.get_processing_data("orange"), nullptr);
}