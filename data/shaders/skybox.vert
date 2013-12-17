#version 140

uniform mat4 m_pvm;

in vec3 in_pos;

out vec3 texcoord;

void main(void) {
    gl_Position = m_pvm * vec4(in_pos, 1.0);
    texcoord = in_pos;
}
