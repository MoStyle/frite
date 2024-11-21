#version 410

layout(location = 0) in vec2 cursorPos;

flat out vec2 pos; 

uniform mat3 view;
uniform mat4 proj;
uniform float cursorDiameter;
uniform vec2 winSize;
uniform vec2 nudge;

void main(void) {
  gl_Position = proj * vec4(vec2(view * vec3(cursorPos, 1.0)), 0.5, 1.0);
  pos = (gl_Position.xy + vec2(1.0)) * winSize * 0.5;
  float nudgeSpeed = 1.0 + smoothstep(0.01, 30.0, length(nudge)) * 0.5;
  gl_PointSize = cursorDiameter * nudgeSpeed;
}
