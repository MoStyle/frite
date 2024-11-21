#version 410

uniform sampler2D offscreen;
uniform sampler2D kernel;

in vec2 v_texCoord;

out vec4 fragColor;

float erode(vec2 uv, vec2 size, int radius) {
    vec2 invSize = 1.0 / size;
    float invKR = 1.0 / float(radius);
    float value = texture(offscreen, uv).r;
    for(int i = -radius; i <= radius; ++i) {
        for(int j = -radius; j <= radius; ++j) {
            vec2 rxy = vec2(ivec2(i, j));
            vec2 kuv = rxy * invKR;
            vec2 texOffset = uv + rxy * invSize;
            float k = texture(kernel, vec2(0.5) + kuv).r;
            float tex = texture(offscreen, texOffset).r;
            if(k > 0 && tex < value) {
                value = tex;
            }
    	  }
    }
    return value;
}

void main(void)
{
  vec2 size = vec2(textureSize(offscreen, 0));
  int radius = textureSize(kernel, 0).x/2;
  fragColor.r = erode(v_texCoord, size, radius);
}
