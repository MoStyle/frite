#version 410 core

layout(location = 0) in vec2 vertex;
layout(location = 1) in float pressure;
layout(location = 2) in float type;     // 0=intermediate, 1=start/end

uniform mat3 view;
uniform mat4 proj;

out VTX {
    float pressure;
    float type;
    int id;
} out_vtx;

void main(void) {
    gl_Position = proj * vec4(vec2(view * vec3(vertex, 1.0)), 0.5, 1.0);
    out_vtx.pressure = pressure;
    out_vtx.type = type;
    out_vtx.id = gl_VertexID;
}