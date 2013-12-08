#include "opengl.h"
#include "statecontext.h"
#include "fixedhashtable.h"
#include "mtrand.h"

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
    assert(val != EnumIndex());
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

    if (_depth_mask != prev->_depth_mask)
        glDepthMask(prev->_depth_mask);
    
    if (_depth_func != prev->_depth_func)
        glDepthFunc(prev->_depth_func);
    
    if (_cull_face != prev->_cull_face)
        glCullFace(prev->_cull_face);
    
    if (_enabled != prev->_enabled) {
        for (unsigned int i = 0; i < Enabled_Max; ++i) {
            uint64_t flag = (1LL << i);
            if ((_enabled & flag) != (prev->_enabled & flag)) {
                if (prev->_enabled & flag)
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
    if (((_enabled & flag) != 0) != (value != 0)) {
        if (value) {
            glEnable(cap);
            _enabled |= flag;
        } else {
            glDisable(cap);
            _enabled &= ~flag;
        }
    }
}

void StateContext::disable(GLenum cap) {
    enable(cap, false);
}

bool StateContext::enabled(GLenum cap) {
    int i = enabled_index(cap);
    uint64_t flag = 1LL << i;
    return (_enabled & flag) != 0;
}

void StateContext::depth_mask(GLboolean flag) {
    if (_depth_mask != flag) {
        _depth_mask = flag;
        glDepthMask(flag);
    }
}

GLboolean StateContext::depth_mask() {
    return _depth_mask;
}

void StateContext::depth_func(GLenum func) {
    if (_depth_func != func) {
        _depth_func = func;
        glDepthFunc(func);
    }
}

GLenum StateContext::depth_func() {
    return _depth_func;
}

void StateContext::cull_face(GLenum mode) {
    if (_cull_face != mode) {
        _cull_face = mode;
        glCullFace(mode);
    }
}

GLenum StateContext::cull_face() {
    return _cull_face;
}

void StateContext::load_state() {
    GLint int_value;

    glGetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);

    glGetIntegerv(GL_DEPTH_FUNC, &int_value);
    _depth_func = int_value;

    glGetIntegerv(GL_CULL_FACE_MODE, &int_value);
    _cull_face = int_value;

    _enabled = 0;
    for (unsigned int i = 0; i < Enabled_Max; ++i) {
        if (glIsEnabled(enabled_values[i])) {
            uint64_t flag = 1LL << i;
            _enabled |= flag;
        }
    }
}
