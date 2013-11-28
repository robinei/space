#include "opengl.h"
#include "renderqueue.h"
#include <algorithm>

struct CommandCompare {
    bool operator()(RenderCommand *a, RenderCommand *b) const {
        if (a->program.get() < b->program.get()) return true;
        if (a->program.get() > b->program.get()) return false;
        if (a->mesh.get() < b->mesh.get()) return true;
        return false;
    }
};

RenderCommand *RenderQueue::add_command(Program::Ref program, Mesh::Ref mesh) {
    RenderCommand *cmd = arena.alloc<RenderCommand>();
    cmd->program = program;
    cmd->mesh = mesh;
    cmd->renderqueue = this;
    commands.push_back(cmd);
    return cmd;
}

void RenderQueue::sort() {
    std::sort(commands.begin(), commands.end(), CommandCompare());
}

void RenderQueue::perform() {
    Program *program = nullptr;
    Mesh *mesh = nullptr;

    for (auto cmd : commands) {
        if (cmd->program.get() != program) {
            if (program) program->unbind();
            program = cmd->program.get();
            program->bind();
        }

        for (auto b = cmd->uniforms; b; b = b->next) {
            b->set_uniform(program);
        }

        if (cmd->mesh.get() != mesh) {
            if (mesh) mesh->unbind();
            mesh = cmd->mesh.get();
            mesh->bind();
        }

        mesh->render();
    }

    if (mesh) mesh->unbind();
    if (program) program->unbind();
}

void RenderQueue::clear() {
    for (auto cmd : commands)
        cmd->~RenderCommand();
    commands.clear();
    arena.clear();
}

