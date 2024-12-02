#version 410

flat in vec2 pos;

out vec4 oColor;

uniform float cursorDiameter;
uniform float zoom;
uniform vec2 nudge;

void main(void) {
    float nudgeSpeed = 1.0 + smoothstep(0.01, 30.0, length(nudge)) * 0.5;
    float nudgeAngle = atan(nudge.y, nudge.x);
    mat2 rotAndScale = mat2(cos(nudgeAngle) * nudgeSpeed, -sin(nudgeAngle) * nudgeSpeed,
                            sin(nudgeAngle) / nudgeSpeed, cos(nudgeAngle) / nudgeSpeed);
    float halfCursorWidth = max(2.0, 2.0);
    float d = length(inverse(rotAndScale) * (pos -  gl_FragCoord.xy)) - ((cursorDiameter * 0.5) - halfCursorWidth);
    oColor = vec4(0.0, 0.0, 0.0, mix(0.0, 1.0, 1.0 - smoothstep(halfCursorWidth - 2, halfCursorWidth, abs(d)))); // 2px antialiasing
}
