#ifndef STATECONTEXT_H
#define STATECONTEXT_H

// requires: opengl.h
#include <cstdint>

class StateContext {
public:
    StateContext();
    ~StateContext();

    void enable(GLenum cap, bool value = true);
    void disable(GLenum cap);
    bool enabled(GLenum cap);

    void depth_mask(GLboolean flag);
    GLboolean depth_mask();

    void depth_func(GLenum func);
    GLenum depth_func();

    void cull_face(GLenum mode);
    GLenum cull_face();

private:
    void load_state();

    GLboolean _depth_mask;
    GLenum _depth_func;
    GLenum _cull_face;
    uint64_t _enabled;

    StateContext *prev;
};

#endif
