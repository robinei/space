#include "render/opengl.h"
#include "render/statecontext.h"
#include "util/fixedhashtable.h"
#include "deps/mtrand.h"
#include <cstring>

#define Foreach_Enabled(X) \
    X(GL_BLEND) \
    X(GL_CLIP_DISTANCE0) \
    X(GL_CLIP_DISTANCE1) \
    X(GL_CLIP_DISTANCE2) \
    X(GL_CLIP_DISTANCE3) \
    X(GL_CLIP_DISTANCE4) \
    X(GL_CLIP_DISTANCE5) \
    X(GL_COLOR_LOGIC_OP) \
    X(GL_CULL_FACE) \
    X(GL_DEBUG_OUTPUT) \
    X(GL_DEBUG_OUTPUT_SYNCHRONOUS) \
    X(GL_DEPTH_CLAMP) \
    X(GL_DEPTH_TEST) \
    X(GL_DITHER) \
    X(GL_FRAMEBUFFER_SRGB) \
    X(GL_LINE_SMOOTH) \
    X(GL_MULTISAMPLE) \
    X(GL_POLYGON_OFFSET_FILL) \
    X(GL_POLYGON_OFFSET_LINE) \
    X(GL_POLYGON_OFFSET_POINT) \
    X(GL_POLYGON_SMOOTH) \
    X(GL_PRIMITIVE_RESTART) \
    X(GL_PRIMITIVE_RESTART_FIXED_INDEX) \
    X(GL_RASTERIZER_DISCARD) \
    X(GL_SAMPLE_ALPHA_TO_COVERAGE) \
    X(GL_SAMPLE_ALPHA_TO_ONE) \
    X(GL_SAMPLE_COVERAGE) \
    X(GL_SAMPLE_SHADING) \
    X(GL_SAMPLE_MASK) \
    X(GL_SCISSOR_TEST) \
    X(GL_STENCIL_TEST) \
    X(GL_TEXTURE_CUBE_MAP_SEAMLESS) \
    X(GL_PROGRAM_POINT_SIZE)


typedef std::pair<unsigned int, unsigned int> EnumIndex;

struct EnumIndexKey {
    static unsigned int key(EnumIndex e) {
        return e.first;
    }
};

typedef FixedHashTable<6, EnumIndex, EnumIndexKey> EnumIndexTable;


static EnumIndexTable enabled_indexes;


#define DefineEnum(Enum) _enum_##Enum,
enum {
    Foreach_Enabled(DefineEnum)
    Enabled_Max
};

static_assert(Enabled_Max < 64, "to many enabled states");

#define ListEnum(Enum) Enum,
GLenum enabled_values[Enabled_Max] = {
    Foreach_Enabled(ListEnum)
};


#define InsertEnum(Enum) enabled_indexes.insert(EnumIndex(Enum, _enum_##Enum));
static void init_enabled_indexes() {
    Foreach_Enabled(InsertEnum);
    MTRand_int32 rnd;
    enabled_indexes.optimize(rnd);
}

static unsigned int enabled_index(GLenum x) {
    EnumIndex val = enabled_indexes.lookup(x);
    assert(val.first == x);
    assert(enabled_values[val.second] == x);
    return val.second;
}



static StateContext *active = nullptr;

StateContext::StateContext() {
    if (active) {
        *this = *active;
    } else {
        init_enabled_indexes();
        load_state();
    }
    prev = active;
    active = this;
}

StateContext::~StateContext() {
    active = prev;
    if (!prev)
        return;

    if (!memcmp(&state, &prev->state, sizeof(State)))
        return;

    if (state.depth_mask != prev->state.depth_mask)
        glDepthMask(prev->state.depth_mask);
    
    if (state.depth_func != prev->state.depth_func)
        glDepthFunc(prev->state.depth_func);
    
    if (state.cull_face != prev->state.cull_face)
        glCullFace(prev->state.cull_face);
    
    if (state.src_rgb != prev->state.src_rgb ||
        state.dst_rgb != prev->state.dst_rgb ||
        state.src_alpha != prev->state.src_alpha ||
        state.dst_alpha != prev->state.dst_alpha)
        glBlendFuncSeparate(prev->state.src_rgb, prev->state.dst_rgb,
                            prev->state.src_alpha, prev->state.dst_alpha);
    
    if (state.enabled != prev->state.enabled) {
        for (unsigned int i = 0; i < Enabled_Max; ++i) {
            uint64_t flag = (1LL << i);
            if ((state.enabled & flag) != (prev->state.enabled & flag)) {
                if (prev->state.enabled & flag)
                    glEnable(enabled_values[i]);
                else
                    glDisable(enabled_values[i]);
            }
        }
    }
}


void StateContext::enable(GLenum cap, bool value) {
    int i = enabled_index(cap);
    uint64_t flag = 1LL << i;
    if (((state.enabled & flag) != 0) != value) {
        if (value) {
            glEnable(cap);
            state.enabled |= flag;
        } else {
            glDisable(cap);
            state.enabled &= ~flag;
        }
    }
}

void StateContext::disable(GLenum cap) {
    enable(cap, false);
}

bool StateContext::enabled(GLenum cap) {
    int i = enabled_index(cap);
    uint64_t flag = 1LL << i;
    return (state.enabled & flag) != 0;
}

void StateContext::depth_mask(GLboolean flag) {
    if (state.depth_mask != flag) {
        state.depth_mask = flag;
        glDepthMask(flag);
    }
}

GLboolean StateContext::depth_mask() {
    return state.depth_mask;
}

void StateContext::depth_func(GLenum func) {
    if (state.depth_func != func) {
        state.depth_func = func;
        glDepthFunc(func);
    }
}

GLenum StateContext::depth_func() {
    return state.depth_func;
}

void StateContext::cull_face(GLenum mode) {
    if (state.cull_face != mode) {
        state.cull_face = mode;
        glCullFace(mode);
    }
}

GLenum StateContext::cull_face() {
    return state.cull_face;
}

void StateContext::blend_func(GLenum src, GLenum dst) {
    blend_func_separate(src, dst, src, dst);
}

void StateContext::blend_func_separate(GLenum src_rgb, GLenum dst_rgb,
                                       GLenum src_alpha, GLenum dst_alpha) {
    if (src_rgb != state.src_rgb || dst_rgb != state.dst_rgb ||
        src_alpha != state.src_alpha || dst_alpha != state.dst_alpha) {
        state.src_rgb = src_rgb;
        state.dst_rgb = dst_rgb;
        state.src_alpha = src_alpha;
        state.dst_alpha = dst_alpha;
        glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
    }
}

GLenum StateContext::blend_src_rgb() {
    return state.src_rgb;
}

GLenum StateContext::blend_dst_rgb() {
    return state.dst_rgb;
}

GLenum StateContext::blend_src_alpha() {
    return state.src_alpha;
}

GLenum StateContext::blend_dst_alpha() {
    return state.dst_alpha;
}

void StateContext::load_state() {
    GLint int_value;

    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depth_mask);

    glGetIntegerv(GL_DEPTH_FUNC, &int_value);
    state.depth_func = int_value;

    glGetIntegerv(GL_CULL_FACE_MODE, &int_value);
    state.cull_face = int_value;

    glGetIntegerv(GL_BLEND_SRC_RGB, &int_value);
    state.src_rgb = int_value;

    glGetIntegerv(GL_BLEND_DST_RGB, &int_value);
    state.dst_rgb = int_value;

    glGetIntegerv(GL_BLEND_SRC_ALPHA, &int_value);
    state.src_alpha = int_value;

    glGetIntegerv(GL_BLEND_DST_ALPHA, &int_value);
    state.dst_alpha = int_value;

    state.enabled = 0;
    for (unsigned int i = 0; i < Enabled_Max; ++i) {
        if (glIsEnabled(enabled_values[i])) {
            uint64_t flag = 1LL << i;
            state.enabled |= flag;
        }
    }
}
