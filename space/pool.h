#ifndef POOL_H
#define POOL_H

#include <vector>

template <class T>
class Pool {
    struct Block {
        Block *next;
        int num_objects;
        T *objects;

        Block(Block *next, int num_objects) :
            next(next),
            num_objects(num_objects),
            objects((T *)::malloc(sizeof(T)*num_objects))
        {}
        ~Block() {
            ::free(objects);
            delete next;
        }
    };

public:
    Pool() :
        blocks(new Block(nullptr, 16)),
        block_index(0)
    {}

    ~Pool() {
        delete blocks;
    }

    template<typename ...Args>
    T *create(Args&&... params) {
        T *obj = alloc();
        return new (obj)T(std::forward<Args>(params)...);
    }

    void free(T *obj) {
        obj->~T();
        freelist.push_back(obj);
    }

private:
    T *alloc() {
        if (freelist.size()) {
            T *obj = freelist.back();
            freelist.pop_back();
            return obj;
        }
        if (block_index == blocks->num_objects) {
            blocks = new Block(blocks, blocks->num_objects * 2);
            block_index = 0;
        }
        return &blocks->objects[block_index++];
    }

    std::vector<T *> freelist;
    Block *blocks;
    int block_index;
};

#endif
