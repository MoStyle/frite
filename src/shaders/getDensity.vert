#version 410 core

in vec3 vertex;

uniform float pointSize;
uniform mat4 matrix;

out float thick;

void main(void)
{
    gl_Position = matrix * vec4(vertex.x, vertex.y, 0.0, 1.0);
    gl_PointSize = pointSize;
    thick=vertex.z;
}
