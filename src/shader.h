// =====================================================================
//  shader.h - Doc file GLSL, bien dich va lien ket thanh chuong trinh shader
// =====================================================================
#pragma once
#include <GL/glew.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "math3d.h"

inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) std::printf("Khong mo duoc file: %s\n", path.c_str());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline GLuint compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::printf("Loi bien dich shader:\n%s\n", log);
    }
    return s;
}

inline GLuint loadProgram(const std::string& vsFile, const std::string& fsFile) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compileShader(GL_VERTEX_SHADER, readFile(vsFile)));
    glAttachShader(p, compileShader(GL_FRAGMENT_SHADER, readFile(fsFile)));
    glLinkProgram(p);
    return p;
}

// Cac ham dat gia tri uniform cho gon
inline void setInt(GLuint p, const char* n, int v) { glUniform1i(glGetUniformLocation(p, n), v); }
inline void setFloat(GLuint p, const char* n, float v) { glUniform1f(glGetUniformLocation(p, n), v); }
inline void setVec3(GLuint p, const char* n, vec3 v) { glUniform3f(glGetUniformLocation(p, n), v.x, v.y, v.z); }
inline void setMat4(GLuint p, const char* n, const mat4& m) {
    glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, m.m);
}
