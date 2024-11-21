#version 410

uniform mat4 matrix;
in vec2 vertex;

out vec2 v_texCoord;

void main(void)
{
    gl_Position = matrix * vec4( vertex, 0.0, 1.0 );
    v_texCoord = vertex * 0.5 + vec2(0.5);
}
