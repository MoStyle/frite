#version 410 core

layout(location = 0) out vec4 oColor;

in vec3 edgeDist;

uniform float edgeSize;
uniform bool visBitmask;

void main(void)
{
    // float d = min(edgeDist.z, edgeDist.y);
    // oColor = vec4(latticeColor.rgb, step(d, edgeSize) * latticeColor.a);
    oColor = vec4(edgeDist, visBitmask ? 0.5 : 0.0);
}
