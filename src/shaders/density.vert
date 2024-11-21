#version 410

uniform mat4 matrix;
uniform qreal alpha;

in vec2 vertex1;
in vec2 vertex2;

out vec2 vPos;

void main(void)
{
    vPos = (1.0-alpha) * vertex1 + alpha * vertex2;
    gl_Position = matrix * vec4(vPos.x, 1.0-vPos.y, 0.0, 1.0 );
}
