#ifndef WHS_UTILS_H
#define WHS_UTILS_H

#include <cstdlib>

#include "config.h"

namespace whsutils
{
    class MemoryBuffer
    {
        static const size_t _pageSize = 512;

        struct _Storage {
            _Storage(const _Storage &) = delete;

            char *_ptr;
            char *_end;
            char *_begin;

            ~_Storage()
            {
                if (_ptr) {
                    delete[] _ptr;
                }
            }

            size_t remainSize() const
            {
                return _ptr + _pageSize - _end;
            }

            size_t storedSize() const
            {
                return _end - _begin;
            }

            _Storage(_Storage &&s)
            {
                _ptr = s._ptr;
                _begin = s._begin;
                _end = s._end;
                s._ptr = nullptr;
            }

            _Storage()
            {
                _begin = _end = _ptr = new char[_pageSize];
            }

            size_t write(const char *, size_t);
            size_t read(char *, size_t);
        };

        std::deque<_Storage> _pool;

        size_t _stored;
        size_t _gcount;

    public:
        size_t gcount() const
        {
            return _gcount;
        }
        operator bool()
        {
            return true;
        }

        MemoryBuffer() : _stored(0), _gcount(0) {}

        size_t write(const char *, size_t);
        size_t read(char *, size_t);
    };
}  // namespace whsutils


#endif