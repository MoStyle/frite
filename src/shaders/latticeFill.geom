#version 410 core

layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

// todo: flat and noperspective
in uint flags[];

out vec3 edgeDist;

uniform int bitToVis;

void main() {
    gl_Position = gl_in[0].gl_Position;
    edgeDist = vec3(1.0, flags[0] & (1u << bitToVis), 0.0);
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    edgeDist = vec3(1.0, flags[1] & (1u << bitToVis), 0.0);
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    edgeDist = vec3(1.0, flags[2] & (1u << bitToVis), 0.0);
    EmitVertex();
    gl_Position = gl_in[3].gl_Position;
    edgeDist = vec3(1.0, flags[3] & (1u << bitToVis), 0.0);
    EmitVertex();
    EndPrimitive();
}