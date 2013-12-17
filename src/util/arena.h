#ifndef ARENA_H
#define ARENA_H

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>

class Arena {
    enum { MAX_BUFFER_SIZE = 1024*1024*2 };
public:
    Arena(int initial_size = 1024*64, int growth_factor = 150)
        : initial_size(initial_size), curr_buffer(nullptr),
          curr_used(0), curr_size(0), growth_factor(growth_factor)
    {
        assert(initial_size <= MAX_BUFFER_SIZE);
        assert(growth_factor <= 500);
        assert(growth_factor >= 100);
    }

    ~Arena() {
        clear();
    }

    void clear() {
        if (curr_buffer) {
            delete[] curr_buffer;
            curr_buffer = nullptr;
            curr_used = 0;
            curr_size = 0;
            for (size_t i = 0; i < buffers.size(); ++i)
                delete[] buffers[i];
            buffers.clear();
        }
    }

    template <class T>
    T *alloc() {
        void *buf = alloc(sizeof(T));
        return new (buf) T();
    }

    template<class T, typename ...Args>
    T *alloc(Args&&... params) {
        void *buf = alloc(sizeof(T));
        return new (buf) T(std::forward<Args>(params)...);
    }

    void *alloc(int size) {
        if (curr_used + size < curr_size) {
            // maybe help simple branch predictors...
        } else {
            assert(size <= MAX_BUFFER_SIZE);
            if (curr_size == 0)
                curr_size = initial_size;
            else
                curr_size = (curr_size * growth_factor) / 100;
            if (curr_size > MAX_BUFFER_SIZE)
                curr_size = MAX_BUFFER_SIZE;
            if (size > curr_size)
                curr_size = size;
            if (curr_buffer)
                buffers.push_back(curr_buffer);
            curr_used = 0;
            curr_buffer = new char[curr_size];
        }
        char *result = curr_buffer + curr_used;
        curr_used += size;
        return result;
    }

    void *alloc0(int size) {
        void *buf = alloc(size);
        memset(buf, 0, size);
        return buf;
    }

    char *copy_string(const char *str) {
        int len = strlen(str);
        char *buf = (char *)alloc(len + 1);
        memcpy(buf, str, len + 1);
        return buf;
    }

private:
    int initial_size;
    char *curr_buffer;
    int curr_used;
    int curr_size;
    int growth_factor;
    std::vector<char *> buffers;
};

#endif
