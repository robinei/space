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

    void blend_func(GLenum src, GLenum dst);
    void blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha);
    GLenum blend_src_rgb();
    GLenum blend_dst_rgb();
    GLenum blend_src_alpha();
    GLenum blend_dst_alpha();

private:
    void load_state();

    struct State {
        GLboolean depth_mask;
        GLenum depth_func;
        GLenum cull_face;
        GLenum src_rgb, dst_rgb, src_alpha, dst_alpha;
        uint64_t enabled;
    };

    State state;
    StateContext *prev;
};

#endif
