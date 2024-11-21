#version 410

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec2 vPos[];

out float area;

float cross2(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

float signed_area(vec2 a, vec2 b, vec2 c) { return 0.5 * cross2(b-a,c-a); }

void main(void)
{
    area = signed_area(vPos[0], vPos[1], vPos[2]);

    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    EmitVertex();

    gl_Position = gl_in[2].gl_Position;
    EmitVertex();

    EndPrimitive();
}
