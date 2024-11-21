#version 410 core

layout(location = 0) out vec4 oColor;

uniform vec4 groupColor;

void main(void)
{
    // vec2 p =  gl_FragCoord.xy;
    // float interval = 20.0;
    // float a = step(mod(p.x - p.y, interval) / (interval - 1.0), 0.2);
    // oColor = vec4(groupColor.rgb, 0.5 + 0.1 * a);
    oColor = vec4(groupColor.rgb, 1.0);
}
