#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aSize;   // kich thuoc hat (met)

uniform mat4 uViewProj;
uniform float uScale;  // = chieu cao man hinh * proj[1][1] / 2

out vec4 vColor;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    gl_PointSize = aSize * uScale / gl_Position.w;  // hat o xa nho hon (phoi canh)
    vColor = aColor;
}
