// =====================================================================
//  MO PHONG CAU RONG DA NANG - C++ / OpenGL 3.3 / GLFW / GLEW
//
//  Phim:  chuot trai keo = xoay | con lan = phong to/thu nho
//         N = doi canh: ngay -> hoang hon -> dem | F = phun lua | G = phun nuoc
//         1 = toan canh tren cao | 2 = dau rong tu mat duong | 3 = tu mat song
//         4 = flycam doc cau | Esc = thoat
// =====================================================================
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "math3d.h"
#include "mesh.h"
#include "shader.h"

// ------------------------------ Kich thuoc cau (met) ------------------------------
const float DECK_Y = 8.0f;      // do cao mat duong so voi mat nuoc (y = 0)
const float HALF_LEN = 333.0f;  // cau dai 666 m theo truc X
const float HALF_W = 18.75f;    // cau rong 37.5 m theo truc Z
const float MED_W = 3.0f;       // nua be rong dai phan cach giua (than rong dung tren day)
const float WALK_Z = 14.5f;     // mep trong le di bo (6 lan xe nam giua MED_W va WALK_Z)
const float LAMP_SP = 36.0f;    // khoang cach cot den (phai khop voi scene.frag)
const float ROAD_END = 1500.0f; // duong noi tiep tren bo

// ------------------------------ Bang mau ------------------------------
const vec3 GOLD(1.0f, 0.76f, 0.06f);       // son vang cua rong
const vec3 GOLD_DK(0.9f, 0.56f, 0.03f);    // vang dam (chi tiet)
const vec3 WHITE_C(0.93f, 0.93f, 0.9f);    // be tong trang (be do chan vom)
const vec3 BRICK(0.66f, 0.29f, 0.22f);     // gach do le duong / dai phan cach
const vec3 ASPHALT(0.3f, 0.3f, 0.32f);
const vec3 CONCRETE(0.72f, 0.7f, 0.66f);
const vec3 RAIL_GREEN(0.12f, 0.5f, 0.3f);  // lan can xanh la
const vec3 STEEL(0.8f, 0.81f, 0.83f);      // inox / cot den

// ------------------------------ Trang thai chuong trinh ------------------------------
int timeMode = 0;  // 0 = ngay, 1 = hoang hon, 2 = dem
bool fireOn = false, waterOn = false;
float camYaw, camPitch, camDist;  // camera quy dao
vec3 camTarget;
bool dragging = false;
double lastX, lastY;

vec3 headPos, mouthPos, mouthDir;  // tinh khi dung rong
std::vector<vec3> dragonPts;       // duong tam than rong (dung lai cho phan chieu tren nuoc)
std::vector<float> dragonR;

// ------------------------------ Bo tham so anh sang cho moi canh ------------------------------
struct Lighting {
    vec3 skyTop, horizon;   // mau dinh troi, chan troi (cung la mau suong mu)
    vec3 sunDir, sunColor;  // huong toi nguon sang chinh va mau cua no
    vec3 ambient;           // anh sang moi truong
    vec3 sunGlow;           // quang sang quanh mat troi tren bau troi
    vec3 water;             // mau nuoc sau
    float led;              // muc bat den LED / den duong (0..1)
};
const Lighting PRESETS[3] = {
    // Ngay: troi xanh trong, nang trang
    {{0.2f, 0.45f, 0.88f}, {0.72f, 0.82f, 0.93f}, {0.45f, 0.8f, 0.35f}, {1.05f, 1.0f, 0.92f},
     {0.42f, 0.46f, 0.55f}, {0.3f, 0.28f, 0.2f}, {0.06f, 0.24f, 0.28f}, 0.0f},
    // Hoang hon: troi cam tim, mat troi thap phia sau cau
    {{0.3f, 0.28f, 0.55f}, {1.0f, 0.62f, 0.42f}, {-0.6f, 0.12f, 0.8f}, {1.0f, 0.62f, 0.38f},
     {0.7f, 0.58f, 0.58f}, {1.0f, 0.55f, 0.3f}, {0.12f, 0.1f, 0.2f}, 0.0f},
    // Dem: troi toi, anh trang xanh nhat, bat LED
    {{0.01f, 0.015f, 0.05f}, {0.04f, 0.05f, 0.1f}, {-0.3f, 0.6f, -0.4f}, {0.12f, 0.14f, 0.22f},
     {0.12f, 0.13f, 0.2f}, {0.0f, 0.0f, 0.0f}, {0.01f, 0.02f, 0.04f}, 1.0f},
};
const char* MODE_NAMES[3] = {"Ngay", "Hoang hon", "Dem"};

// Tien dan bo tham so hien tai ve bo dich => chuyen canh muot
void blend(Lighting& a, const Lighting& b, float k) {
    a.skyTop = mix(a.skyTop, b.skyTop, k);
    a.horizon = mix(a.horizon, b.horizon, k);
    a.sunDir = normalize(mix(a.sunDir, b.sunDir, k));
    a.sunColor = mix(a.sunColor, b.sunColor, k);
    a.ambient = mix(a.ambient, b.ambient, k);
    a.sunGlow = mix(a.sunGlow, b.sunGlow, k);
    a.water = mix(a.water, b.water, k);
    a.led += (b.led - a.led) * k;
}

// Da giac trong mat phang XY (z co dinh) tu danh sach x0,y0,x1,y1,...
std::vector<vec3> XY(std::initializer_list<float> c, float z = 0) {
    std::vector<vec3> p;
    for (auto it = c.begin(); it != c.end(); it += 2) p.push_back({*it, *(it + 1), z});
    return p;
}

// =====================================================================
//  1. CON RONG
//     Than = gian 3 ong thep quet doc spline Catmull-Rom, giang ngang moi 3 m,
//     2 hang vay tam giac tren lung. Dau rong ghep tu cac tam thep phang.
// =====================================================================
vec3 catmullRom(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
            (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

// Dau rong trong he toa do cuc bo: +x = huong nhin, +y = len, +z = ben trai. Don vi ~ met.
void buildHead(MeshBuilder& h, MeshBuilder& e) {
    const float S = 1.0f;  // vi tri doc than (dau = 1) cho hieu ung LED
    const vec3 MOUTH(0.55f, 0.12f, 0.04f);

    // Khoi chinh: so sau, ham tren, ham duoi (ha mieng), vom hong
    h.prism(XY({-3, -2.4f, 1.2f, -2.8f, 1.8f, 3.2f, -2.4f, 2.7f}), 4.6f, GOLD, S);
    h.prism(XY({0, -0.6f, 8.6f, -0.4f, 9.7f, 0.5f, 9.1f, 1.7f, 4, 2.75f, 0.5f, 3.3f}), 4.0f, GOLD, S);
    h.prism(XY({0.5f, -1.5f, 7.8f, -2.9f, 8.3f, -3.5f, 6.6f, -3.9f, 0.5f, -2.9f}), 3.4f, GOLD, S);
    h.prism(XY({0, -2.9f, 2.5f, -2.4f, 2.5f, -0.5f, 0, -0.6f}), 3.6f, MOUTH, S);

    // Cac cuon tron tren tran va song mui (nhu cac ong thep uon cong cua tuong that)
    for (int i = 0; i < 5; i++) {
        float x = 1.0f + 1.45f * i;
        float y = x < 4 ? 3.3f - (x - 0.5f) * 0.157f : 2.75f - (x - 4) * 0.206f;
        h.rod({x, y + 0.15f, -2.1f}, {x, y + 0.15f, 2.1f}, 0.55f, GOLD, S, 14);
    }
    for (float sg : {-1.0f, 1.0f}) {
        h.ellipsoid({8.7f, 1.6f, sg * 0.95f}, {1.3f, 0.75f, 0.8f}, GOLD_DK, S, 10, 14);  // canh mui
        h.ellipsoid({4.6f, 2.4f, sg * 1.7f}, {1.8f, 0.5f, 0.6f}, GOLD_DK, S, 8, 12);     // long may

        // Mat: dia tron co vong lo, nhan cau o giua
        h.rod({2.6f, 1.1f, sg * 2.0f}, {2.6f, 1.1f, sg * 2.75f}, 1.65f, GOLD, S, 20);
        for (int k = 0; k < 8; k++) {
            float a = k * PI / 4;
            vec3 c(2.6f + 1.2f * std::cos(a), 1.1f + 1.2f * std::sin(a), sg * 2.76f);
            h.rod(c, c + vec3(0, 0, sg * 0.06f), 0.2f, GOLD_DK * 0.5f, S, 8);
        }
        e.ellipsoid({2.6f, 1.1f, sg * 2.85f}, {0.75f, 0.75f, 0.4f}, vec3(0.35f, 0.33f, 0.32f), 1, 10, 14);

        // Rang tren va rang duoi
        for (float x = 3.0f; x < 8.2f; x += 1.3f)
            h.prism({{x, -0.45f, sg * 1.7f}, {x + 0.9f, -0.4f, sg * 1.7f}, {x + 0.45f, -1.4f, sg * 1.7f}}, 0.35f, GOLD_DK, S);
        for (float x = 2.5f; x < 7.0f; x += 1.3f) {
            float y = -1.5f - (x - 0.5f) * 0.192f;
            h.prism({{x, y, sg * 1.4f}, {x + 0.9f, y - 0.17f, sg * 1.4f}, {x + 0.45f, y + 0.9f, sg * 1.4f}}, 0.35f, GOLD_DK, S);
        }

        // Bom/ma xoe ra sau: 2 lop la thep tam giac toa tia quanh ma
        for (int layer = 0; layer < 2; layer++) {
            vec3 C = layer == 0 ? vec3(0.2f, 0.6f, sg * 2.45f) : vec3(-0.6f, 0.5f, sg * 3.1f);
            for (int j = 0; j < 8; j++) {
                float a = (95.0f + j * 150.0f / 7 + layer * 10.0f) * PI / 180;
                vec3 dir(std::cos(a), std::sin(a), 0), perp(-std::sin(a), std::cos(a), 0);
                float len = (4.6f + 1.4f * std::fabs(std::sin(j * 1.3f))) * (layer ? 0.8f : 1.0f);
                vec3 tip = C + dir * len + vec3(0, 0, sg * (1.0f + 0.25f * j));
                h.prism({C + perp * 1.0f, C - perp * 1.0f, tip}, 0.3f, (j + layer) % 2 ? GOLD : GOLD_DK, S);
            }
        }

        // Sung, rau
        h.rod({-0.3f, 3.0f, sg * 1.2f}, {-3.8f, 5.2f, sg * 2.0f}, 0.45f, GOLD_DK, S, 10);
        h.cone({-3.8f, 5.2f, sg * 2.0f}, {-5.6f, 6.6f, sg * 2.4f}, 0.45f, GOLD_DK, S);
        h.rod({9.3f, 0.2f, sg * 1.7f}, {7.2f, -1.5f, sg * 4.0f}, 0.18f, GOLD, S, 6);
        h.rod({7.2f, -1.5f, sg * 4.0f}, {3.8f, -2.2f, sg * 6.2f}, 0.15f, GOLD, S, 6);

        // Rau cam duoi ham duoi
        for (int j = 0; j < 2; j++) {
            float x = 2.0f + 2.2f * j, y = -2.9f - (x - 0.5f) * 0.164f;
            h.prism({{x, y, sg * 0.8f}, {x + 1.6f, y - 0.25f, sg * 0.8f}, {x - 1.8f, y - 2.0f, sg * 1.3f}}, 0.4f, GOLD_DK, S);
        }
    }
    // Mao tren dinh dau (giua + 2 ben xoe ra)
    for (int j = 0; j < 5; j++) {
        float x0 = 0.8f - 1.2f * j, y = 3.0f - 0.35f * j;
        h.prism({{x0, y, 0}, {x0 - 1.3f, y - 0.2f, 0}, {x0 - 3.5f, y + 3.0f - 0.3f * j, 0}}, 0.4f, GOLD, S);
        for (float sg : {-1.0f, 1.0f})
            h.prism({{x0, y - 0.3f, sg * 1.4f}, {x0 - 1.3f, y - 0.5f, sg * 1.4f}, {x0 - 3.2f, y + 2.4f, sg * 2.4f}}, 0.3f,
                    GOLD_DK, S);
    }
}

// body: phan son vang (ve voi den LED) | eyes: nhan cau | extra: day treo + be do trang
void buildDragon(MeshBuilder& body, MeshBuilder& eyes, MeshBuilder& extra) {
    const float D = DECK_Y;
    // Diem dieu khien: duoi o phia Tay (-X), dau huong ra bien (+X).
    // Cac diem y = D - 4.5 la chan vom: than rong chui xuong DUOI mat cau va tua len tru.
    std::vector<vec3> c = {{-306, D + 18, 0}, {-300, D + 12, 0}, {-292, D + 6, 0},  {-280, D + 2, 0},
                           {-262, D - 4.5f, 0}, {-238, D + 12, 0}, {-215, D + 10, 0}, {-190, D - 4.5f, 0},
                           {-160, D + 18, 0}, {-130, D + 26, 0}, {-100, D + 18, 0}, {-70, D - 4.5f, 0},
                           {-35, D + 22, 0},  {0, D + 31, 0},    {35, D + 22, 0},   {70, D - 4.5f, 0},
                           {100, D + 18, 0},  {130, D + 26, 0},  {160, D + 18, 0},  {190, D - 4.5f, 0},
                           {215, D + 10, 0},  {238, D + 12, 0},  {262, D - 4.5f, 0}, {278, D + 6, 0},
                           {288, D + 15, 0},  {298, D + 20.5f, 0}, {310, D + 22.5f, 0}};

    // a) Lay mau duong cong: 30 diem cho moi doan
    std::vector<vec3> pts;
    int nc = c.size();
    for (int i = 0; i + 1 < nc; i++)
        for (int k = 0; k < 30; k++)
            pts.push_back(catmullRom(c[i > 0 ? i - 1 : 0], c[i], c[i + 1], c[i + 2 < nc ? i + 2 : nc - 1], k / 30.0f));
    pts.push_back(c.back());
    size_t n = pts.size();

    // b) Tham so do dai cung s in [0,1]; ban kinh bao ngoai R (thon o duoi, to dan len co)
    std::vector<float> s(n, 0), R(n);
    for (size_t i = 1; i < n; i++) s[i] = s[i - 1] + length(pts[i] - pts[i - 1]);
    for (size_t i = 0; i < n; i++) {
        s[i] /= s[n - 1];
        R[i] = 0.3f + 2.8f * std::fmin(s[i] / 0.12f, 1.0f) + 0.4f * std::fmax(0.0f, (s[i] - 0.9f) / 0.1f);
    }

    // c) He truc tai moi diem: T = tiep tuyen, U = phia "lung" (vuong goc T trong mat phang dung), Z = ngang
    const vec3 Z(0, 0, 1);
    std::vector<vec3> T(n), U(n);
    for (size_t i = 0; i < n; i++) {
        T[i] = normalize(pts[i + 1 < n ? i + 1 : i] - pts[i > 0 ? i - 1 : i]);
        U[i] = normalize(cross(Z, T[i]));
    }
    // 3 ong thep: 2 ong duoi dat canh nhau, 1 ong tren (mat cat tam giac)
    const float OFF[3][2] = {{-0.55f, -0.3f}, {0.55f, -0.3f}, {0.0f, 0.42f}};
    auto tubeAt = [&](size_t i, int k) { return pts[i] + Z * (OFF[k][0] * R[i]) + U[i] * (OFF[k][1] * R[i]); };
    for (int k = 0; k < 3; k++) {
        std::vector<vec3> q(n);
        std::vector<float> rr(n);
        for (size_t i = 0; i < n; i++) {
            q[i] = tubeAt(i, k);
            rr[i] = (k == 2 ? 0.36f : 0.42f) * R[i];
        }
        body.tube(q, rr, s, GOLD, 14);
    }

    // d) Giang ngang, vay lung, day treo, vi tri than rong xuyen qua mat cau
    float dist = 0, nextC = 0, nextS = 0, nextH = -255;
    std::vector<float> crossings;
    for (size_t i = 1; i + 1 < n; i++) {
        dist += length(pts[i] - pts[i - 1]);
        float r = R[i];
        if (dist >= nextC && s[i] < 0.985f) {  // giang tam giac moi 3.2 m
            nextC = dist + 3.2f;
            vec3 a = tubeAt(i, 0), b = tubeAt(i, 1), t = tubeAt(i, 2);
            body.rod(a, b, 0.1f * r, GOLD_DK, s[i], 6);
            body.rod(a, t, 0.1f * r, GOLD_DK, s[i], 6);
            body.rod(b, t, 0.1f * r, GOLD_DK, s[i], 6);
        }
        if (dist >= nextS && s[i] > 0.03f && s[i] < 0.965f) {  // 2 hang vay tam giac nghieng ve phia duoi
            nextS = dist + 3.0f;
            float hgt = 0.5f + 0.6f * r, w = 1.0f + 0.45f * r;
            for (float sg : {-1.0f, 1.0f}) {
                vec3 base = pts[i] + U[i] * (0.67f * r) + Z * (sg * 0.25f * r);
                vec3 a = base - T[i] * (w * 0.5f), b = base + T[i] * (w * 0.5f);
                vec3 tip = a + U[i] * hgt - T[i] * (0.55f * hgt) + Z * (sg * 0.3f * hgt);
                body.prism({a, b, tip}, 0.25f, GOLD, s[i]);
            }
        }
        if ((pts[i - 1].y - D) * (pts[i].y - D) <= 0 && s[i] > 0.01f && s[i] < 0.99f)
            if (crossings.empty() || std::fabs(pts[i].x - crossings.back()) > 5) crossings.push_back(pts[i].x);

        // Day treo trang hinh chu V tu ong duoi xuong dai phan cach
        float x = pts[i].x, low = tubeAt(i, 0).y - 0.42f * r;
        if (x >= nextH && std::fabs(x) < 255 && low > D + 3.5f && U[i].y > 0.6f) {
            nextH = x + 2.6f;
            for (float sg : {-1.0f, 1.0f})
                extra.rod({x, low + 0.3f, sg * 0.55f * r}, {x, D + 0.3f, sg * 2.2f}, 0.06f, WHITE_C, 0, 5);
        }
    }
    // Be do be tong trang hinh thap noi than rong chui qua mat cau
    for (float x : crossings) {
        extra.prism(XY({x - 6.5f, D, x + 6.5f, D, x + 3.6f, D + 2.2f, x - 3.6f, D + 2.2f}), 7.2f, WHITE_C);
        extra.prism(XY({x - 3.6f, D + 2.2f, x + 3.6f, D + 2.2f, x + 2.4f, D + 3.2f, x - 2.4f, D + 3.2f}), 6.0f, WHITE_C);
    }
    // Vay duoi xoe hinh quat
    for (int j = 0; j < 5; j++) {
        float a = -1.0f + 0.5f * j;
        vec3 dir = normalize(-T[0] * std::cos(a) + U[0] * std::sin(a));
        body.prism({pts[0] + U[0] * 0.6f, pts[0] - U[0] * 0.6f, pts[0] + dir * (4.5f - std::fabs(a) * 1.2f)}, 0.2f,
                   j % 2 ? GOLD : GOLD_DK, 0);
    }

    // e) Gan dau rong vao cuoi than; truc X = huong cuoi duong cong (lam phang bot)
    MeshBuilder h, e;
    buildHead(h, e);
    const float k = 1.3f;
    vec3 X = normalize(pts.back() - pts[n - 6]);
    X = normalize(vec3(X.x, X.y * 0.15f, X.z));
    vec3 Y = normalize(vec3(0, 1, 0) - X * X.y);
    vec3 Zh = cross(X, Y);
    vec3 o = pts.back();
    body.place(h, o, X * k, Y * k, Zh * k);
    eyes.place(e, o, X * k, Y * k, Zh * k);

    headPos = o + (X * 3.0f + Y * 1.0f) * k;
    mouthPos = o + (X * 9.4f - Y * 1.6f) * k;
    mouthDir = normalize(X - Y * 0.15f);
    dragonPts = pts;
    dragonR = R;
}

// =====================================================================
//  2. CAU VA MOI TRUONG
// =====================================================================
// Mat cat ngang duong: nhua 6 lan, dai phan cach gach do, le gach do, vach son
void roadSection(MeshBuilder& w, float x0, float x1, bool onBridge) {
    const float D = DECK_Y;
    const vec3 paint(0.92f);
    w.box({x0, D - 0.4f, -WALK_Z}, {x1, D, WALK_Z}, ASPHALT);
    w.box({x0, D, -(MED_W - 0.25f)}, {x1, D + 0.22f, MED_W - 0.25f}, BRICK);
    float lw = (WALK_Z - MED_W) / 3;
    for (float sg : {-1.0f, 1.0f}) {
        w.box({x0, D, sg * (MED_W - 0.25f)}, {x1, D + 0.32f, sg * MED_W}, WHITE_C);                       // bo via giua
        w.box({x0, D - 0.4f, sg * WALK_Z}, {x1, D + 0.28f, sg * (HALF_W - (onBridge ? 0.6f : 0))}, BRICK); // le di bo
        w.box({x0, D, sg * WALK_Z}, {x1, D + 0.34f, sg * (WALK_Z + 0.25f)}, WHITE_C);                     // bo via le
        for (float z : {MED_W + 0.35f, WALK_Z - 0.35f})  // vach lien sat mep
            w.box({x0, D, sg * z - 0.08f}, {x1, D + 0.05f, sg * z + 0.08f}, paint);
        for (int L = 1; L <= 2; L++)  // vach dut phan lan
            for (float x = x0; x < x1 - 6; x += 12)
                w.box({x, D, sg * (MED_W + lw * L) - 0.08f}, {x + 6, D + 0.05f, sg * (MED_W + lw * L) + 0.08f}, paint);
    }
}

void tree(MeshBuilder& m, vec3 p) {
    float hg = randf(5, 8);
    m.rod(p, p + vec3(0, hg * 0.55f, 0), 0.25f, vec3(0.4f, 0.3f, 0.2f), 0, 5);
    m.ellipsoid(p + vec3(0, hg * 0.7f, 0), vec3(randf(2.5f, 3.5f), hg * 0.35f, randf(2.5f, 3.5f)),
                vec3(0.16f, 0.38f, 0.14f) * randf(0.8f, 1.2f), 0, 6, 8);
}

// Thuyen: than hinh thang (lang tru), man thuyen, mui che. Dat theo goc ang, ti le k.
void boat(MeshBuilder& m, vec3 p, float ang, float k, bool tour) {
    MeshBuilder b;
    vec3 hull = tour ? vec3(0.95f, 0.95f, 0.92f) : vec3(0.12f, 0.3f, 0.62f);
    vec3 trim = tour ? vec3(0.1f, 0.25f, 0.6f) : vec3(0.85f, 0.2f, 0.15f);
    b.prism(XY({-4.5f, 1.0f, -3.6f, -0.4f, 3.6f, -0.4f, 5.0f, 1.3f}), 2.6f, hull);
    b.box({-4.3f, 0.95f, -1.35f}, {4.8f, 1.15f, 1.35f}, trim);
    if (tour) {
        b.box({-3, 1.15f, -1.1f}, {3, 2.6f, 1.1f}, WHITE_C);
        b.box({-3.3f, 2.6f, -1.25f}, {3.3f, 2.8f, 1.25f}, trim);
    } else {
        b.box({-2.5f, 1.15f, -0.9f}, {0.5f, 2.0f, 0.9f}, vec3(0.2f, 0.45f, 0.75f));
    }
    vec3 X(std::cos(ang), 0, std::sin(ang)), Z(-std::sin(ang), 0, std::cos(ang));
    m.place(b, p, X * k, vec3(0, k, 0), Z * k);
}

void buildWorld(MeshBuilder& w, MeshBuilder& lamps) {
    const float D = DECK_Y;

    // ---- Mat duong tren cau va duong noi tiep hai ben bo ----
    roadSection(w, -HALF_LEN, HALF_LEN, true);
    roadSection(w, HALF_LEN, ROAD_END, false);
    roadSection(w, -ROAD_END, -HALF_LEN, false);

    // ---- Dam cau, mep cau, lan can ----
    w.box({-HALF_LEN, D - 2.6f, -HALF_W + 1.5f}, {HALF_LEN, D - 0.4f, HALF_W - 1.5f}, CONCRETE * 0.8f);
    for (float sg : {-1.0f, 1.0f}) {
        w.box({-HALF_LEN, D - 1.4f, sg * (HALF_W - 0.6f)}, {HALF_LEN, D + 0.5f, sg * HALF_W}, CONCRETE);
        w.box({-HALF_LEN, D - 0.9f, sg * (HALF_W - 1.5f)}, {HALF_LEN, D - 0.4f, sg * (HALF_W - 0.6f)}, CONCRETE * 0.9f);
        float zr = sg * (HALF_W - 0.3f);
        for (float x = -HALF_LEN; x <= HALF_LEN; x += 2.5f)
            w.box({x - 0.05f, D + 0.5f, zr - 0.05f}, {x + 0.05f, D + 1.35f, zr + 0.05f}, RAIL_GREEN);
        for (float y : {D + 0.9f, D + 1.32f}) w.box({-HALF_LEN, y, zr - 0.07f}, {HALF_LEN, y + 0.1f, zr + 0.07f}, STEEL);
        // Mo cau (abutment) tren bo
        w.box({sg * HALF_LEN, -3, -HALF_W}, {sg * 345, D - 0.4f, HALF_W}, CONCRETE * 0.85f);
    }

    // ---- Den duong canh cong vuon ra mat duong (2 ben, moi LAMP_SP m) + bang ron tren cot ----
    for (float x = -900; x <= 900; x += LAMP_SP)
        for (float sg : {-1.0f, 1.0f}) {
            float zp = sg * (HALF_W - 0.9f);
            w.rod({x, D + 0.28f, zp}, {x, D + 11, zp}, 0.14f, STEEL, 0, 8);
            vec3 p0(x, D + 11, zp), p1(x, D + 11.6f, zp - sg * 1.0f), p2(x, D + 11.55f, zp - sg * 2.2f),
                p3(x, D + 11.05f, zp - sg * 3.0f);
            w.rod(p0, p1, 0.09f, STEEL, 0, 6);
            w.rod(p1, p2, 0.09f, STEEL, 0, 6);
            w.rod(p2, p3, 0.09f, STEEL, 0, 6);
            lamps.box({x - 0.55f, D + 10.75f, p3.z - 0.3f}, {x + 0.55f, D + 11.05f, p3.z + 0.3f}, vec3(1));
            w.box({x - 0.04f, D + 5.0f, zp - sg * 0.15f}, {x + 0.04f, D + 7.4f, zp - sg * 1.05f}, WHITE_C);
        }

    // ---- Co duoi nheo nhieu mau doc lan can ----
    const vec3 FLAGS[6] = {{0.9f, 0.1f, 0.1f}, {0.95f, 0.2f, 0.55f}, {1.0f, 0.85f, 0.1f},
                           {0.15f, 0.35f, 0.9f}, {0.1f, 0.7f, 0.35f}, {0.1f, 0.75f, 0.85f}};
    int f = 0;
    for (float x = -HALF_LEN + 6; x < HALF_LEN; x += 12)
        for (float sg : {-1.0f, 1.0f}) {
            float zf = sg * (HALF_W - 0.3f);
            w.rod({x, D + 1.4f, zf}, {x, D + 4.6f, zf}, 0.035f, STEEL, 0, 5);
            w.prism({{x, D + 4.5f, zf}, {x, D + 3.3f, zf}, {x + 1.5f, D + 3.9f, zf}}, 0.04f, FLAGS[f++ % 6]);
        }

    // ---- Tru cau: than tron dang "san van dong" + xa mu do dam ----
    for (float px : {-302.f, -262.f, -190.f, -130.f, -70.f, 0.f, 70.f, 130.f, 190.f, 262.f, 302.f}) {
        for (float sz : {-1.0f, 1.0f}) w.cylinder({px, -3, sz * 4.5f}, 3.6f, D - 2.6f + 3, CONCRETE);
        w.box({px - 3.6f, -3, -4.5f}, {px + 3.6f, D - 2.6f, 4.5f}, CONCRETE);
        w.box({px - 3.0f, D - 3.8f, -HALF_W + 1.0f}, {px + 3.0f, D - 2.6f, HALF_W - 1.0f}, CONCRETE * 0.92f);
    }

    // ---- Bo song: ke da, via he ven song, duong ven song, mat dat do thi, hang cay ----
    for (float sg : {-1.0f, 1.0f}) {
        w.box({sg * HALF_LEN, -3, -2500}, {sg * 341, D - 0.6f, 2500}, vec3(0.58f, 0.56f, 0.52f));
        w.box({sg * 341, -3, -2500}, {sg * 356, D - 0.3f, 2500}, vec3(0.74f, 0.7f, 0.62f));
        w.box({sg * 356, -3, -2500}, {sg * 372, D - 0.25f, 2500}, ASPHALT);
        w.box({sg * 372, -3, -2500}, {sg * 3000, D - 0.2f, 2500}, vec3(0.48f, 0.48f, 0.45f));
    }

    // ---- Thuyen ca mau xanh neo gan bo + vai tau du lich giua song ----
    std::srand(11);
    for (int i = 0; i < 9; i++) {
        boat(w, {randf(-326, -300), 0, randf(-200, -50)}, PI / 2 + randf(-0.3f, 0.3f), 1.0f, false);
        boat(w, {randf(300, 326), 0, randf(50, 220)}, PI / 2 + randf(-0.3f, 0.3f), 1.0f, false);
    }
    boat(w, {-120, 0, 140}, 0.2f, 2.2f, true);
    boat(w, {160, 0, -110}, -0.1f, 2.2f, true);
    boat(w, {40, 0, -260}, 0.6f, 2.0f, true);
}

// Nha cao tang hai ben bo (bo Tay = trung tam, nhieu toa thap kinh) + hang cay ven song
void buildCity(MeshBuilder& c) {
    const float D = DECK_Y;
    std::srand(2026);
    const vec3 plain[5] = {{0.9f, 0.89f, 0.86f}, {0.92f, 0.86f, 0.72f}, {0.74f, 0.76f, 0.78f},
                           {0.84f, 0.7f, 0.6f}, {0.95f, 0.95f, 0.93f}};
    const vec3 glass[3] = {{0.32f, 0.45f, 0.6f}, {0.2f, 0.27f, 0.36f}, {0.45f, 0.6f, 0.65f}};
    for (float sg : {-1.0f, 1.0f})
        for (float gx = 400; gx < 1500; gx += 40)
            for (float gz = -1500; gz < 1500; gz += 40) {
                if (std::fabs(gz) < 45 || randf(0, 1) < 0.1f) continue;
                float nearRiver = 1 - (gx - 400) / 1100;
                float x = sg * (gx + randf(-5, 5)), z = gz + randf(-5, 5);
                float wd = randf(14, 30), dp = randf(14, 30), hg = randf(7, 18) + (randf(0, 1) < 0.25f ? randf(8, 22) : 0);
                bool tower = randf(0, 1) < (sg < 0 ? 0.09f : 0.03f) * (0.3f + nearRiver * nearRiver * 1.5f);
                bool isGlass = false;
                if (tower) {
                    hg = randf(45, sg < 0 ? 140.0f : 85.0f);
                    wd = randf(22, 34);
                    dp = randf(22, 34);
                    isGlass = randf(0, 1) < 0.55f;
                }
                vec3 col = isGlass ? glass[std::rand() % 3] : plain[std::rand() % 5];
                float seed = randf(0.05f, 0.45f) + (isGlass ? 0.5f : 0.0f);  // vS > 0: shader ve cua so
                c.box({x - wd / 2, D - 0.2f, z - dp / 2}, {x + wd / 2, D - 0.2f + hg, z + dp / 2}, col, seed);
                if (tower && randf(0, 1) < 0.5f)
                    c.box({x - wd * 0.35f, D - 0.2f + hg, z - dp * 0.35f}, {x + wd * 0.35f, D + hg + randf(4, 10), z + dp * 0.35f},
                          col, seed);
            }
    for (float sg : {-1.0f, 1.0f})
        for (float z = -1500; z < 1500; z += 11) {
            if (std::fabs(z) < 26) continue;
            for (float xx : {348.0f, 376.0f}) tree(c, {sg * xx + randf(-1, 1), D - 0.3f, z + randf(-2, 2)});
        }
}

// Nui xa: vanh dai luoi do cao bao quanh thanh pho. Do cao = tong cac song sin
// (tan so k, bien do ~1/k, pha ngau nhien) => duong song nui gap ghenh tu nhien.
// Phia Dong (+X) la bien nen thap, chi chua mot khoi nui (ban dao Son Tra).
void buildFar(MeshBuilder& f) {
    std::srand(99);
    const int NA = 720, NR = 8, NK = 28;
    float amp[NK], ph[NK], drift[NK], norm = 0;
    for (int k = 0; k < NK; k++) {
        amp[k] = 1.0f / std::pow(k + 1.0f, 0.7f);
        ph[k] = randf(0, 2 * PI);
        drift[k] = randf(-0.6f, 0.6f);
        norm += amp[k];
    }
    auto height = [&](float th, float ring) {
        float hsum = 0;
        for (int k = 0; k < NK; k++) hsum += amp[k] * std::sin((k + 1) * th + ph[k] + ring * drift[k]);
        hsum = hsum / norm * 2.2f;                                  // ~[-1, 1]
        float bump = std::sin(PI * ring);                           // 0 o mep trong/ngoai
        float east = std::cos(th);
        float sea = 1 - 0.85f * std::fmin(1.0f, std::fmax(0.0f, (east - 0.2f) / 0.5f));
        float sonTra = 0.9f * std::exp(-(th + 0.75f) * (th + 0.75f) / 0.03f);  // khoi nui phia Dong Bac
        return -30 + 520 * std::fmax(0.0f, 0.35f + hsum) * bump * (sea + sonTra);
    };
    std::vector<vec3> P(NA * NR);
    for (int j = 0; j < NR; j++)
        for (int i = 0; i < NA; i++) {
            float th = 2 * PI * i / NA - PI, ring = j / (NR - 1.0f);
            float r = 3300 + 2000 * ring;
            P[j * NA + i] = vec3(std::cos(th) * r, height(th, ring), std::sin(th) * r);
        }
    unsigned base = f.v.size();
    for (int j = 0; j < NR; j++)
        for (int i = 0; i < NA; i++) {
            vec3 du = P[j * NA + (i + 1) % NA] - P[j * NA + (i + NA - 1) % NA];
            vec3 dv = P[std::min(j + 1, NR - 1) * NA + i] - P[std::max(j - 1, 0) * NA + i];
            vec3 n = normalize(cross(dv, du));
            if (n.y < 0) n = -n;
            float hgt = std::fmax(0.0f, P[j * NA + i].y) / 700;
            vec3 col = mix(vec3(0.17f, 0.27f, 0.19f), vec3(0.3f, 0.36f, 0.33f), std::fmin(1.0f, hgt));
            f.v.push_back({P[j * NA + i], n, col, 0});
        }
    for (int j = 0; j + 1 < NR; j++)
        for (int i = 0; i < NA; i++) {
            unsigned a = base + j * NA + i, b = base + j * NA + (i + 1) % NA;
            f.idx.insert(f.idx.end(), {a, a + NA, b, b, a + NA, b + NA});
        }
}

// =====================================================================
//  3. GIAO THONG: o to, xe buyt, xe may chay tren 6 lan (luoi dung lai moi khung hinh)
// =====================================================================
struct Vehicle {
    float x0, speed, z, len;
    int type;  // 0 = o to, 1 = xe buyt, 2 = xe may
    vec3 col, col2;
};
std::vector<Vehicle> traffic;
const float TRAFFIC_SPAN = 1400;

void initTraffic() {
    std::srand(77);
    const vec3 carCols[6] = {{0.95f, 0.95f, 0.95f}, {0.15f, 0.15f, 0.17f}, {0.7f, 0.72f, 0.75f},
                             {0.75f, 0.1f, 0.1f},   {0.98f, 0.8f, 0.1f},   {0.2f, 0.3f, 0.6f}};
    const vec3 shirts[5] = {{0.9f, 0.9f, 0.9f}, {0.2f, 0.4f, 0.8f}, {0.85f, 0.3f, 0.3f}, {0.3f, 0.3f, 0.3f}, {0.95f, 0.6f, 0.2f}};
    float lw = (WALK_Z - MED_W) / 3;
    for (float sg : {-1.0f, 1.0f})  // ben z > 0 chay theo +x (giao thong ben phai)
        for (int k = 0; k < 3; k++) {
            float zc = sg * (MED_W + lw * (k + 0.5f));
            float speed = k == 2 ? randf(8, 10) : randf(11, 15);  // moi lan 1 van toc => khong dam nhau
            float x = randf(0, 20), prevLen = 0;
            while (true) {
                float r = randf(0, 1);
                int type = k == 2 ? (r < 0.9f ? 2 : 0) : k == 1 ? (r < 0.35f ? 2 : r < 0.92f ? 0 : 1) : (r < 0.9f ? 0 : 1);
                float len = type == 0 ? 4.4f : type == 1 ? 11.0f : 1.8f;
                x += prevLen * 0.5f + len * 0.5f + (type == 2 ? randf(2, 12) : randf(8, 40));
                if (x > TRAFFIC_SPAN - 20) break;
                float z = zc + (type == 2 ? randf(-1.2f, 1.2f) : 0);
                traffic.push_back({x, speed * sg, z, len, type, carCols[std::rand() % 6], shirts[std::rand() % 5]});
                prevLen = len;
            }
        }
}

void buildTraffic(MeshBuilder& b, MeshBuilder& l, float t) {
    const float D = DECK_Y;
    const vec3 HEAD(1, 0.95f, 0.8f), TAIL(1, 0.08f, 0.05f), GLASS(0.12f, 0.14f, 0.17f), TYRE(0.06f);
    b.clear();
    l.clear();
    for (const Vehicle& q : traffic) {
        float along = std::fmod(q.x0 + std::fabs(q.speed) * t, TRAFFIC_SPAN);
        float dir = q.speed > 0 ? 1.0f : -1.0f;
        float x = dir > 0 ? -TRAFFIC_SPAN / 2 + along : TRAFFIC_SPAN / 2 - along;
        float h = q.len * 0.5f, z = q.z, fx = x + dir * h, bx = x - dir * h;
        if (q.type == 0) {
            b.box({x - h + 0.3f, D, z - 0.85f}, {x + h - 0.3f, D + 0.3f, z + 0.85f}, TYRE);
            b.box({x - h, D + 0.25f, z - 0.9f}, {x + h, D + 1.0f, z + 0.9f}, q.col);
            float cx = x - dir * 0.3f;
            b.box({cx - h * 0.5f, D + 1.0f, z - 0.8f}, {cx + h * 0.5f, D + 1.5f, z + 0.8f}, GLASS);
            b.box({cx - h * 0.45f, D + 1.5f, z - 0.78f}, {cx + h * 0.45f, D + 1.56f, z + 0.78f}, q.col);
            for (float sd : {-0.6f, 0.6f}) {
                l.box({fx - dir * 0.05f, D + 0.6f, z + sd - 0.18f}, {fx + dir * 0.03f, D + 0.8f, z + sd + 0.18f}, HEAD);
                l.box({bx + dir * 0.05f, D + 0.6f, z + sd - 0.15f}, {bx - dir * 0.03f, D + 0.78f, z + sd + 0.15f}, TAIL);
            }
        } else if (q.type == 1) {
            b.box({x - h + 0.5f, D, z - 1.2f}, {x + h - 0.5f, D + 0.4f, z + 1.2f}, TYRE);
            b.box({x - h, D + 0.35f, z - 1.25f}, {x + h, D + 3.2f, z + 1.25f}, q.col * 0.4f + vec3(0.55f));
            b.box({x - h + 0.3f, D + 1.6f, z - 1.28f}, {x + h - 0.3f, D + 2.7f, z + 1.28f}, GLASS);
            for (float sd : {-0.9f, 0.9f}) {
                l.box({fx - dir * 0.05f, D + 0.7f, z + sd - 0.2f}, {fx + dir * 0.03f, D + 0.95f, z + sd + 0.2f}, HEAD);
                l.box({bx + dir * 0.05f, D + 0.7f, z + sd - 0.2f}, {bx - dir * 0.03f, D + 0.95f, z + sd + 0.2f}, TAIL);
            }
        } else {
            b.box({x - 0.9f, D + 0.05f, z - 0.18f}, {x + 0.9f, D + 0.85f, z + 0.18f}, q.col);
            float rc = x - dir * 0.15f;
            b.box({rc - 0.22f, D + 0.85f, z - 0.22f}, {rc + 0.22f, D + 1.6f, z + 0.22f}, q.col2);       // nguoi lai
            b.box({rc - 0.16f, D + 1.6f, z - 0.16f}, {rc + 0.16f, D + 1.9f, z + 0.16f}, q.col * 0.8f);  // mu bao hiem
            l.box({fx - dir * 0.02f, D + 0.75f, z - 0.1f}, {fx + dir * 0.05f, D + 0.9f, z + 0.1f}, HEAD);
            l.box({bx + dir * 0.02f, D + 0.6f, z - 0.08f}, {bx - dir * 0.05f, D + 0.72f, z + 0.08f}, TAIL);
        }
    }
}

// =====================================================================
//  4. HE THONG HAT: phun lua va phun nuoc
// =====================================================================
struct Particle {
    vec3 p, v;
    float age, life;
    bool fire;
};
struct PointVertex {
    vec3 pos;
    float r, g, b, a, size;
};
std::vector<Particle> particles;

// Huong ngau nhien trong hinh non quanh dir, goc mo spread
vec3 randomInCone(vec3 dir, float spread) {
    vec3 a = normalize(cross(dir, vec3(0, 1, 0)));
    vec3 b = cross(dir, a);
    return normalize(dir + a * randf(-spread, spread) + b * randf(-spread, spread));
}

void updateParticles(float dt) {
    int nFire = fireOn ? (int)(900 * dt) + 1 : 0;
    int nWater = waterOn ? (int)(1200 * dt) + 1 : 0;
    for (int i = 0; i < nFire; i++)
        particles.push_back({mouthPos, randomInCone(mouthDir, 0.12f) * randf(22, 30), 0, randf(0.9f, 1.5f), true});
    for (int i = 0; i < nWater; i++)
        particles.push_back({mouthPos, randomInCone(normalize(mouthDir + vec3(0, 0.12f, 0)), 0.07f) * randf(30, 36), 0,
                             randf(3, 4), false});

    // Cap nhat theo vat ly (tich phan Euler): lua boc len, nuoc roi xuong
    for (Particle& q : particles) {
        q.age += dt;
        q.v += (q.fire ? vec3(0, 6, 0) : vec3(0, -9.8f, 0)) * dt;
        q.v = q.v * (q.fire ? 0.98f : 0.995f);
        q.p += q.v * dt;
    }
    for (size_t i = 0; i < particles.size();) {
        if (particles[i].age > particles[i].life || particles[i].p.y < 0) {
            particles[i] = particles.back();
            particles.pop_back();
        } else
            i++;
    }
}

// =====================================================================
//  5. DIEU KHIEN: ban phim va chuot
// =====================================================================
void setView(int v) {
    if (v == 1) { camTarget = vec3(0, 15, 0); camYaw = 0.6f; camPitch = 0.42f; camDist = 560; }       // toan canh tren cao
    if (v == 2) { camTarget = headPos; camYaw = 0.55f; camPitch = -0.28f; camDist = 50; }             // dau rong tu mat duong
    if (v == 3) { camTarget = vec3(180, 16, 0); camYaw = -0.95f; camPitch = 0.02f; camDist = 230; }  // tu mat song
    if (v == 4) { camTarget = vec3(150, 14, 0); camYaw = -0.5f; camPitch = 0.42f; camDist = 250; }    // flycam doc cau
}
void onKey(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, 1);
    if (key == GLFW_KEY_N) timeMode = (timeMode + 1) % 3;
    if (key == GLFW_KEY_F) fireOn = !fireOn;
    if (key == GLFW_KEY_G) waterOn = !waterOn;
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) setView(key - GLFW_KEY_0);
}
void onMouseButton(GLFWwindow* win, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) dragging = (action == GLFW_PRESS);
    glfwGetCursorPos(win, &lastX, &lastY);
}
void onCursor(GLFWwindow*, double x, double y) {
    if (dragging) {
        camYaw += (x - lastX) * 0.005f;
        camPitch += (y - lastY) * 0.005f;
        camPitch = std::fmax(-0.5f, std::fmin(1.5f, camPitch));
    }
    lastX = x;
    lastY = y;
}
void onScroll(GLFWwindow*, double, double dy) {
    camDist *= std::pow(0.88f, (float)dy);
    camDist = std::fmax(15.0f, std::fmin(2000.0f, camDist));
}

// Luu anh man hinh ra file PPM (dung khi chay voi --shot)
void saveScreenshot(const char* file, int w, int h) {
    std::vector<unsigned char> px(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = std::fopen(file, "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--) std::fwrite(&px[y * w * 3], 1, w * 3, f);  // lat anh
    std::fclose(f);
}

// Ma tran chieu song song tu mat troi bao tron vung cau => dung cho shadow map
mat4 lightMatrix(vec3 dir, float& depthRange) {
    dir = normalize(dir);
    vec3 c(0, 20, 0);
    mat4 V = lookAt(c + dir * 2000.0f, c, std::fabs(dir.y) > 0.95f ? vec3(1, 0, 0) : vec3(0, 1, 0));
    vec3 mn(1e9f), mx(-1e9f);
    for (float x : {-420.0f, 420.0f})
        for (float y : {-5.0f, 65.0f})
            for (float z : {-160.0f, 160.0f}) {
                vec3 p = transformPoint(V, {x, y, z});
                mn = vec3(std::fmin(mn.x, p.x), std::fmin(mn.y, p.y), std::fmin(mn.z, p.z));
                mx = vec3(std::fmax(mx.x, p.x), std::fmax(mx.y, p.y), std::fmax(mx.z, p.z));
            }
    float n = -mx.z - 10, f = -mn.z + 10;
    depthRange = f - n;
    return ortho(mn.x, mx.x, mn.y, mx.y, n, f) * V;
}

// =====================================================================
//  6. CHUONG TRINH CHINH
// =====================================================================
int main(int argc, char** argv) {
    // Tham so tuy chon: --mode 0|1|2  --view 1..4  --fire  --water  --shot anh.ppm  --time giay
    const char* shotFile = nullptr;
    int view = 4;
    float startTime = 0;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--mode") && i + 1 < argc) timeMode = std::atoi(argv[++i]) % 3;
        if (!std::strcmp(argv[i], "--view") && i + 1 < argc) view = std::atoi(argv[++i]);
        if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotFile = argv[++i];
        if (!std::strcmp(argv[i], "--time") && i + 1 < argc) startTime = std::atof(argv[++i]);
        if (!std::strcmp(argv[i], "--fire")) fireOn = true;
        if (!std::strcmp(argv[i], "--water")) waterOn = true;
    }

    // ---- Tao cua so va context OpenGL 3.3 ----
#if defined(GLFW_PLATFORM_X11) && defined(__linux__)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);  // GLEW can GLX (tren Wayland dung XWayland)
#endif
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // macOS bat buoc de co context 3.3 Core
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Cau Rong Da Nang - OpenGL", nullptr, nullptr);
    if (!win) { std::printf("Khong tao duoc cua so OpenGL 3.3\n"); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    glewInit();
    glfwSetKeyCallback(win, onKey);
    glfwSetMouseButtonCallback(win, onMouseButton);
    glfwSetCursorPosCallback(win, onCursor);
    glfwSetScrollCallback(win, onScroll);

    // ---- Nap shader ----
    GLuint sceneShader = loadProgram("shaders/scene.vert", "shaders/scene.frag");
    GLuint pointShader = loadProgram("shaders/particle.vert", "shaders/particle.frag");
    GLuint shadowShader = loadProgram("shaders/shadow.vert", "shaders/shadow.frag");

    // ---- Dung hinh hoc (chi 1 lan) ----
    MeshBuilder worldB, lampB, dragonB, eyeB, cityB, farB, waterB, skyB, trafficB, trafficLightB;
    buildWorld(worldB, lampB);
    buildDragon(dragonB, eyeB, worldB);
    buildCity(cityB);
    buildFar(farB);
    initTraffic();
    waterB.box({-5000, -1, -5000}, {5000, 0, 5000}, vec3(1));
    skyB.ellipsoid({0, 0, 0}, {7000, 7000, 7000}, vec3(1));
    Mesh world, lamps, dragon, eyes, city, far, water, sky, trafficM, trafficLights;
    world.upload(worldB);
    lamps.upload(lampB);
    dragon.upload(dragonB);
    eyes.upload(eyeB);
    city.upload(cityB);
    far.upload(farB);
    water.upload(waterB);
    sky.upload(skyB);

    // ---- Shadow map: texture do sau 4096x4096 gan vao framebuffer rieng ----
    const int SM = 4096;
    GLuint shadowTex, shadowFbo;
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SM, SM, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---- Profile than rong theo x (do cao tam, ban kinh) cho phan chieu tren mat nuoc ----
    const int PN = 1024;
    std::vector<float> prof(PN * 2);
    for (int i = 0; i < PN; i++) prof[2 * i] = -1000, prof[2 * i + 1] = 0;
    for (size_t i = 0; i + 1 < dragonPts.size(); i++) {
        float ua = (dragonPts[i].x + 340) / 680 * (PN - 1), ub = (dragonPts[i + 1].x + 340) / 680 * (PN - 1);
        for (int k = (int)std::ceil(std::fmin(ua, ub)); k <= (int)std::floor(std::fmax(ua, ub)); k++) {
            if (k < 0 || k >= PN) continue;
            float t = ub != ua ? (k - ua) / (ub - ua) : 0;
            float y = dragonPts[i].y + (dragonPts[i + 1].y - dragonPts[i].y) * t;
            if (y > prof[2 * k]) {
                prof[2 * k] = y;
                prof[2 * k + 1] = dragonR[i] + (dragonR[i + 1] - dragonR[i]) * t;
            }
        }
    }
    GLuint profTex;
    glGenTextures(1, &profTex);
    glBindTexture(GL_TEXTURE_2D, profTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, PN, 1, 0, GL_RG, GL_FLOAT, prof.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(sceneShader);
    setInt(sceneShader, "uShadowMap", 1);
    setInt(sceneShader, "uDragonProfile", 2);
    setFloat(sceneShader, "uDeckY", DECK_Y);

    // VAO cho hat (du lieu cap nhat moi khung hinh)
    GLuint pVao, pVbo;
    glGenVertexArrays(1, &pVao);
    glGenBuffers(1, &pVbo);
    glBindVertexArray(pVao);
    glBindBuffer(GL_ARRAY_BUFFER, pVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)28);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    std::printf("Phim: chuot trai keo = xoay | con lan = zoom | N = ngay/hoang hon/dem\n"
                "      F = phun lua | G = phun nuoc | 1 = toan canh | 2 = dau rong | 3 = mat song | 4 = flycam\n"
                "      Esc = thoat\n");
    setView(view);

    Lighting L = PRESETS[timeMode];
    double last = glfwGetTime(), fpsTimer = 0, simTime = startTime;
    int frames = 0, fpsCount = 0;

    // ======================== VONG LAP CHINH ========================
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = shotFile ? 1.0f / 60 : (float)std::fmin(0.1, now - last);
        last = now;
        simTime += dt;
        glfwPollEvents();

        // ---- Cap nhat ----
        blend(L, PRESETS[timeMode], std::fmin(1.0f, dt * 2));
        updateParticles(dt);
        buildTraffic(trafficB, trafficLightB, (float)simTime);
        trafficM.upload(trafficB, GL_DYNAMIC_DRAW);
        trafficLights.upload(trafficLightB, GL_DYNAMIC_DRAW);
        float flicker = 0.8f + 0.2f * std::sin(now * 37) * std::sin(now * 23);
        vec3 fireColor = fireOn ? vec3(1.0f, 0.55f, 0.18f) * (3.5f * flicker) : vec3(0);

        // ---- Pass 1: shadow map (ve tu goc nhin mat troi) ----
        float depthRange;
        mat4 lightVP = lightMatrix(L.sunDir, depthRange);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        glViewport(0, 0, SM, SM);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shadowShader);
        setMat4(shadowShader, "uLightVP", lightVP);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        world.draw();
        dragon.draw();
        eyes.draw();
        trafficM.draw();
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Camera quy dao: mat nam tren mat cau quanh diem camTarget (khong cho xuong duoi nuoc)
        int fbw, fbh;
        glfwGetFramebufferSize(win, &fbw, &fbh);
        vec3 eye = camTarget + vec3(std::cos(camPitch) * std::cos(camYaw), std::sin(camPitch),
                                    std::cos(camPitch) * std::sin(camYaw)) * camDist;
        eye.y = std::fmax(eye.y, 1.5f);
        mat4 proj = perspective(55 * PI / 180, (float)fbw / fbh, 0.8f, 12000);
        mat4 viewProj = proj * lookAt(eye, camTarget, vec3(0, 1, 0));

        // ---- Pass 2: ve canh ----
        glViewport(0, 0, fbw, fbh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, profTex);
        glActiveTexture(GL_TEXTURE0);

        glUseProgram(sceneShader);
        setMat4(sceneShader, "uViewProj", viewProj);
        setMat4(sceneShader, "uLightVP", lightVP);
        setFloat(sceneShader, "uShadowBias", 0.25f / depthRange);
        setVec3(sceneShader, "uCamPos", eye);
        setVec3(sceneShader, "uLightDir", L.sunDir);
        setVec3(sceneShader, "uLightColor", L.sunColor);
        setVec3(sceneShader, "uAmbient", L.ambient);
        setVec3(sceneShader, "uSkyTop", L.skyTop);
        setVec3(sceneShader, "uFogColor", L.horizon);
        setVec3(sceneShader, "uSunGlow", L.sunGlow);
        setVec3(sceneShader, "uWaterColor", L.water);
        setVec3(sceneShader, "uFirePos", mouthPos + mouthDir * 12.0f);
        setVec3(sceneShader, "uFireColor", fireColor);
        setFloat(sceneShader, "uTime", (float)simTime);
        setFloat(sceneShader, "uLedOn", L.led);
        setFloat(sceneShader, "uEmitVtx", 0);

        // uMode: 0 = vat the thuong, 1 = rong (den LED), 2 = mat nuoc, 3 = bau troi, 4 = nha (cua so)
        setInt(sceneShader, "uMode", 3);
        glDepthMask(GL_FALSE);
        sky.draw();
        glDepthMask(GL_TRUE);
        setInt(sceneShader, "uMode", 0);
        setVec3(sceneShader, "uEmissive", vec3(0));
        world.draw();
        far.draw();
        trafficM.draw();
        setInt(sceneShader, "uMode", 4);
        city.draw();
        setInt(sceneShader, "uMode", 0);
        setFloat(sceneShader, "uEmitVtx", 0.1f + 2.0f * L.led);
        trafficLights.draw();
        setFloat(sceneShader, "uEmitVtx", 0);
        setVec3(sceneShader, "uEmissive", vec3(1.0f, 0.85f, 0.6f) * L.led);
        lamps.draw();
        setVec3(sceneShader, "uEmissive", vec3(1.0f, 0.1f, 0.05f) * L.led);
        eyes.draw();
        setVec3(sceneShader, "uEmissive", vec3(0));
        setInt(sceneShader, "uMode", 1);
        dragon.draw();
        setInt(sceneShader, "uMode", 2);
        water.draw();

        // ---- Ve hat: lua = cong mau (sang ruc), nuoc = tron alpha ----
        std::vector<PointVertex> firePts, waterPts;
        for (const Particle& q : particles) {
            float t = q.age / q.life;
            if (q.fire) {
                vec3 c = t < 0.3f ? mix(vec3(1, 0.9f, 0.6f), vec3(1, 0.5f, 0.1f), t / 0.3f)
                                  : mix(vec3(1, 0.5f, 0.1f), vec3(0.4f, 0.05f, 0.02f), (t - 0.3f) / 0.7f);
                firePts.push_back({q.p, c.x, c.y, c.z, 0.6f * (1 - t), 1.5f + 6.5f * t});
            } else {
                waterPts.push_back({q.p, 0.8f, 0.88f, 1.0f, 0.45f * (1 - t), 1.0f + 3.0f * t});
            }
        }
        glUseProgram(pointShader);
        setMat4(pointShader, "uViewProj", viewProj);
        setFloat(pointShader, "uScale", fbh * proj.m[5] * 0.5f);
        glBindVertexArray(pVao);
        glBindBuffer(GL_ARRAY_BUFFER, pVbo);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBufferData(GL_ARRAY_BUFFER, waterPts.size() * sizeof(PointVertex), waterPts.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, waterPts.size());
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBufferData(GL_ARRAY_BUFFER, firePts.size() * sizeof(PointVertex), firePts.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, firePts.size());
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // Chup anh tu dong roi thoat
        if (shotFile && ++frames == 90) {
            saveScreenshot(shotFile, fbw, fbh);
            break;
        }
        glfwSwapBuffers(win);

        fpsCount++;
        if (now - fpsTimer > 0.5) {
            char title[128];
            std::snprintf(title, sizeof(title), "Cau Rong Da Nang | %s | FPS: %.0f", MODE_NAMES[timeMode],
                          fpsCount / (now - fpsTimer));
            glfwSetWindowTitle(win, title);
            fpsTimer = now;
            fpsCount = 0;
        }
    }
    glfwTerminate();
    return 0;
}
