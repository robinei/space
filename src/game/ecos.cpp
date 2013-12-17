#include "ecos.h"
#include <cassert>




Component *Entity::get_component(ComponentType type) {
    ComponentBlock *b = &block;
    do {
        Component *c = b->table.lookup(type);
        if (c)
            return c;
        b = b->next;
    } while (b);
    return nullptr;
}




EntityManager::~EntityManager() {
    for (Entity *e : entity_pool)
        really_destroy_entity(e);
}

void EntityManager::update() {
    for (Entity *e : kill_this_time)
        really_destroy_entity(e);
    kill_this_time = kill_next_time;
    kill_next_time.clear();
}

Entity *EntityManager::create_entity() {
    return entity_pool.create();
}

void EntityManager::destroy_entity(Entity *e) {
    e->_dying = true;
    kill_next_time.push_back(e);
}

void EntityManager::really_destroy_entity(Entity *e) {
    Entity::ComponentBlock *b = &e->block;
    do {
        Entity::ComponentBlock *next = b->next;
        for (Component *c : b->table)
            c->destroy(this);
        if (b != &e->block) // don't free embedded block
            block_pool.free(b);
        b = next;
    } while (b);
    entity_pool.free(e);
}

void EntityManager::optimize_entity(Entity *e) {
    Entity::ComponentBlock *b = &e->block;
    do {
        b->table.optimize(rnd);
        b = b->next;
    } while (b);
}

void EntityManager::init_entity(Entity *e) {
    Entity::ComponentBlock *b = &e->block;
    do {
        for (Component *c : b->table)
            c->init(this, e);
        b = b->next;
    } while (b);
}

void EntityManager::add_component(Entity *e, Component *c) {
    add_component(&e->block, c);
}

void EntityManager::del_component(Entity *e, ComponentType type) {
    Entity::ComponentBlock *b = &e->block;
    do {
        Component *c = b->table.remove(type);
        if (c) {
            b->table.rehash();
            c->destroy(this);
            return;
        }
        b = b->next;
    } while (b);
}

void EntityManager::add_component(Entity::ComponentBlock *block, Component *c) {
    Entity::ComponentBlock *b = block;

    do {
        if (b->table.insert(c))
            return;
        b = b->next;
    } while (b);

    block->next = block_pool.create(block->next);
    add_component(block->next, c);
}

System *EntityManager::get_system(SystemType type) {
    return systems.lookup(type);
}

void EntityManager::add_system(System *s) {
    bool ok = systems.insert(s);
    assert(ok);
}

void EntityManager::optimize_systems() {
    systems.optimize(rnd);
}
