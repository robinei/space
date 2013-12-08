#version 150 core

uniform sampler2D tex;

in vec2 uv;

out vec4 out_color;

void main(void) {
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
    out_color = texture(tex, uv);
}
