#version 410 core

layout(location = 0) out vec4 ocolor;

uniform vec4 stroke_color;

void main(void) {
    ocolor = stroke_color;
}