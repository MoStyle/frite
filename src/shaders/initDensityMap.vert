#version 410 core

in vec2 vertex;

uniform float pointSize;
uniform mat4 matrix;

void main(void)
{
    gl_Position = matrix * vec4(vertex, 0.0, 1.0);
    gl_PointSize = pointSize;
}
