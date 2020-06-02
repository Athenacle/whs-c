#include "whs/entity.h"
#include "whs-internal.h"

using std::string;
using namespace whs;
using namespace whs::route;

using builder = route::HttpRouteBuilder;
using mp = MiddlewarePointer;
using hr = route::HttpRouter;

bool hr::operator()(Request& req, Response& resp) const THROWS
{
    auto& url = req.getBaseURL();
    const auto& next = GetRoute(req, url);
    if (next) {
        return next->operator()(req, resp);
    } else {
#ifdef ENABLE_EXCEPTIONS
        throw NotFoundException(req, url);
#else
        return false;
#endif
    }
}

HttpRouteNode::~HttpRouteNode()
{
    for (int i = 0; i < _childrenCount; i++) {
        delete *(_children + i);
    }
    delete[] _children;
}

HttpRouteStringNode::~HttpRouteStringNode() {}

HttpRouteParamNode::HttpRouteParamNode(const std::string& pn, utils::regex&& regex) : _paramName(pn)
{
    _regex = new utils::regex(std::move(regex));
}

HttpRouteParamNode::~HttpRouteParamNode()
{
    if (_regex != nullptr) {
        delete _regex;
    }
}

const mp hr::emptyMiddleware(nullptr);

builder::HttpRouteBuilder()
{
    root = new TreeNode;
    root->_myNodeName = "";
    root->_pathToMe = "";
    root->type = TreeNodeType::URL_ROOT;
}

HttpRouteRootNode* builder::build()
{
    HttpRouteRootNode* root = new HttpRouteRootNode(this->root->_myNodeName);
    HttpRouteBuilder::buildTreeNode(this->root, root);
    return root;
}

void builder::buildTreeNode(TreeNode* bnode, HttpRouteNode* rnode)
{
    auto children = bnode->_children;
    auto size = children.size();
    rnode->_childrenCount = size;
    rnode->_children = new HttpRouteNode*[size];
    for (size_t i = 0; i < size; i++) {
        auto c = children[i];
        HttpRouteNode* node = nullptr;
        switch (c->type) {
            case TreeNodeType::URL_QUERY_SEGMENT: {
                std::string pn;
                utils::regex regex;
                bool s = utils::parseParam(c->_myNodeName, pn, regex);
                assert(s);
                auto nParam = new HttpRouteParamNode(pn, std::move(regex));
                node = nParam;
            } break;
            case TreeNodeType::URL_STRING_SEGMENT: {
                auto nString = new HttpRouteStringNode(c->_myNodeName);
                node = nString;
            } break;
            case TreeNodeType::URL_SEGMENT_END: {
                assert(c->func != nullptr);
                HttpRouteEndNode* end = new HttpRouteEndNode(c->func);
                end->method = c->method;
                node = end;
                break;
            } break;
            case TreeNodeType::URL_ROOT:
                assert(false);
                break;
        }
        *(rnode->_children + i) = node;
        buildTreeNode(c, node);
    }
}


void builder::insertChild(
    int method, TreeNode* current, iterator begin, iterator end, Middleware* m, const char* name)
{
    if (begin == end) {
        auto ins = new TreeNode;
        ins->type = TreeNodeType::URL_SEGMENT_END;
        ins->method = method;
        ins->func = m;
        ins->_midName = name == nullptr ? "unknown" : name;
        current->_children.push_back(ins);
        endNodes.emplace_back(ins);
        return;
    }
    auto children = current->_children;
    for (auto& c : children) {
        if (c->operator==(*begin)) {
            return insertChild(method, c, begin + 1, end, m, name);
        }
    }

    // no current 'begin' segment in children. insert it.
    auto ins = new TreeNode;
    const auto& currentSegment = *begin;
    ins->_myNodeName = currentSegment;
    ins->_pathToMe = current->_pathToMe + "/" + currentSegment;
    ins->type =
        utils::isParam(*begin) ? TreeNodeType::URL_QUERY_SEGMENT : TreeNodeType::URL_STRING_SEGMENT;
    current->_children.push_back(ins);
    insertChild(method, ins, begin + 1, end, m, name);
}

HttpRouteRootNode::HttpRouteRootNode(const string& myName) : HttpRouteStringNode(myName) {}

const mp& HttpRouteRootNode::GetRoute(Request& req, const string& url) const
{
    const char* p = url.c_str();
    const char* end = p + url.length();
    return getRoute(req, p, end);
}

const mp& HttpRouteStringNode::getRoute(Request& req,
                                        const char* current,
                                        const char* end) const THROWS
{
    if (current >= end) {
        return HttpRouter::emptyMiddleware;
    } else {
        for (const char* my = _nodeName.c_str(); *my; ++my, ++current) {
            if (current == end) {
                return HttpRouter::emptyMiddleware;
            }
            if (*my != *current) {
                return HttpRouter::emptyMiddleware;
            }
        }
        if (*current != '/' && *current != '\0') {
            return hr::emptyMiddleware;
        }
        for (int i = 0; i < _childrenCount; i++) {
            auto& existInChildren = _children[i]->getRoute(req, current + 1, end);
            if (existInChildren) {
                return existInChildren;
            }
        }
    }
    return hr::emptyMiddleware;
}

const mp& HttpRouteRootNode::getRoute(Request& req,
                                      const char* current,
                                      const char* end) const THROWS
{
    if (!_nodeName.empty()) {
        for (const char* my = _nodeName.c_str(); *my; ++my, ++current) {
            if (current == end) {
                return HttpRouter::emptyMiddleware;
            }
            if (*my != *current) {
                return HttpRouter::emptyMiddleware;
            }
        }
    }
    if (*current == '/') {
        // prefix check success
        for (int i = 0; i < _childrenCount; i++) {
            auto& existInChildren = _children[i]->getRoute(req, current + 1, end);
            if (existInChildren) {
                return existInChildren;
            }
        }
    }
    return hr::emptyMiddleware;
}

const mp& HttpRouteEndNode::getRoute(Request& req,
                                     const char* current,
                                     const char* end) const THROWS
{
    if (current >= end && req.getMethod() == method) {
        return func;
    }
    return hr::emptyMiddleware;
}

const mp& HttpRouteParamNode::getRoute(Request& req,
                                       const char* current,
                                       const char* end) const THROWS
{
    if (current >= end) {
        return hr::emptyMiddleware;
    } else {
        auto partend = current;
        for (; *partend != '/' && *partend && partend < end; ++partend) {
        }
        string param(current, partend);

        if (_regex->match(param)) {
            for (int i = 0; i < _childrenCount; i++) {
                auto n = partend + 1;
                auto& existInChildren = _children[i]->getRoute(req, n, end);
                if (existInChildren) {
                    req.addParam(_paramName, param);
                    return existInChildren;
                }
            }
        }
    }
    return hr::emptyMiddleware;
}


void hr::swap(hr& other)
{
    std::swap(start, other.start);
    std::swap(middles, other.middles);
}

hr::HttpRouter(hr&& hr) : HttpRouter()
{
    this->swap(hr);
}

hr::HttpRouter()
{
    start = nullptr;
}

HttpRouter::HttpRouter(HttpRouteBuilder&& b)
{
    this->start = b.build();
    middles.swap(b.middles);
}

hr::~HttpRouter()
{
    for (auto& p : middles) {
        delete p;
    }
    delete start;
}

builder::reference builder::withURLPrefix(const std::string& prefix)
{
    root->_myNodeName = prefix;
    return *this;
}

builder::~HttpRouteBuilder()
{
    delete root;
}

builder::TreeNode::~TreeNode()
{
    for (auto c : _children) {
        delete c;
    }
}

bool builder::TreeNode::operator==(const std::string& cmp)
{
    return _myNodeName == cmp;
}