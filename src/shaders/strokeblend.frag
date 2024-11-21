#version 410

uniform sampler2D offscreen;

in vec2 v_texCoord;

out vec4 fragColor;

void main(void)
{
    vec4 c = texture(offscreen, v_texCoord);
    fragColor = vec4(c.rgb, 1.0);
}
