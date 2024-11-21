#version 410 core

#define MASK_EPS 0.001

layout(location = 0) out vec4 oColor;

in float iVisibility;
in vec4 iColor;

uniform vec2 winSize;
uniform vec4 strokeColor;
uniform sampler2D maskStrength;
uniform int groupId;
uniform int maskMode;
uniform int stride;
uniform float time;
uniform int displayMode;

void main(void) {
    if (displayMode == 0) {         // stroke color
        float maskValue = 1.0 - texture(maskStrength, gl_FragCoord.xy/winSize.xy).r;
        vec4 c = maskMode == 0 ? vec4(0.0) : vec4(15/255.0, 113/255.0, 189/255.0, strokeColor.a * 0.85);
        oColor = maskValue < MASK_EPS ? c : strokeColor;
    } else if (displayMode == 1) {  // point color
        float maskValue = 1.0 - texture(maskStrength, gl_FragCoord.xy/winSize.xy).r;
        vec4 c = maskMode == 0 ? vec4(0.0) : vec4(15/255.0, 113/255.0, 189/255.0, strokeColor.a * 0.85);
        oColor = maskValue < MASK_EPS ? c : iColor;
    } else if (displayMode == 2) {  // visibility threshold
        vec2 p =  gl_FragCoord.xy;
        vec4 a = sign(iVisibility) >= 0.0 ? strokeColor : vec4(0.75, 0.0, 0.0, strokeColor.a);
        vec4 b = sign(iVisibility) >= 0.0 ? vec4(0.0, 0.6, 0.0, strokeColor.a) : strokeColor;
        oColor = iVisibility >= -1.0 ? mix(a, b, abs(floor(iVisibility * stride) / stride)) : vec4(0, 0, 0, step(mod(p.x - p.y, 10.0) / (10.0 - 1.0), 0.2));        
    }
}