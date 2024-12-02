#version 410

uniform vec2 direction;
uniform sampler2D offscreen;

in vec2 v_texCoord;

out vec4 fragColor;

vec3 GaussianBlur(sampler2D tex0, vec2 centreUV, vec2 pixelOffset );

void main(void)
{
    fragColor.r = GaussianBlur( offscreen, v_texCoord, direction).r;
}
