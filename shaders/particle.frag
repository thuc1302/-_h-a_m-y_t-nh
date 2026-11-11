#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    // Cat diem vuong thanh hinh tron mo dan ra vien
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;
    FragColor = vec4(vColor.rgb, vColor.a * (1.0 - r2));
}
