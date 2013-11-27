#ifndef QUADTREE_H
#define QUADTREE_H

#include "listlink.h"
#include "mymath.h"
#include "arena.h"
#include <vector>

class QuadTree {
public:
    // objects that want to be stored in the tree must derive from this
    class Object {
    public:
        Object() : qtree_node(nullptr) {}
        virtual ~Object() { qtree_remove(); }

        void qtree_remove();
        void qtree_update(); // call after position has changed

        virtual vec2 qtree_position() = 0;

    private:
        friend class QuadTree;
        friend class Node;
        class Node *qtree_node;
        ListLink qtree_link;
    };

    QuadTree(Rect rect, int max_depth);

    void insert(Object *obj);
    void remove(Object *obj);

    void query(Rect rect, std::vector<Object *> &result);
    void query(vec2 pos, float radius, std::vector<Object *> &result);

    void gather_outlines(std::vector<vec2> &lines);

private:
    // non-copyable
    QuadTree(const QuadTree &);
    QuadTree &operator=(const QuadTree &);

    bool insert(class Node *n, Object *obj);

    class Node *new_node(class Node *parent, Rect rect);
    void free_node(class Node *n);

    Arena arena;
    int max_depth;
    class Node *freelist;
    class Node *root;
};


#endif
