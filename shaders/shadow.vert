#version 330 core
// Pass 1 cua shadow mapping: ve canh tu goc nhin cua mat troi, chi ghi do sau
layout(location = 0) in vec3 aPos;
uniform mat4 uLightVP;
void main() { gl_Position = uLightVP * vec4(aPos, 1.0); }
