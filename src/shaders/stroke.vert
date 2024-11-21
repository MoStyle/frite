#version 410 core

layout(location = 0) in vec2 vertex;
layout(location = 1) in float pressure;
layout(location = 2) in float visibility;
layout(location = 3) in vec4 color;

uniform mat3 view;
uniform mat4 proj;
uniform mat3 jitter;

out VTX {
    float pressure;
    float visibility;
    int id;
    vec4 color;
} out_vtx;

void main(void) {
    gl_Position = proj * vec4(vec2(view * jitter * vec3(vertex, 1.0)), 0.5, 1.0);
    out_vtx.pressure = pressure;
    out_vtx.visibility = visibility;
    out_vtx.id = gl_VertexID;
    out_vtx.color = color;
}