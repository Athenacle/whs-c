
#include "whs/builder.h"
#include "whs-internal.h"


whs::Pipeline::Pipeline(const PipelineBuilder& b)
{
    auto _wares = b._wares;
    auto c = _wares.size();
    this->wares = new Middleware*[c + 1];
    for (uint32_t i = 0; i < c; ++i) {
        this->wares[i] = _wares[i].second;
    }
    this->wares[c] = nullptr;
}

whs::Pipeline::Pipeline()
{
    wares = new Middleware*[1];
    wares[0] = nullptr;
}

whs::Pipeline::~Pipeline()
{
    if (wares != nullptr) {
        for (auto first = wares; *first != nullptr; ++first) {
            delete *first;
        }
        delete[] wares;
    }
}

void whs::Pipeline::addMiddleware(Middleware* m)
{
    int i = 0;
    for (i = 0; wares[i] != nullptr; ++i) {
    }
    assert(wares[i] == nullptr);
    auto w = new Middleware*[i + 2];
    memcpy(w, wares, i * sizeof(*wares));
    w[i] = m;
    w[i + 1] = nullptr;
    delete[] wares;
    wares = w;
}