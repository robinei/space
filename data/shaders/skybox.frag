#version 150 core

uniform samplerCube tex;

in vec3 texcoord;

out vec4 out_color;

void main(void) {
    out_color = texture(tex, texcoord);
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
}
