#include "quadtree.h"
#include "list.h"


enum {
    SPLIT_THRESHOLD = 3,
    MERGE_THRESHOLD = 1
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

QuadTree::Node *QuadTree::new_node(Node *parent, Rect rect) {
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


