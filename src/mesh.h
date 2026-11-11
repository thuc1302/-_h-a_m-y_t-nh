// =====================================================================
//  mesh.h - Sinh hinh hoc (hop, elipxoit, hinh non, ong) va dua len GPU
// =====================================================================
#pragma once
#include <GL/glew.h>

#include <cstddef>
#include <vector>

#include "math3d.h"

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 color;
    float s;  // vi tri doc than rong (0 = duoi, 1 = dau) - dung de to mau den LED
};

struct MeshBuilder {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;

    // Hinh hop tu goc min den goc max
    void box(vec3 mn, vec3 mx, vec3 col) {
        vec3 c = (mn + mx) * 0.5f, h = (mx - mn) * 0.5f;
        vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (vec3 n : normals) {
            vec3 t = std::fabs(n.y) > 0.5f ? vec3(1, 0, 0) : vec3(0, 1, 0);
            vec3 b = cross(n, t);
            unsigned s = v.size();
            float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
            for (int k = 0; k < 4; k++) v.push_back({c + (n + t * sx[k] + b * sy[k]) * h, n, col, 0});
            idx.insert(idx.end(), {s, s + 1, s + 2, s, s + 2, s + 3});
        }
    }

    // Elipxoit tam c, ban kinh r (theo 3 truc)
    void ellipsoid(vec3 c, vec3 r, vec3 col, float s = 1) {
        const int ST = 16, SL = 24;
        unsigned base = v.size();
        for (int i = 0; i <= ST; i++) {
            float phi = PI * i / ST;
            for (int j = 0; j <= SL; j++) {
                float th = 2 * PI * j / SL;
                vec3 p(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
                vec3 n = normalize(vec3(p.x / r.x, p.y / r.y, p.z / r.z));
                v.push_back({c + p * r, n, col, s});
            }
        }
        for (int i = 0; i < ST; i++)
            for (int j = 0; j < SL; j++) {
                unsigned a = base + i * (SL + 1) + j, b = a + SL + 1;
                idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
    }

    // Hinh non tu day (ban kinh r) den dinh tip
    void cone(vec3 base, vec3 tip, float r, vec3 col, float s = 1) {
        const int SL = 10;
        vec3 ax = normalize(tip - base);
        vec3 a = normalize(cross(ax, std::fabs(ax.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
        vec3 b = cross(ax, a);
        float h = length(tip - base);
        unsigned st = v.size();
        for (int j = 0; j <= SL; j++) {
            float th = 2 * PI * j / SL;
            vec3 d = a * std::cos(th) + b * std::sin(th);
            vec3 n = normalize(d * h + ax * r);
            v.push_back({base + d * r, n, col, s});
            v.push_back({tip, n, col, s});
        }
        for (int j = 0; j < SL; j++) idx.insert(idx.end(), {st + 2 * j, st + 2 * j + 1, st + 2 * j + 2});
    }

    // Hinh tru dung: tam day c, ban kinh r, cao h (co nap tren)
    void cylinder(vec3 c, float r, float h, vec3 col) {
        const int SL = 24;
        unsigned st = v.size();
        for (int j = 0; j <= SL; j++) {
            float th = 2 * PI * j / SL;
            vec3 n(std::cos(th), 0, std::sin(th));
            v.push_back({c + n * r, n, col, 0});
            v.push_back({c + n * r + vec3(0, h, 0), n, col, 0});
        }
        for (int j = 0; j < SL; j++) {
            unsigned a = st + 2 * j;
            idx.insert(idx.end(), {a, a + 1, a + 2, a + 2, a + 1, a + 3});
        }
        unsigned top = v.size();  // nap tren
        v.push_back({c + vec3(0, h, 0), vec3(0, 1, 0), col, 0});
        for (int j = 0; j <= SL; j++) {
            float th = 2 * PI * j / SL;
            v.push_back({c + vec3(std::cos(th) * r, h, std::sin(th) * r), vec3(0, 1, 0), col, 0});
        }
        for (int j = 0; j < SL; j++) idx.insert(idx.end(), {top, top + 1 + j, top + 2 + j});
    }

    // Ong quet doc duong cong pts (ban kinh radii[i]) - dung cho than rong.
    // Khung (T, N, B) duoc "van chuyen song song" de ong khong bi xoan.
    void tube(const std::vector<vec3>& pts, const std::vector<float>& radii, const std::vector<float>& sv,
              vec3 col, int SL) {
        size_t n = pts.size();
        vec3 N(0, 1, 0);
        unsigned base = v.size();
        for (size_t i = 0; i < n; i++) {
            vec3 T = normalize(pts[i + 1 < n ? i + 1 : i] - pts[i > 0 ? i - 1 : i]);
            N = normalize(N - T * dot(N, T));  // chieu N cu len mat phang vuong goc voi T
            vec3 B = cross(T, N);
            for (int j = 0; j <= SL; j++) {
                float th = 2 * PI * j / SL;
                vec3 d = N * std::cos(th) + B * std::sin(th);
                v.push_back({pts[i] + d * radii[i], d, col, sv[i]});
            }
        }
        for (size_t i = 0; i + 1 < n; i++)
            for (int j = 0; j < SL; j++) {
                unsigned a = base + i * (SL + 1) + j, b = a + SL + 1;
                idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
    }

    // Gop luoi src (toa do cuc bo) vao luoi nay, dat theo he truc X, Y, Z tai goc o
    // (do dai cua X, Y, Z = he so phong to)
    void place(const MeshBuilder& src, vec3 o, vec3 X, vec3 Y, vec3 Z) {
        unsigned base = v.size();
        for (Vertex q : src.v) {
            q.pos = o + X * q.pos.x + Y * q.pos.y + Z * q.pos.z;
            q.normal = normalize(X * q.normal.x + Y * q.normal.y + Z * q.normal.z);
            v.push_back(q);
        }
        for (unsigned i : src.idx) idx.push_back(base + i);
    }
};

// Luoi da nap len GPU
struct Mesh {
    GLuint vao = 0;
    GLsizei count = 0;

    void upload(const MeshBuilder& b) {
        GLuint vbo, ebo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, b.v.size() * sizeof(Vertex), b.v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, b.idx.size() * sizeof(unsigned), b.idx.data(), GL_STATIC_DRAW);
        GLsizei st = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, s));
        count = b.idx.size();
    }
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    }
};
