#version 410 core

in vec4 realCoord;

uniform sampler2D offscreen;
uniform vec2 resolution;
out vec4 out_color;

void main(void)
{
    //out_color = texture(offscreen,realCoord.xy/resolution.xy).rrrr;
    //out_color = vec4(1.0,0,0,1);
    //out_color = texture(offscreen,vec2(realCoord.x+1,realCoord.y+1)/2).rrrr;
    out_color.r = max(texture(offscreen,vec2(realCoord.x+1,realCoord.y+1)/2).r, 0.0);
}
