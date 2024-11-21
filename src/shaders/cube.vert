#version 410 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 color;

uniform mat3 view;
uniform mat4 proj;

out vec4 vcolor;

void main(void) {
    gl_Position = proj * vec4(vec2(view * vec3(vertex.xy, 1.0)), vertex.z, 1.0);
    vcolor = vec4(color, 1.0);
}