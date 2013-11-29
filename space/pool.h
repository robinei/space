#ifndef POOL_H
#define POOL_H

template <class T>
class Pool {
    struct Object {
        union {
            Pool *pool;
            Object *next;
        };
        T value;
    };

    struct Block {
        Block *next;
        int num_objects;
        Object *objects;

        Block(Block *next, int num_objects) :
            next(next),
            num_objects(num_objects),
            objects((Object *)::malloc(sizeof(Object)*num_objects))
        {}
        ~Block() {
            ::free(objects);
            delete next;
        }
    };

public:
    Pool() :
        freelist(nullptr),
        blocks(new Block(nullptr, 16)),
        block_index(0)
    {}

    ~Pool() {
        delete blocks;
    }

    template<typename ...Args>
    T *create(Args&&... params) {
        Object *obj = new_object();
        return new (&obj->value)T(std::forward<Args>(params)...);
    }

    static void free(T *value) {
        Object *obj = (Object *)((char *)value - sizeof(Pool *));
        obj->pool->free_object(obj);
    }

private:
    Object *new_object() {
        Object *obj = freelist;
        if (obj) {
            freelist = obj->next;
        } else {
            if (block_index == blocks->num_objects) {
                blocks = new Block(blocks, blocks->num_objects * 2);
                block_index = 0;
            }
            obj = &blocks->objects[block_index++];
        }
        obj->pool = this;
        return obj;
    }

    void free_object(Object *obj) {
        obj->value.~T();
        obj->next = freelist;
        freelist = obj;
    }

    Object *freelist;
    Block *blocks;
    int block_index;
};

template <class T>
void pool_free(T *value) {
    Pool<T>::free(value);
}

#endif
