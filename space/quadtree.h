#ifndef QUADTREE_H
#define QUADTREE_H

#include "list.h"
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

        virtual void qtree_position(float &x, float &y) = 0;

    private:
        friend class QuadTree;
        friend class Node;
        class QuadTree::Node *qtree_node;
        ListLink qtree_link;
    };

    QuadTree(float x0, float y0, float x1, float y1, int max_depth);

    void insert(Object *obj);
    void remove(Object *obj);

    template <class Func>
    void query(float x0, float y0, float x1, float y1, Func func) {
        root->query(x0, y0, x1, y1, func);
    }

    template <class Func>
    void gather_outlines(Func func) {
        // generate the outside edges of the root:
        func(root->x0, root->y0); func(root->x1, root->y0);
        func(root->x0, root->y0); func(root->x0, root->y1);
        func(root->x1, root->y1); func(root->x0, root->y1);
        func(root->x1, root->y1); func(root->x1, root->y0);

        // generate crosses (two lines splitting the four children)
        // for all nodes that have children
        root->gather_crosses(func);
    }

private:
    class Node {
    public:
        float x0, y0, x1, y1;
        float center_x, center_y;
        QuadTree *qtree;
        Node *parent;
        int depth;

        Node *child[4];

        List<Object, &Object::qtree_link> objects;
        int num_objects;


        void remove(Object *obj) {
            obj->qtree_link.unlink();
            obj->qtree_node = nullptr;
            --num_objects;
        }

        int calc_index(float x, float y) {
            if (y < center_y)
                return x < center_x ? 0 : 1;
            return x < center_x ? 2 : 3;
        }

        Node *calc_child(float x, float y) {
            return child[calc_index(x, y)];
        }

        template <class Func>
        void query(float x0, float y0, float x1, float y1, Func func) {
            if (child[0]) {
                if (y0 < child[0]->y1) {
                    if (x0 < child[0]->x1) child[0]->query(x0, y0, x1, y1, func);
                    if (x1 > child[1]->x0) child[1]->query(x0, y0, x1, y1, func);
                }
                if (y1 > child[2]->y0) {
                    if (x0 < child[2]->x1) child[2]->query(x0, y0, x1, y1, func);
                    if (x1 > child[3]->x0) child[3]->query(x0, y0, x1, y1, func);
                }
            } else {
                for (Object *obj : objects)
                    func(obj);
            }
        }

        template <class Func>
        void gather_crosses(Func func) {
            if (!child[0])
                return;

            func(child[2]->x0, child[2]->y0); func(child[1]->x1, child[1]->y1);
            func(child[1]->x0, child[1]->y0); func(child[2]->x1, child[2]->y1);

            for (int i = 0; i < 4; ++i)
                child[i]->gather_crosses(func);
        }
    };

    // non-copyable
    QuadTree(const QuadTree &);
    QuadTree &operator=(const QuadTree &);

    void insert(Node *n, Object *obj);
    void maybe_merge_with_siblings(Node *n);

    Node *new_node(Node *parent, float x0, float y0, float x1, float y1);
    void free_node(Node *n);

    Pool<Node> pool;
    int max_depth;
    Node *root;
};


#endif
