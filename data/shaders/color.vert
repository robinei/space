#version 150 core

uniform mat4 m_pvm;

in vec4 in_pos;
in vec4 in_color;

out vec4 color;

void main(void) {
    color = in_color;
    gl_Position = m_pvm * in_pos;
}
