#ifndef RENDERER_H
#define RENDERER_H

// requires: opengl.h
#include "render/program.h"
#include "render/mesh.h"
#include "util/arena.h"
#include <vector>


struct UniformBinding {
    GLint location;
    UniformBinding *next;

    UniformBinding(GLint location) : location(location), next(nullptr) {}
    
    virtual void set_uniform(Program::Ref program) = 0;
};


template <typename T>
struct UniformBindingImpl : public UniformBinding {
    T value;

    UniformBindingImpl(GLint location, const T &value)
        : UniformBinding(location), value(value) {}
    
    virtual void set_uniform(Program::Ref program) {
        program->uniform(location, value);
    }
};


class RenderCommand {
public:
    Program::Ref program;
    Mesh::Ref mesh;

    bool indexed;
    GLint offset;
    GLsizei count;

    template <typename T>
    void add_uniform(const char *name, const T &value) {
        GLint location = program->uniform_location(name);
        add_uniform(location, value);
    }

    template <typename T>
    void add_uniform(GLint location, const T &value) {
        UniformBinding *binding = renderqueue->arena.alloc<UniformBindingImpl<T>>(location, value);
        binding->next = uniforms;
        uniforms = binding;
    }

private:
    friend class Arena;
    friend class RenderQueue;

    RenderCommand() :
        indexed(true),
        offset(0),
        count(0),
        renderqueue(nullptr),
        uniforms(nullptr) {}
    ~RenderCommand() {}
    RenderCommand(const RenderCommand &);
    RenderCommand &operator=(const RenderCommand &);

    RenderQueue *renderqueue;
    UniformBinding *uniforms;
};


class RenderQueue {
public:
    ~RenderQueue() {
        clear();
    }

    RenderCommand *add_command(Program::Ref program, Mesh::Ref mesh);

    void sort();
    void perform();
    void clear();

    void flush() {
        sort();
        perform();
        clear();
    }

private:
    friend class RenderCommand;

    Arena arena;
    std::vector<RenderCommand *> commands;
};


#endif
