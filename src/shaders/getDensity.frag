#version 410 core

uniform sampler2D gaussian;
uniform float pointSize;
uniform float weight;
in float thick;
out vec4 out_color;

void main(void)
{
    out_color = weight*thick*texture(gaussian,gl_PointCoord).rrrr;
//    float dist = 0.5 - length(gl_PointCoord - vec2(0.5));
//    out_color = vec4(dist);
//    out_color = vec4(1,0,0,1);
}
