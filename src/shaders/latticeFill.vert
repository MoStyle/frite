#version 410 core

in vec2 vertex;
in uint iFlags;

out uint flags;

uniform mat3 view;
uniform mat4 proj;

void main(void)
{
    gl_Position = proj * vec4(vec2(view * vec3(vertex, 1.0)), 0.5, 1.0);
    flags = iFlags;
}
