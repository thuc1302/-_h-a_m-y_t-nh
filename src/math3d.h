// =====================================================================
//  math3d.h - Vector 3 chieu va ma tran 4x4 (tu viet, khong dung thu vien)
//  Ma tran luu theo COT (column-major) giong quy uoc cua OpenGL.
// =====================================================================
#pragma once
#include <cmath>
#include <cstdlib>

const float PI = 3.14159265f;

struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    explicit vec3(float s) : x(s), y(s), z(s) {}  // vec3(0.5) = (0.5, 0.5, 0.5)
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    vec3 operator+(const vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(const vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    vec3 operator*(const vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(vec3 a, vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(vec3 v) { return std::sqrt(dot(v, v)); }
inline vec3 normalize(vec3 v) { float l = length(v); return l > 1e-6f ? v * (1.0f / l) : v; }
inline vec3 mix(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
inline float randf(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }

struct mat4 {
    float m[16];  // phan tu (hang r, cot c) = m[c*4 + r]
    mat4() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
    float& at(int r, int c) { return m[c * 4 + r]; }
};

inline mat4 operator*(mat4 a, mat4 b) {
    mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            r.at(i, j) = 0;
            for (int k = 0; k < 4; k++) r.at(i, j) += a.at(i, k) * b.at(k, j);
        }
    return r;
}

// Ma tran chieu phoi canh
inline mat4 perspective(float fovy, float aspect, float n, float f) {
    float k = 1.0f / std::tan(fovy / 2);
    mat4 r;
    r.at(0, 0) = k / aspect;
    r.at(1, 1) = k;
    r.at(2, 2) = (f + n) / (n - f);
    r.at(2, 3) = 2 * f * n / (n - f);
    r.at(3, 2) = -1;
    r.at(3, 3) = 0;
    return r;
}

// Ma tran nhin: camera dat tai eye, nhin ve center
inline mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    mat4 r;
    r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;  r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
}
