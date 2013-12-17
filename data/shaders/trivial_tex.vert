#version 140

uniform mat4 m_pvm;

in vec4 in_pos;
in vec2 in_uv;

out vec2 uv;

void main(void) {
    uv = in_uv;
    gl_Position = m_pvm * in_pos;
}
