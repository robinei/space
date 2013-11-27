#ifndef RENDERER_H
#define RENDERER_H

// requires: opengl.h
#include "program.h"
#include "mesh.h"
#include "list.h"
#include <vector>


class UniformBinding {
public:
    GLint location;
    ListLink link;

    UniformBinding(GLint location) : location(location) {}
    
    virtual void set(Program::Ref program) = 0;
    virtual bool equals(UniformBinding *other) = 0;
};


template <typename T>
class UniformBindingImpl : public UniformBinding {
public:
    UniformBindingImpl(GLint location, const T &value)
        : UniformBinding(location), value(value) {}
    
    void set(Program::Ref program) {
        program->uniform(location, value);
    }

    bool equals(UniformBinding *other) {
        if (location == other->location) {
            UniformBindingImpl *impl = dynamic_cast<UniformBindingImpl *>(other);
            return impl && impl->value == value;
        }
        return false;
    }

private:
    T value;
};


class RenderCommand : public RefCounted {
public:
    typedef boost::intrusive_ptr<RenderCommand> Ref;
    typedef List<UniformBinding, &UniformBinding::link> UniformList;

    Program::Ref program;
    Mesh::Ref mesh;
    UniformList uniforms;

    ~RenderCommand() {
        while (!uniforms.empty())
            delete uniforms.front();
    }

    static Ref create() {
        return new RenderCommand();
    }

    template <typename T>
    void set_uniform(const char *name, const T &value) {
        GLint location = program->uniform_location(name);
        UniformBinding *binding = new UniformBindingImpl<T>(location, value);
        for (auto b : uniforms) {
            if (b->equals(binding)) {
                delete b;
                break;
            }
        }
        uniforms.push_back(binding);
    }

private:
    RenderCommand() {}
    RenderCommand(const RenderCommand &);
    RenderCommand &operator=(const RenderCommand &);
};


class Renderer {
public:
    ~Renderer();

    void add_command(RenderCommand::Ref cmd);
    void sort_commands();
    void perform_commands();
    void clear_commands();

private:
    std::vector<RenderCommand *> commands;
};


#endif
