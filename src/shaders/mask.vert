#version 410 core

layout(location = 0) in vec2 vertex;

uniform mat3 view;
uniform mat4 proj;
uniform float maskStrength;

void main(void)
{
    float aaa = maskStrength * 2.0;
    gl_Position = proj * vec4(vec2(view * vec3(vertex, 1.0)), 0.5, 1.0);
}
