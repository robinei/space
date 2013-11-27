#version 150 core

uniform mat4 m_pvm;

in vec4 in_pos;

void main(void) {
   gl_Position = m_pvm * in_pos;
}
