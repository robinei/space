#include "quadtree.h"
#include "list.h"


enum {
    SPLIT_THRESHOLD = 3,
    MERGE_THRESHOLD = 1
};

#define FIT_FUZZ 0.0001f



class Node {
public:
    typedef List<QuadTree::Object, &QuadTree::Object::qtree_link> ObjectList;

    Rect rect;
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

    void query(Rect rect, std::vector<QuadTree::Object *> &result) {
        if (child[0]) {
            for (int i = 0; i < 4; ++i)
            if (child[i]->rect.intersects(rect))
                child[i]->query(rect, result);
        } else {
            for (QuadTree::Object *obj : objects) {
                if (rect.contains(obj->qtree_position()))
                    result.push_back(obj);
            }
        }
    }

    void query(Rect rect, vec2 pos, float radius, std::vector<QuadTree::Object *> &result) {
        if (child[0]) {
            for (int i = 0; i < 4; ++i)
            if (child[i]->rect.intersects(rect))
                child[i]->query(rect, pos, radius, result);
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
    if (qtree_node) qtree_node->qtree->remove(this);
}

void QuadTree::Object::qtree_update() {
    if (!qtree_node) return;
    if (qtree_node->rect.contains(qtree_position(), FIT_FUZZ)) return;
    QuadTree *qtree = qtree_node->qtree;
    qtree->remove(this);
    qtree->insert(this);
}



QuadTree::QuadTree(Rect rect, int max_depth) : max_depth(max_depth), freelist(nullptr) {
    root = new_node(nullptr, rect);
}

void QuadTree::insert(Object *obj) {
    insert(root, obj);
}

bool QuadTree::insert(Node *n, Object *obj) {
    assert(!obj->qtree_node);
    assert(!obj->qtree_link.is_linked());

    if (!n->rect.contains(obj->qtree_position(), FIT_FUZZ))
        return false;

    if (!n->child[0]) {
        // we are a leaf node; check if there is space
        if (n->num_objects < SPLIT_THRESHOLD || n->depth == max_depth) {
            obj->qtree_node = n;
            n->objects.push_back(obj);
            ++n->num_objects;
            return true;
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
            Object *obj = n->objects.front();
            n->remove(obj);
            if (insert(n->child[0], obj)) continue;
            if (insert(n->child[1], obj)) continue;
            if (insert(n->child[2], obj)) continue;
            if (insert(n->child[3], obj)) continue;

            // all parents' objects should fit in the children
            assert(0);
        }
        assert(n->objects.empty());
    }

    // this is an internal node, so we recurse
    if (insert(n->child[0], obj)) return true;
    if (insert(n->child[1], obj)) return true;
    if (insert(n->child[2], obj)) return true;
    if (insert(n->child[3], obj)) return true;

    return false; // shouldn't happen!
}

void QuadTree::remove(Object *obj) {
    Node *n = obj->qtree_node;
    if (!n)
        return; // it has not been inserted yet, so nothing to do

    assert(!n->child[0]);

    n->remove(obj);

    // only when a removal leaves the count below or at MERGE_THRESHOLD do we
    // investigate merging the node with its siblings
    if (n->num_objects > MERGE_THRESHOLD)
        return;

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

    // if the count is greater than the threshold, the node should remain split
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
    Node *n = freelist;
    if (n)
        freelist = n->parent;
    else
        n = arena.alloc<Node>();
    n->rect = rect;
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
    n->parent = freelist;
    freelist = n;
}


