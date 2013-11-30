#ifndef POOL_H
#define POOL_H

#include <vector>
#include <cassert>

template <class T>
class Pool {
    struct Block {
        Block *next;
        int num_objects;
        T *objects;
    };

public:
    Pool(int initial_size = 16) :
        blocks(new_block(nullptr, initial_size)),
        block_index(0) {}

    ~Pool() {
        while (blocks) {
            Block *next = blocks->next;
            ::free(blocks);
            blocks = next;
        }
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
            blocks = new_block(blocks, blocks->num_objects * 2);
            block_index = 0;
        }
        return &blocks->objects[block_index++];
    }

    Block *new_block(Block *next, int num_objects) {
        assert(num_objects > 0);
        Block *b = (Block *)::malloc(sizeof(Block)+sizeof(T)*num_objects);
        b->next = next;
        b->num_objects = num_objects;
        b->objects = (T *)((char *)b + sizeof(Block));
        return b;
    }

    std::vector<T *> freelist;
    Block *blocks;
    int block_index;
};


template <class T>
class IterablePool {
    typedef std::vector<T *> Freelist;

    struct Block {
        Block *next;
        int offset;
        int index;
        int num_objects;
        char *livemap;
        T *objects;
        Freelist freelist;

        T *freelist_alloc() {
            if (freelist.empty())
                return nullptr;
            T *obj = freelist.back();
            freelist.pop_back();
            livemap[obj - objects] = 1;
            return obj;
        }
    };

public:
    typedef T *value_type;

    class iterator {
    public:
        typedef T *value_type;

        T *operator*() { return block->objects + index; }
        T *operator->() { return block->objects + index; }

        bool operator==(iterator it) const { return index == it.index && block == it.block; }
        bool operator!=(iterator it) const { return !(*this == it); }

        iterator &operator++() {
            while (block) {
                if (++index >= block->offset + block->index) {
                    index = -1;
                    block = block->next;
                } else if (block->livemap[index]) {
                    break;
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator it(*this);
            ++*this;
            return it;
        }
    private:
        friend class IterablePool;

        iterator(Block *block, int index) : block(block), index(index) {
            ++*this;
        }

        Block *block;
        int index;
    };

    iterator begin() { return iterator(blocks, -1); }
    iterator end() { return iterator(nullptr, -1); }

    IterablePool(int initial_size = 16) :
        freelist_block(nullptr),
        blocks(new_block(nullptr, initial_size)),
        count(0)
    {}

    ~IterablePool() {
        if (count) {
            for (T *obj : *this)
                obj->~T();
        }
        while (blocks) {
            Block *next = blocks->next;
            (blocks->freelist).~Freelist();
            ::free(blocks);
            blocks = next;
        }
    }

    template<typename ...Args>
    T *create(Args&&... params) {
        ++count;
        T *obj = alloc();
        return new (obj)T(std::forward<Args>(params)...);
    }

    void free(T *obj) {
        --count;
        assert(count >= 0);
        obj->~T();
        Block *b = find_block(obj);
        b->freelist.push_back(obj);
        char *alive = &b->livemap[obj - b->objects];
        assert(*alive);
        *alive = 0;
        if (!freelist_block)
            freelist_block = b;
    }

    int size() {
        return count;
    }

private:
    T *alloc() {
        if (freelist_block) {
            T *obj = freelist_block->freelist_alloc();
            if (obj)
                return obj;
            freelist_block = blocks;
            while (freelist_block) {
                obj = freelist_block->freelist_alloc();
                if (obj)
                    return obj;
            }
        }

        if (blocks->index == blocks->num_objects) {
            blocks = new_block(blocks, blocks->num_objects * 2);
        }

        T *obj = &blocks->objects[blocks->index];
        blocks->livemap[blocks->index++] = 1;
        return obj;
    }

    Block *new_block(Block *next, int num_objects) {
        assert(num_objects > 0);
        assert(!next || (next->index == next->num_objects));
        int livemap_offset = sizeof(Block)+sizeof(T)*num_objects;
        Block *b = (Block *)::malloc(livemap_offset + num_objects);
        b->next = next;
        b->offset = next ? next->offset + next->num_objects : 0;
        b->index = 0;
        b->num_objects = num_objects;
        b->objects = (T *)((char *)b + sizeof(Block));
        b->livemap = (char *)b + livemap_offset;
        memset(b->livemap, 0, num_objects);
        new (&b->freelist) Freelist;
        return b;
    }

    Block *find_block(T *obj) {
        Block *b = blocks;
        while (b) {
            if (obj >= b->objects && obj < b->objects + b->num_objects)
                return b;
            b = b->next;
        }
        assert(0 && "object not allocated from this pool!");
        return nullptr;
    }

    Block *freelist_block;
    Block *blocks;
    int count;
};

#endif
