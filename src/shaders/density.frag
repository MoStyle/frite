#version 410

uniform float in_mass;

in float area;

out vec4 fragColor;

void main(void)
{
  fragColor = vec4(vec3(in_mass/area), 1.0);
//  fragColor = vec4(0.0,0.0,0.0, 1.0);
}
