#ifndef WHS_BUILDER_H_
#define WHS_BUILDER_H_

#include <whs/common.h>

#include <type_traits>
#include <vector>
#include <string>
#include <utility>


namespace whs
{
    namespace utils
    {
        bool splitURL(std::vector<std::string> &, const std::string &);
    }  // namespace utils

    class RestfulHttpRequest;
    class RestfulHttpResponse;

    // Middleware Object.
    // all http requests will be push into a Middleware queue for processing
    class Middleware
    {
        Middleware(const Middleware &) = delete;
        Middleware(Middleware &&) = delete;

    public:
        virtual ~Middleware() {}
        Middleware() {}

        // Middleware -> work as pipeline.
        // if operator() return true, continuing pipline,
        // otherwise, skip and go into error handle.
        virtual bool operator()(RestfulHttpRequest &req,
                                RestfulHttpResponse &resp) const THROWS = 0;
    };

    template <class T>
    struct IsMiddleWare : public std::integral_constant<bool,
                                                        std::is_base_of<Middleware, T>::value
                                                            && !std::is_abstract<T>::value> {
    };

    template <class T, class V>
    struct EnableIfMiddle : public std::enable_if<IsMiddleWare<T>::value, V> {
    };

    template <class M, class V = void>
    using EnableIfMiddleType = typename EnableIfMiddle<M, V>::type;

    namespace route
    {
        struct HttpRouteNode;
        struct HttpRouteRootNode;

        class HttpRouteBuilder
        {
            friend class whs::route::HttpRouter;

            using iterator = std::vector<std::string>::const_iterator;
            using reference = HttpRouteBuilder &;
            using vstring = std::vector<std::string>;

        private:
            enum class TreeNodeType {
                URL_STRING_SEGMENT,
                URL_QUERY_SEGMENT,
                URL_SEGMENT_END,
                URL_ROOT
            };

            struct TreeNode {
                TreeNodeType type;
                int method;
                std::string _midName;
                std::string _myNodeName;
                std::string _pathToMe;
                std::vector<TreeNode *> _children;

                Middleware *func;

                ~TreeNode();

                bool operator==(const std::string &cmp);
            };

            TreeNode *root;

            void insertChild(int, TreeNode *, iterator, iterator, Middleware *, const char *);

            std::vector<Middleware *> middles;
            std::vector<TreeNode *> endNodes;

        public:
            template <class Middle, class... Args>
            auto use(int Method, const std::string &path, Args &&... args)
                -> EnableIfMiddleType<Middle, reference>
            {
                std::vector<std::string> paths;
                Middleware *ware = new Middle(std::forward<Args>(args)...);
                middles.emplace_back(ware);
                utils::splitURL(paths, path);

                insertChild(Method,
                            root,
                            paths.cbegin(),
                            paths.cend(),
                            ware,
                            utils::demangle(typeid(Middle).name()));
                return *this;
            }

            template <int Method, class Middle, class... Args>
            auto use(const std::string &path, Args &&... args)
                -> EnableIfMiddleType<Middle, reference>
            {
                return use<Middle>(Method, path, std::forward<Args>(args)...);
            }

            reference withURLPrefix(const std::string &);

            ~HttpRouteBuilder();

            HttpRouteBuilder();

            HttpRouteRootNode *build();

            int endNodesCount() const
            {
                return static_cast<int>(endNodes.size());
            }

            static void buildTreeNode(TreeNode *bnode, HttpRouteNode *rnode);
        };

    }  // namespace route

    class PipelineBuilder final
    {
        friend class Pipeline;

        using itor = std::pair<std::string, Middleware *>;
        std::vector<std::pair<std::string, Middleware *>> _wares;

    public:
        PipelineBuilder() {}

        void addMiddleware(Middleware *p)
        {
            _wares.emplace_back(std::make_pair("unknown", p));
        }

        template <class T, class _ = EnableIfMiddleType<T>, class... Args>
        void addMiddleware(const std::string &name, Args &&... args)
        {
            MiddlewarePointer p(new T(std::forward<Args>(args)...));
            _wares.emplace_back(std::make_pair(name, p));
        }

        template <class T, class _ = EnableIfMiddleType<T>, class... Args>
        void addMiddleware(Args &&... args)
        {
            MiddlewarePointer p = new T(std::forward<Args>(args)...);
            _wares.emplace_back(std::make_pair("unknown", p));
        }

        auto cend() const
        {
            return _wares.cend();
        }

        auto cbegin() const
        {
            return _wares.cbegin();
        }
#define AddMiddleware(pb, Middle, ...) pb.addMiddleware<Middle>(#Middle __VA_ARGS__)
    };

}  // namespace whs
#endif