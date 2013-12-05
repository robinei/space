#ifndef ECOS_H
#define ECOS_H

#include "fixedhashtable.h"
#include "pool.h"
#include "mtrand.h"


class EntityManager;

typedef unsigned int ComponentType;

class Component {
public:
    virtual ~Component() {}
    virtual ComponentType type() = 0;
    virtual void destroy(EntityManager *m) = 0;
};


struct ComponentHashKey {
    static unsigned int key(Component *c) {
        return c->type();
    }
};
typedef FixedHashTable<3, Component *, ComponentHashKey> ComponentTable;


// we store component refs in an associative container that has the structure
// of a linked list of fixed size hash tables using open addressing scheme.
// we use universal hashing to ensure that most components fall into the exact
// bucket that their type hashes to, meaning that probe lengths are usually 0.
// chained blocks are independent, so lookup operations must try all blocks
// until they find the type they are looking for, or fail.
// table size should be set so that only one or two blocks are needed per entity.
struct ComponentBlock {
    ComponentTable table;
    ComponentBlock *next; // if the table overflows, we allocate a new block.

    ComponentBlock(ComponentBlock *next = nullptr) : next(next) {}
};


class Entity {
public:
    Component *get_component(ComponentType type);

    template <class T>
    T *get_component() {
        return static_cast<T *>(get_component(T::TYPE));
    }

private:
    friend class EntityManager;
    ComponentBlock block; // an entity always has one embedded component block
};



typedef unsigned int SystemType;

class System {
public:
    ~System() {}
    virtual SystemType type() = 0;
};

struct SystemHashKey {
    static unsigned int key(System *c) {
        return c->type();
    }
};
typedef FixedHashTable<8, System *, SystemHashKey> SystemTable;



class EntityManager {
public:
    ~EntityManager();

    Entity *create_entity();
    void destroy_entity(Entity *e);
    void optimize_entity(Entity *e);

    void add_component(Entity *e, Component *c);
    void del_component(Entity *e, ComponentType type);

    System *get_system(SystemType type);
    void add_system(System *s);
    void optimize_systems();

    template <class T>
    T *add_component(Entity *e) {
        T *c = T::create(this);
        add_component(e, c);
        return c;
    }

    template <class T>
    void del_component(Entity *e) {
        del_component(e, T::TYPE);
    }

    template <class T>
    T *get_system() {
        return static_cast<T *>(get_system(T::TYPE));
    }

private:
    void add_component(ComponentBlock *block, Component *c);

    MTRand_int32 rnd;
    Pool<ComponentBlock> block_pool;
    IterablePool<Entity> entity_pool;
    SystemTable systems;
};

#endif
