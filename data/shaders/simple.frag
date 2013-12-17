#version 140

uniform vec4 mat_ambient;
uniform vec4 mat_diffuse;
uniform vec4 mat_specular;
uniform float mat_shininess;
uniform vec3 light_dir;

in vec3 normal;
in vec4 eye;

out vec4 out_color;

void main(void) {
   vec4 spec = vec4(0.0);
   vec3 n = normalize(normal);
   vec3 e = normalize(vec3(eye));
   float intensity = max(dot(n, light_dir), 0.0);
   if (intensity > 0.0) {
       vec3 h = normalize(light_dir + e);
       float intSpec = max(dot(h, n), 0.0);
       spec = mat_specular * pow(intSpec, mat_shininess);
   }
   out_color = max(intensity * mat_diffuse + spec, mat_ambient);
}
