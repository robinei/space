#ifndef QUADTREE_H
#define QUADTREE_H

#include "list.h"
#include "mymath.h"
#include "pool.h"

class QuadTree {
    class Node;

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
        class QuadTree::Node *qtree_node;
        ListLink qtree_link;
    };

    QuadTree(Rect rect, int max_depth);

    void insert(Object *obj);
    void remove(Object *obj);

    template <class Func>
    void query(float x0, float y0, float x1, float y1, Func func) {
        root->query(x0, y0, x1, y1, func);
    }

    template <class Func>
    void gather_outlines(Func func) {
        Rect rect = root->rect;

        // generate the outside edges of the root:

        func(rect.min.x, rect.min.y);
        func(rect.max.x, rect.min.y);

        func(rect.min.x, rect.min.y);
        func(rect.min.x, rect.max.y);

        func(rect.max.x, rect.max.y);
        func(rect.min.x, rect.max.y);

        func(rect.max.x, rect.max.y);
        func(rect.max.x, rect.min.y);

        // generate crosses (two lines splitting the four children)
        // for all nodes that have children
        root->gather_crosses(func);
    }

private:
    class Node {
    public:
        typedef List<Object, &Object::qtree_link> ObjectList;

        Rect rect;
        vec2 center;
        QuadTree *qtree;
        Node *parent;
        int depth;

        Node *child[4];

        ObjectList objects;
        int num_objects;


        void remove(Object *obj) {
            obj->qtree_link.unlink();
            obj->qtree_node = nullptr;
            --num_objects;
        }

        int calc_index(vec2 pos) {
            if (pos.y < center.y)
                return pos.x < center.x ? 0 : 1;
            return pos.x < center.x ? 2 : 3;
        }

        Node *calc_child(vec2 pos) {
            return child[calc_index(pos)];
        }

        template <class Func>
        void query(float x0, float y0, float x1, float y1, Func func) {
            if (child[0]) {
                if (y0 < child[0]->rect.max.y) {
                    if (x0 < child[0]->rect.max.x) child[0]->query(x0, y0, x1, y1, func);
                    if (x1 > child[1]->rect.min.x) child[1]->query(x0, y0, x1, y1, func);
                }
                if (y1 > child[2]->rect.min.y) {
                    if (x0 < child[2]->rect.max.x) child[2]->query(x0, y0, x1, y1, func);
                    if (x1 > child[3]->rect.min.x) child[3]->query(x0, y0, x1, y1, func);
                }
            } else {
                for (Object *obj : objects) {
                    func(obj);
                }
            }
        }

        template <class Func>
        void gather_crosses(Func func) {
            if (!child[0])
                return;

            func(child[2]->rect.min.x, child[2]->rect.min.y);
            func(child[1]->rect.max.x, child[1]->rect.max.y);

            func(child[1]->rect.min.x, child[1]->rect.min.y);
            func(child[2]->rect.max.x, child[2]->rect.max.y);

            for (int i = 0; i < 4; ++i)
                child[i]->gather_crosses(func);
        }
    };

    // non-copyable
    QuadTree(const QuadTree &);
    QuadTree &operator=(const QuadTree &);

    void insert(class Node *n, Object *obj);
    void maybe_merge_with_siblings(class Node *n);

    class Node *new_node(class Node *parent, Rect rect);
    void free_node(class Node *n);

    Pool<class Node> pool;
    int max_depth;
    class Node *root;
};


#endif
