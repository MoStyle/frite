#version 410 core

uniform mat4 matrix;
uniform vec2 resolution;

in vec3 vertex;

out vec4 realCoord;

void main(void)
{
    gl_Position = matrix * vec4(floor(vertex.z/resolution.y)-0.5*resolution.x, mod(vertex.z,resolution.y)-0.5*resolution.y+1, 0.0, 1.0);
    //gl_Position = matrix * vec4(vertex, 0.0, 1.0);
    gl_PointSize = 1;
    //realCoord = vec4(vertex.x,resolution.y-vertex.y, 0.0, 1.0);
    realCoord = matrix * vec4(vertex.x, vertex.y, 0.0, 1.0);
}
