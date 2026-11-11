#version 330 core
// Moi luoi da o san toa do the gioi => chi can nhan ma tran View * Projection
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aS;

uniform mat4 uViewProj;

out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
out float vS;

void main() {
    vPos = aPos;
    vNormal = aNormal;
    vColor = aColor;
    vS = aS;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
