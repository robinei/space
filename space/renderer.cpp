#include "opengl.h"
#include "renderer.h"
#include <algorithm>

struct CommandCompare {
    bool operator()(RenderCommand *a, RenderCommand *b) const {
        if (a->program.get() < b->program.get()) return true;
        if (a->program.get() > b->program.get()) return false;
        if (a->mesh.get() < b->mesh.get()) return true;
        return false;
    }
};

Renderer::~Renderer() {
    clear_commands();
}

void Renderer::add_command(RenderCommand::Ref cmd) {
    intrusive_ptr_add_ref(cmd.get());
    commands.push_back(cmd.get());
}

void Renderer::sort_commands() {
    std::sort(commands.begin(), commands.end(), CommandCompare());
}

void Renderer::perform_commands() {
    Program *curr_program = nullptr;
    Mesh *curr_mesh = nullptr;

    for (auto cmd : commands) {
        if (cmd->program.get() != curr_program) {
            if (curr_program) curr_program->unbind();
            curr_program = cmd->program.get();
            curr_program->bind();
        }
        if (cmd->mesh.get() != curr_mesh) {
            if (curr_mesh) curr_mesh->unbind();
            curr_mesh = cmd->mesh.get();
            curr_mesh->bind();
        }
        for (auto binding : cmd->uniforms) {
            binding->set(curr_program);
        }
        curr_mesh->render();
    }

    if (curr_mesh) curr_mesh->unbind();
    if (curr_program) curr_program->unbind();
}

void Renderer::clear_commands() {
    for (auto cmd : commands)
        intrusive_ptr_release(cmd);
    commands.clear();
}

