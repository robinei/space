#include "quadtree.h"
#include "list.h"


enum {
    SPLIT_THRESHOLD = 3,
    MERGE_THRESHOLD = 1
};



class Node {
public:
    typedef List<QuadTree::Object, &QuadTree::Object::qtree_link> ObjectList;

    Rect rect;
    vec2 center;
    QuadTree *qtree;
    Node *parent;
    int depth;

    Node *child[4];

    ObjectList objects;
    int num_objects;


    void remove(QuadTree::Object *obj) {
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

    void query(Rect rect, std::vector<QuadTree::Object *> &result) {
        if (child[0]) {
            if (rect.min.y < child[0]->rect.max.y) {
                if (rect.min.x < child[0]->rect.max.x) child[0]->query(rect, result);
                if (rect.max.x > child[1]->rect.min.x) child[1]->query(rect, result);
            }
            if (rect.max.y > child[2]->rect.min.y) {
                if (rect.min.x < child[2]->rect.max.x) child[2]->query(rect, result);
                if (rect.max.x > child[3]->rect.min.x) child[3]->query(rect, result);
            }
        } else {
            for (QuadTree::Object *obj : objects) {
                if (rect.contains(obj->qtree_position()))
                    result.push_back(obj);
            }
        }
    }

    void query(Rect rect, vec2 pos, float radius, std::vector<QuadTree::Object *> &result) {
        if (child[0]) {
            if (rect.min.y < child[0]->rect.max.y) {
                if (rect.min.x < child[0]->rect.max.x) child[0]->query(rect, pos, radius, result);
                if (rect.max.x > child[1]->rect.min.x) child[1]->query(rect, pos, radius, result);
            }
            if (rect.max.y > child[2]->rect.min.y) {
                if (rect.min.x < child[2]->rect.max.x) child[2]->query(rect, pos, radius, result);
                if (rect.max.x > child[3]->rect.min.x) child[3]->query(rect, pos, radius, result);
            }
        } else {
            for (QuadTree::Object *obj : objects) {
                vec2 d = obj->qtree_position() - pos;
                if (d.x*d.x + d.y*d.y < radius*radius)
                    result.push_back(obj);
            }
        }
    }

    void gather_outlines(std::vector<vec2> &lines) {
        lines.push_back(vec2(rect.min.x, rect.min.y));
        lines.push_back(vec2(rect.max.x, rect.min.y));

        lines.push_back(vec2(rect.min.x, rect.min.y));
        lines.push_back(vec2(rect.min.x, rect.max.y));

        lines.push_back(vec2(rect.max.x, rect.max.y));
        lines.push_back(vec2(rect.min.x, rect.max.y));

        lines.push_back(vec2(rect.max.x, rect.max.y));
        lines.push_back(vec2(rect.max.x, rect.min.y));

        if (child[0]) {
            for (int i = 0; i < 4; ++i)
                child[i]->gather_outlines(lines);
        }
    }
};



void QuadTree::Object::qtree_remove() {
    if (qtree_node)
        qtree_node->qtree->remove(this);
}

void QuadTree::Object::qtree_update() {
    if (qtree_node && !qtree_node->rect.contains(qtree_position())) {
        QuadTree *qtree = qtree_node->qtree;
        qtree->remove(this);
        qtree->insert(this);
    }
}



QuadTree::QuadTree(Rect rect, int max_depth) : max_depth(max_depth) {
    root = new_node(nullptr, rect);
}

void QuadTree::insert(Object *obj) {
    insert(root, obj);
}

void QuadTree::insert(Node *n, Object *obj) {
    assert(!obj->qtree_node);
    assert(!obj->qtree_link.is_linked());

    if (!n->child[0]) {
        // we are a leaf node; check if there is space
        if (n->num_objects < SPLIT_THRESHOLD || n->depth == max_depth) {
            obj->qtree_node = n;
            n->objects.push_back(obj);
            ++n->num_objects;
            return;
        }

        // this node is full; split into four children
        vec2 min(n->rect.min);
        vec2 max(n->rect.max);
        float w = (max.x - min.x) / 2.0f;
        float h = (max.y - min.y) / 2.0f;
        n->child[0] = new_node(n, Rect(min, vec2(min.x + w, min.y + h)));
        n->child[1] = new_node(n, Rect(vec2(min.x + w, min.y), vec2(max.x, min.y + h)));
        n->child[2] = new_node(n, Rect(vec2(min.x, min.y + h), vec2(min.x + w, max.y)));
        n->child[3] = new_node(n, Rect(vec2(min.x + w, min.y + h), max));

        // spread objects among children
        while (n->num_objects) {
            Object *obj2 = n->objects.front();
            n->remove(obj2);
            insert(n->calc_child(obj2->qtree_position()), obj2);
        }
        assert(n->objects.empty());
    }

    // this is an internal node, so we recurse
    insert(n->calc_child(obj->qtree_position()), obj);
}

void QuadTree::remove(Object *obj) {
    Node *n = obj->qtree_node;
    if (!n)
        return; // it has not been inserted yet, so nothing to do

    assert(!n->child[0]);

    n->remove(obj);
    
    // only when a removal leaves the count below or at MERGE_THRESHOLD do we
    // investigate merging the node with its siblings
    if (n->num_objects <= MERGE_THRESHOLD)
        maybe_merge_with_siblings(n);
}

void QuadTree::maybe_merge_with_siblings(Node *n) {
    Node *parent = n->parent;
    if (!parent)
        return; // can't merge any more since we're at the root node

    assert(parent->child[0]);
    assert(!parent->num_objects);
    assert(parent->objects.empty());

    // count all objects in all siblings
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        Node *c = parent->child[i];
        if (c->child[0])
            return; // we can't merge if parent has non-leaf children
        count += c->num_objects;
    }

    // if the count is greater than the split threshold,
    // the node should remain split
    if (count > SPLIT_THRESHOLD)
        return;

    // remove child nodes from parent
    Node *child[4];
    for (int i = 0; i < 4; ++i) {
        child[i] = parent->child[i];
        parent->child[i] = nullptr;
    }

    // insert all their objects into the parent, then release the nodes
    for (int i = 0; i < 4; ++i) {
        Node *c = child[i];
        while (c->num_objects) {
            Object *obj = c->objects.front();
            c->remove(obj);
            insert(parent, obj);
        }
        free_node(c);
    }

    // recurse here, since there may be opportunity for even more merging
    maybe_merge_with_siblings(parent);
}

void QuadTree::query(Rect rect, std::vector<Object *> &result) {
    root->query(rect, result);
}

void QuadTree::query(vec2 pos, float radius, std::vector<Object *> &result) {
    Rect rect;
    rect.min = pos - vec2(radius, radius);
    rect.max = pos + vec2(radius, radius);
    root->query(rect, pos, radius, result);
}

void QuadTree::gather_outlines(std::vector<vec2> &lines) {
    root->gather_outlines(lines);
}

Node *QuadTree::new_node(Node *parent, Rect rect) {
    Node *n = pool.create();
    n->rect = rect;
    n->center = rect.center();
    n->qtree = this;
    n->parent = parent;
    n->depth = parent ? parent->depth + 1 : 0;
    n->child[0] = nullptr;
    n->child[1] = nullptr;
    n->child[2] = nullptr;
    n->child[3] = nullptr;
    n->num_objects = 0;
    return n;
}

// insert node and all child nodes into freelist, chaining off "parent"
void QuadTree::free_node(Node *n) {
    assert(!n->child[0]);
    assert(n->objects.empty());
    pool.free(n);
}


