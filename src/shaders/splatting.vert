#version 410

#define MASK_EPS 0.001

layout(location = 0) in vec2 vertex;
layout(location = 1) in float pressure;
layout(location = 2) in float visibility;
layout(location = 3) in vec4 color;

layout(location = 0) out float randRot;
layout(location = 1) out float iVisibility;
layout(location = 2) out vec4 iColor;
layout(location = 3) flat out float iOccluded;

uniform mat3 view;
uniform mat4 proj;
uniform mat3 jitter;
uniform int groupId;
uniform int layerId;
uniform float zoom;
uniform float strokeWeight;
uniform float time;
uniform sampler2D maskStrength;
uniform int maskMode;
uniform int displayMode;
uniform float depth;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

void main(void) {
  gl_Position = proj * vec4(vec2(view * jitter * vec3(vertex, 1.0)), 0.5, 1.0);
  randRot = random(vertex.xy) * 6.28318530718 - 3.14159265359;
  iVisibility = visibility;
  iColor = color;
  float mask = texture(maskStrength, (gl_Position.xy+vec2(1.0))*0.5).r;
  bool vis = (maskMode > 0) || (displayMode == 2) || (visibility >= -1.0 && (visibility >= 0.0 ? time >= visibility : -time > visibility));
  gl_PointSize = pressure * strokeWeight * zoom * float(vis);
  iOccluded = 0.0;
  if (maskMode == 1 && texture(maskStrength, (gl_Position.xy+vec2(1.0))*0.5).g > depth && (1.0 - mask) < MASK_EPS) {
    iOccluded = 1.0;
    gl_PointSize *= 0.1;
  }
  // if (layerId == 1 && mask == 0.5) { // table
  //   gl_PointSize *= (sin(gl_VertexID / 6.0) * 0.10 + 0.5);
  // }
}

