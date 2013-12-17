#version 140

uniform mat4 m_pvm;
uniform mat4 m_vm;
uniform mat3 m_normal;

in vec4 in_pos;
in vec3 in_normal;

out vec3 normal;
out vec4 eye;

void main(void) {
   normal = normalize(m_normal * in_normal);
   eye = -(m_vm * in_pos);
   gl_Position = m_pvm * in_pos;
}
