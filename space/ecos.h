#ifndef ECOS_H
#define ECOS_H

#include "fixedhashtable.h"
#include "pool.h"
#include "mtrand.h"
#include <vector>

class Entity;
class EntityManager;

typedef unsigned int SystemType;
typedef unsigned int ComponentType;


class System {
public:
    ~System() {}
    virtual SystemType type() = 0;
};


class Component {
public:
    virtual ~Component() {}
    virtual ComponentType type() = 0;
    virtual void destroy(EntityManager *m) = 0;
    virtual void init(EntityManager *m, Entity *e) {}
};


class Entity {
public:
    Component *get_component(ComponentType type);

    template <class T>
    T *get_component() {
        return static_cast<T *>(get_component(T::TYPE));
    }

    // entities that are to be destroyed will live for exactly one frame
    // tith dying() == true, before being destroyed
    bool dying() { return _dying;  }

private:
    friend class EntityManager;
    template <class T> friend class IterablePool;

    Entity() : _dying(false) {}

    struct ComponentHashKey {
        static unsigned int key(Component *c) { return c->type(); }
    };
    typedef FixedHashTable<3, Component *, ComponentHashKey> ComponentTable;

    // We store component refs in an associative container that has the
    // structure of a linked list of fixed size hash tables using open
    // addressing scheme.
    // We use universal hashing to ensure that most components fall into the
    // exact bucket that their type hashes to, meaning that probe lengths are
    // usually 0.
    // Chained blocks are independent, so lookup operations must try all
    // blocks until they find the type they are looking for, or fail.
    // Table size should be set so that only one or two blocks are needed
    // per entity.
    struct ComponentBlock {
        ComponentTable table;
        ComponentBlock *next; // if the table overflows, we alloc a new block.

        ComponentBlock(ComponentBlock *next = nullptr) : next(next) {}
    };

    bool _dying;
    ComponentBlock block; // an entity always has one embedded block
};


class EntityManager {
public:
    ~EntityManager();

    void update();

    Entity *create_entity();
    void destroy_entity(Entity *e);
    void optimize_entity(Entity *e);
    void init_entity(Entity *e);

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

    IterablePool<Entity>::iterator begin() { return entity_pool.begin(); }
    IterablePool<Entity>::iterator end() { return entity_pool.end(); }

private:
    void really_destroy_entity(Entity *e);
    void add_component(Entity::ComponentBlock *block, Component *c);

    struct SystemHashKey {
        static unsigned int key(System *c) { return c->type(); }
    };
    typedef FixedHashTable<8, System *, SystemHashKey> SystemTable;

    MTRand_int32 rnd; // used to generate universal hash coefficients
    Pool<Entity::ComponentBlock> block_pool;
    IterablePool<Entity> entity_pool;
    SystemTable systems;

    std::vector<Entity *> kill_next_time;
    std::vector<Entity *> kill_this_time;
};

#endif
