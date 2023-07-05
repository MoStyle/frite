#version 410 core

in vec2 vertex;

out vec2 texcoords;

void main(void)
{
    gl_Position = vec4(vertex, 0.0, 1.0);
    texcoords = (vertex+vec2(1,1))/2.0;
}
