#version 410 core

uniform sampler2D offscreen;
uniform vec2 resolution;

in vec2 texcoords;

out vec4 out_color;

void main(void)
{
    out_color = texture(offscreen,texcoords).rrrr;
}
