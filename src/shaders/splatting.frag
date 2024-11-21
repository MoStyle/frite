#version 410

#define MASK_EPS 0.001

uniform int groupId;
uniform int layerId;
uniform int stride;
uniform bool sticker;
uniform bool ignoreMask;
uniform sampler2D tex;
uniform sampler2D texMask;
uniform vec4 strokeColor;
uniform sampler2D maskStrength;
uniform vec2 winSize;
uniform int displayMode;
uniform int maskMode;
uniform float depth;

layout(location = 0) in float randRot;
layout(location = 1) in float iVisibility;
layout(location = 2) in vec4 iColor;
layout(location = 3) flat in float iOccluded;

out vec4 out_color;

float airbrush(vec2 pointCoord) {
  vec2 uv = pointCoord * 2.0 - 1.0;
  return (1.0 - smoothstep(0.0, 1.0, length(uv))) * 0.7;
}

void main(void) {
	mat2 m = mat2(cos(randRot), -sin(randRot), sin(randRot), cos(randRot));
  vec2 ruv = (m * (gl_PointCoord - vec2(0.5))) + vec2(0.5);
  float maskValue = 1.0 - texture(maskStrength, gl_FragCoord.xy / winSize.xy).r;
  float depthValue = texture(maskStrength, gl_FragCoord.xy / winSize.xy).g;
  bool notMasked  = sticker ? (maskValue < 1.0 - MASK_EPS) : (maskValue >= MASK_EPS);
  notMasked = notMasked || (depth > (depthValue - 0.001)); // compare depth
  if (displayMode == 0 && (ignoreMask || maskMode != 0 || notMasked)) {            // normal mode (stroke color)
    vec4 col = texture(tex, vec2(clamp(ruv.x, 0.0, 1.0), clamp(ruv.y, 0.0, 1.0)));
    col.rgb = strokeColor.rgb;
    col.a *= strokeColor.a;
    out_color = vec4(col.rgb * col.a, col.a);
    if (iOccluded > 0.5) out_color = vec4(0.35, 0.35, 0.35, strokeColor.a * 0.85);
  } else if (displayMode == 1 && (ignoreMask || notMasked)) {                     // point color
    vec4 col = texture(tex, vec2(clamp(ruv.x, 0.0, 1.0), clamp(ruv.y, 0.0, 1.0)));
    col.rgb = iColor.rgb;
    col.a *= iColor.a;
    out_color = vec4(col.rgb * col.a, col.a);
  } else if (displayMode == 2) {                                                  // visibility threshold
    vec4 col = texture(tex, vec2(clamp(ruv.x, 0.0, 1.0), clamp(ruv.y, 0.0, 1.0)));
    vec3 a = sign(iVisibility) >= 0.0 ? strokeColor.rgb : vec3(0.75, 0.0, 0.0);
    vec3 b = sign(iVisibility) >= 0.0 ? vec3(0.0, 0.6, 0.0) : strokeColor.rgb;
    col.rgb = mix(a, b, abs(floor(iVisibility * stride) / stride));
    col.a *= strokeColor.a;
    out_color = vec4(col.rgb * col.a, col.a);
  } else {
    out_color = vec4(0.0);
  }
}
