#version 410 core

layout(location = 0) out vec4 oMaskStrength;

uniform float maskStrength;
uniform float depth;

void main(void) {
    oMaskStrength = vec4(maskStrength, depth, 0.0, 1.0);
}
