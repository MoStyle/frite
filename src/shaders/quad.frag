#version 410

uniform sampler2D offscreen;

in vec2 v_texCoord;

out vec4 fragColor;

void main(void)
{
    fragColor = texture(offscreen, v_texCoord);
}
