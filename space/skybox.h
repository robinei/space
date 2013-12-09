#ifndef SKYBOX_H
#define SKYBOX_H

#include "opengl.h"
#include "program.h"
#include "texture.h"
#include "mesh.h"

class SkyBox {
public:
    SkyBox(const char *paths[6]) {
        program = Program::create();
        program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/skybox.vert"));
        program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/skybox.frag"));
        program->attrib("in_pos", 0);
        program->link();
        program->detach_all();

        texture = Texture::create_cubemap(paths);

        mesh = Mesh::load("../data/meshes/sphere.ply", false);
    }

    void render(mat4 view_matrix, mat4 projection_matrix) {
        // remove translation element from view matrix
        // (so we render the mesh centered around the camera)
        view_matrix[3].x = 0;
        view_matrix[3].y = 0;
        view_matrix[3].z = 0;

        StateContext context;
        context.disable(GL_CULL_FACE);
        context.depth_func(GL_ALWAYS);
        context.depth_mask(GL_FALSE);

        program->bind();
        program->uniform("m_pvm", projection_matrix * view_matrix);
        program->uniform("tex", 0);
        texture->bind(0, GL_TEXTURE_CUBE_MAP);
        mesh->bind();
        mesh->render_indexed();
        mesh->unbind();
        texture->unbind();
        program->unbind();
    }

private:
    Program::Ref program;
    Texture::Ref texture;
    Mesh::Ref mesh;
};

#endif
