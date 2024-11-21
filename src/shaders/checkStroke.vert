#version 410 core

in vec2 vertex;
uniform mat4 matrix;
uniform vec2 resolution;
out vec4 realCoord;

void main(void)
{
    //gl_Position = matrix * vec4(0, 0, 0, 1);
    gl_Position = vec4(-1, 1, 0, 1);
    //gl_Position = matrix * vec4(vertex, 0.0, 1.0);
    gl_PointSize = 1;
    //realCoord = matrix * vec4(vertex, 0.0, 1.0);
    //realCoord = vec4(vertex.x,resolution.y-vertex.y, 0.0, 1.0);
    realCoord = matrix * vec4(vertex, 0.0, 1.0);
}
