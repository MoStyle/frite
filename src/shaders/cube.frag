#version 410 core

in vec4 vcolor;

layout(location = 0) out vec4 ocolor;

void main(void) {
    ocolor = vcolor;
    // ocolor = vec4(vec3(gl_FragCoord.z), 1.0);
}