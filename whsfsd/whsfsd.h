
#ifndef WHSFSD_H
#define WHSFSD_H
#include <cstring>
#include <http_parser.h>

inline char* dup_memory(const void* buffer, size_t size)
{
    auto ret = new char[size];
    memcpy(ret, buffer, size);
    return ret;
}

#endif