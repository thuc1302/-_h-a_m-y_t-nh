// =====================================================================
//  MO PHONG CAU RONG DA NANG - C++ / OpenGL 3.3 / GLFW / GLEW
//
//  Phim:  chuot trai keo = xoay | con lan = phong to/thu nho
//         N = doi canh: ngay -> hoang hon -> dem | F = phun lua | G = phun nuoc
//         1 = toan canh | 2 = can canh dau rong | 3 = goc chup tu mat song | Esc = thoat
// =====================================================================
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "math3d.h"
#include "mesh.h"
#include "shader.h"

// ------------------------------ Kich thuoc cau (met) ------------------------------
const float DECK_Y = 8.0f;     // do cao mat duong so voi mat nuoc (y = 0)
const float HALF_LEN = 333.0f; // cau dai 666 m theo truc X
const float HALF_W = 18.75f;   // cau rong 37.5 m theo truc Z
const float MEDIAN = 3.8f;     // khe giua cau, than rong di qua day

// ------------------------------ Trang thai chuong trinh ------------------------------
int timeMode = 1;  // 0 = ngay, 1 = hoang hon, 2 = dem
bool fireOn = false, waterOn = false;
float camYaw, camPitch, camDist;  // camera quy dao
vec3 camTarget;
bool dragging = false;
double lastX, lastY;

vec3 headPos, mouthPos, mouthDir;  // tinh khi dung rong

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
    // Ngay: troi xanh, nang trang tren cao
    {{0.25f, 0.5f, 0.9f}, {0.68f, 0.8f, 0.95f}, {0.5f, 0.8f, 0.4f}, {1.0f, 0.95f, 0.85f},
     {0.4f, 0.45f, 0.55f}, {0.3f, 0.28f, 0.2f}, {0.05f, 0.2f, 0.28f}, 0.0f},
    // Hoang hon: troi cam tim, mat troi thap phia sau cau
    {{0.3f, 0.28f, 0.55f}, {1.0f, 0.62f, 0.42f}, {-0.6f, 0.12f, 0.8f}, {1.0f, 0.62f, 0.38f},
     {0.75f, 0.62f, 0.6f}, {1.0f, 0.55f, 0.3f}, {0.12f, 0.1f, 0.2f}, 0.0f},
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

// =====================================================================
//  1. CON RONG: spline Catmull-Rom -> ong (tube) + dau rong + day treo
// =====================================================================
vec3 catmullRom(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
            (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

void buildDragon(MeshBuilder& body, MeshBuilder& eyes, MeshBuilder& hangers) {
    const float D = DECK_Y;
    // Diem dieu khien: duoi o phia Tay (-X), dau huong ra bien (+X).
    // Cac diem y = D - 4.5 la chan vom: than rong vong xuong DUOI mat cau va tua len tru.
    std::vector<vec3> c = {{-306, D + 18, 0}, {-300, D + 12, 0}, {-292, D + 6, 0},  {-280, D + 2, 0},
                           {-262, D - 4.5f, 0}, {-238, D + 12, 0}, {-215, D + 10, 0}, {-190, D - 4.5f, 0},
                           {-160, D + 18, 0}, {-130, D + 26, 0}, {-100, D + 18, 0}, {-70, D - 4.5f, 0},
                           {-35, D + 22, 0},  {0, D + 31, 0},    {35, D + 22, 0},   {70, D - 4.5f, 0},
                           {100, D + 18, 0},  {130, D + 26, 0},  {160, D + 18, 0},  {190, D - 4.5f, 0},
                           {215, D + 10, 0},  {238, D + 12, 0},  {262, D - 4.5f, 0}, {280, D + 6, 0},
                           {292, D + 16, 0},  {300, D + 24, 0}};

    // a) Lay mau duong cong: 30 diem cho moi doan
    std::vector<vec3> pts;
    int n = c.size();
    for (int i = 0; i + 1 < n; i++)
        for (int k = 0; k < 30; k++)
            pts.push_back(catmullRom(c[i > 0 ? i - 1 : 0], c[i], c[i + 1], c[i + 2 < n ? i + 2 : n - 1], k / 30.0f));
    pts.push_back(c.back());

    // b) Tham so do dai cung s in [0,1] va ban kinh than (thon o duoi, to o co)
    std::vector<float> s(pts.size(), 0), radius(pts.size());
    for (size_t i = 1; i < pts.size(); i++) s[i] = s[i - 1] + length(pts[i] - pts[i - 1]);
    float total = s.back();
    for (size_t i = 0; i < pts.size(); i++) {
        s[i] /= total;
        radius[i] = 0.3f + 3.0f * std::fmin(s[i] / 0.12f, 1.0f) + 0.8f * std::fmax(0.0f, (s[i] - 0.86f) / 0.14f);
    }

    // c) Than rong = ong quet doc duong cong
    const vec3 gold(1.0f, 0.78f, 0.15f);
    body.tube(pts, radius, s, gold, 24);

    // d) Vay lung (hinh non moi 3 m) va day treo (moi 2.5 m duoi cac vom)
    float next = 0, dist = 0, nextHanger = -245;
    for (size_t i = 1; i + 1 < pts.size(); i++) {
        dist += length(pts[i] - pts[i - 1]);
        if (dist >= next && s[i] > 0.04f && s[i] < 0.95f) {
            next = dist + 3;
            vec3 T = normalize(pts[i + 1] - pts[i - 1]);
            vec3 N = cross(vec3(0, 0, 1), T);  // phia "lung" cua duong cong
            vec3 base = pts[i] + N * (radius[i] * 0.8f);
            body.cone(base, base + normalize(N - T * 0.7f) * (0.3f + 0.35f * radius[i]), 0.15f + 0.12f * radius[i], gold, s[i]);
        }
        float x = pts[i].x, top = pts[i].y - radius[i] * 0.5f;
        if (x >= nextHanger && std::fabs(x) < 245 && top > D + 4) {
            nextHanger = x + 2.5f;
            for (float z : {-(MEDIAN + 0.2f), MEDIAN + 0.2f})
                hangers.box({x - 0.07f, D + 0.8f, z - 0.07f}, {x + 0.07f, top, z + 0.07f}, vec3(0.88f));
        }
    }

    // e) Dau rong: dung trong he toa do cuc bo (+x = huong nhin, +y = len)
    MeshBuilder h, e;
    const vec3 dark(0.8f, 0.55f, 0.12f);
    h.ellipsoid({1.5f, 0.8f, 0}, {4.2f, 2.9f, 2.7f}, gold);    // so
    h.ellipsoid({6.6f, 0.1f, 0}, {3.8f, 1.3f, 1.9f}, gold);    // mom tren
    h.ellipsoid({9.9f, 0.5f, 0}, {1.1f, 0.9f, 1.5f}, dark);    // mui
    h.ellipsoid({6.0f, -2.0f, 0}, {3.4f, 0.65f, 1.5f}, gold);  // ham duoi (ha mieng)
    for (float z : {-1.0f, 1.0f}) {
        h.cone({1.5f, 2.8f, 1.0f * z}, {-5.5f, 6.5f, 2.4f * z}, 0.65f, dark);    // sung
        h.cone({9.6f, 0.0f, 1.3f * z}, {6.0f, -3.5f, 6.0f * z}, 0.2f, gold);     // rau
        h.ellipsoid({3.6f, 2.5f, 1.6f * z}, {1.6f, 0.6f, 0.8f}, dark);           // long may
        for (int k = 0; k < 3; k++)                                               // bom hai ben ma
            h.cone({0.5f - k * 1.2f, 0.6f - k * 0.3f, 2.3f * z}, {-2.5f - k * 1.2f, 1.2f - k * 0.6f, 4.2f * z}, 0.6f, dark);
        e.ellipsoid({4.5f, 1.9f, 2.0f * z}, {0.7f, 0.7f, 0.5f}, vec3(0.2f, 0.02f, 0.02f));  // mat
    }
    for (int k = 0; k < 5; k++)  // bom tren gay
        h.cone({2.0f - k * 1.2f, 2.8f - k * 0.2f, 0}, {-0.5f - k * 1.2f, 5.0f - k * 0.3f, 0}, 0.5f, dark);

    // Dat dau rong (phong to 1.6 lan) vao cuoi than; truc X = huong cuoi duong cong (lam phang bot)
    const float k = 2.0f;
    vec3 X = normalize(pts.back() - pts[pts.size() - 6]);
    X = normalize(vec3(X.x, X.y * 0.3f, X.z));
    vec3 Y = normalize(vec3(0, 1, 0) - X * X.y);
    vec3 Z = cross(X, Y);
    vec3 o = pts.back() - X * 1.0f;
    body.place(h, o, X * k, Y * k, Z * k);
    eyes.place(e, o, X * k, Y * k, Z * k);

    headPos = o + X * (3.0f * k);
    mouthPos = o + (X * 10.4f - Y * 1.2f) * k;
    mouthDir = normalize(X - Y * 0.12f);
}

// =====================================================================
//  2. CAU VA MOI TRUONG: mat cau, le, lan can, tru, den, co, bo song, nha, thuyen
// =====================================================================
void buildWorld(MeshBuilder& w, MeshBuilder& lamps) {
    const float D = DECK_Y;
    const vec3 asphalt(0.25f, 0.25f, 0.27f), concrete(0.65f, 0.64f, 0.6f), steel(0.3f, 0.32f, 0.35f);

    // Mat cau: 2 don nguyen, chua khe o giua cho than rong
    w.box({-HALF_LEN, D - 2.5f, MEDIAN}, {HALF_LEN, D, HALF_W}, asphalt);
    w.box({-HALF_LEN, D - 2.5f, -HALF_W}, {HALF_LEN, D, -MEDIAN}, asphalt);
    // Duong tren bo hai ben
    w.box({HALF_LEN, D - 1, -HALF_W}, {1500, D, HALF_W}, asphalt);
    w.box({-1500, D - 1, -HALF_W}, {-HALF_LEN, D, HALF_W}, asphalt);

    const vec3 flagColors[4] = {{0.85f, 0.1f, 0.1f}, {0.1f, 0.6f, 0.25f}, {0.15f, 0.3f, 0.8f}, {0.95f, 0.8f, 0.1f}};
    int flag = 0;
    for (float sg : {-1.0f, 1.0f}) {
        // Le di bo, go dai phan cach, lan can
        w.box({-1500, D, 13.75f * sg}, {1500, D + 0.3f, HALF_W * sg}, concrete);
        w.box({-HALF_LEN, D, MEDIAN * sg}, {HALF_LEN, D + 0.8f, (MEDIAN + 0.4f) * sg}, concrete);
        w.box({-HALF_LEN, D + 1.3f, (HALF_W - 0.2f) * sg}, {HALF_LEN, D + 1.45f, (HALF_W - 0.05f) * sg}, steel);
        for (float x = -HALF_LEN; x <= HALF_LEN; x += 5)
            w.box({x - 0.06f, D + 0.3f, (HALF_W - 0.2f) * sg}, {x + 0.06f, D + 1.3f, (HALF_W - 0.05f) * sg}, steel);
        // Vach ke lan (net dut trang)
        for (float x = -1500; x < 1500; x += 12)
            for (float z : {7.4f, 10.6f})
                w.box({x, D, z * sg - 0.08f}, {x + 6, D + 0.02f, z * sg + 0.08f}, vec3(0.9f));
        // Den duong moi 32 m: cot thep + bong den (phat sang ban dem) + la co
        for (float x = -320; x <= 320; x += 32) {
            float z = (HALF_W - 0.8f) * sg;
            w.box({x - 0.15f, D + 0.3f, z - 0.15f}, {x + 0.15f, D + 10, z + 0.15f}, steel);
            w.box({x - 0.1f, D + 9.9f, z - 2.4f * sg}, {x + 0.1f, D + 10.1f, z}, steel);
            lamps.box({x - 0.4f, D + 9.6f, z - 2.6f * sg - 0.3f}, {x + 0.4f, D + 9.9f, z - 2.6f * sg + 0.3f}, vec3(1));
            w.box({x + 0.15f, D + 7.0f, z - 0.04f}, {x + 1.5f, D + 8.8f, z + 0.04f}, flagColors[flag++ % 4]);
        }
        // Bo song: ke da + mat dat co
        w.box({HALF_LEN * sg, -2, -2000}, {338 * sg, D - 1, 2000}, vec3(0.55f, 0.53f, 0.5f));
        w.box({338 * sg, -2, -2000}, {2000 * sg, D - 0.05f, 2000}, vec3(0.36f, 0.42f, 0.3f));
    }
    // Tru cau tron. Tai chan vom (than rong vong xuong) co tru lon o giua de do rong.
    for (float px : {-262.f, -190.f, -130.f, -70.f, 0.f, 70.f, 130.f, 190.f, 262.f}) {
        bool archFoot = std::fabs(px) == 70 || std::fabs(px) == 190 || std::fabs(px) == 262;
        if (archFoot) w.cylinder({px, -2, 0}, 7.0f, D - 4.0f + 2, concrete);
        for (float z : {-11.0f, 11.0f}) w.cylinder({px, -2, z}, 3.0f, D - 3.5f + 2, concrete);
        w.box({px - 3.2f, D - 3.5f, -17.5f}, {px + 3.2f, D - 2.5f, -MEDIAN}, concrete);
        w.box({px - 3.2f, D - 3.5f, MEDIAN}, {px + 3.2f, D - 2.5f, 17.5f}, concrete);
    }
    // Nha cao tang ngau nhien hai ben bo
    std::srand(2026);
    const vec3 colors[4] = {{0.85f, 0.84f, 0.8f}, {0.7f, 0.75f, 0.82f}, {0.9f, 0.85f, 0.75f}, {0.6f, 0.64f, 0.7f}};
    for (int i = 0; i < 160; i++) {
        float sg = (i % 2) ? 1.0f : -1.0f;
        float x = randf(400, 1300) * sg, z = randf(-1300, 1300);
        if (std::fabs(z) < 50) continue;  // chua cho duong chinh
        float wd = randf(20, 40), dp = randf(20, 40), hg = randf(15, 50) + (randf(0, 1) < 0.2f ? 60 : 0);
        w.box({x - wd / 2, D, z - dp / 2}, {x + wd / 2, D + hg, z + dp / 2}, colors[i % 4]);
    }
    // Thuyen du lich neo gan bo Dong
    const float boats[6][2] = {{250, -70}, {268, -95}, {241, -118}, {286, -60}, {225, -150}, {300, -130}};
    for (auto& b : boats) {
        float x = b[0], z = b[1];
        w.box({x - 6, -0.3f, z - 2}, {x + 6, 1.5f, z + 2}, vec3(0.15f, 0.35f, 0.7f));   // than thuyen
        w.box({x - 3, 1.5f, z - 1.5f}, {x + 3, 3.3f, z + 1.5f}, vec3(0.9f, 0.9f, 0.85f)); // cabin
    }
}

// =====================================================================
//  3. HE THONG HAT: phun lua va phun nuoc
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
    // Phat hat moi
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
        q.v = q.v * (q.fire ? 0.98f : 0.995f);  // luc can khong khi
        q.p += q.v * dt;
    }
    // Xoa hat het tuoi hoac roi xuong song
    for (size_t i = 0; i < particles.size();) {
        if (particles[i].age > particles[i].life || particles[i].p.y < 0) {
            particles[i] = particles.back();
            particles.pop_back();
        } else
            i++;
    }
}

// =====================================================================
//  4. DIEU KHIEN: ban phim va chuot
// =====================================================================
void setView(int v) {
    if (v == 1) { camTarget = vec3(0, 18, 0); camYaw = 1.15f; camPitch = 0.28f; camDist = 560; }
    if (v == 2) { camTarget = headPos; camYaw = 0.75f; camPitch = 0.1f; camDist = 95; }
    if (v == 3) { camTarget = vec3(180, 16, 0); camYaw = -0.95f; camPitch = 0.0f; camDist = 230; }  // giong anh chup
}
void onKey(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, 1);
    if (key == GLFW_KEY_N) timeMode = (timeMode + 1) % 3;
    if (key == GLFW_KEY_F) fireOn = !fireOn;
    if (key == GLFW_KEY_G) waterOn = !waterOn;
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_3) setView(key - GLFW_KEY_0);
}
void onMouseButton(GLFWwindow* win, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) dragging = (action == GLFW_PRESS);
    glfwGetCursorPos(win, &lastX, &lastY);
}
void onCursor(GLFWwindow*, double x, double y) {
    if (dragging) {
        camYaw += (x - lastX) * 0.005f;
        camPitch += (y - lastY) * 0.005f;
        camPitch = std::fmax(0.0f, std::fmin(1.5f, camPitch));
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

// =====================================================================
//  5. CHUONG TRINH CHINH
// =====================================================================
int main(int argc, char** argv) {
    // Tham so tuy chon: --mode 0|1|2  --view 1|2|3  --fire  --water  --shot anh.ppm
    const char* shotFile = nullptr;
    int view = 3;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--mode") && i + 1 < argc) timeMode = std::atoi(argv[++i]) % 3;
        if (!std::strcmp(argv[i], "--view") && i + 1 < argc) view = std::atoi(argv[++i]);
        if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotFile = argv[++i];
        if (!std::strcmp(argv[i], "--fire")) fireOn = true;
        if (!std::strcmp(argv[i], "--water")) waterOn = true;
    }

    // ---- Tao cua so va context OpenGL 3.3 ----
#ifdef GLFW_PLATFORM_X11
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);  // GLEW can GLX (tren Wayland dung XWayland)
#endif
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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

    // ---- Dung hinh hoc (chi 1 lan) ----
    MeshBuilder worldB, lampB, dragonB, eyeB, hangerB, waterB, skyB;
    buildWorld(worldB, lampB);
    buildDragon(dragonB, eyeB, hangerB);
    waterB.box({-5000, -1, -5000}, {5000, 0, 5000}, vec3(1));
    skyB.ellipsoid({0, 0, 0}, {5000, 5000, 5000}, vec3(1));  // vom troi bao quanh canh
    Mesh world, lamps, dragon, eyes, hangers, water, sky;
    world.upload(worldB);
    lamps.upload(lampB);
    dragon.upload(dragonB);
    eyes.upload(eyeB);
    hangers.upload(hangerB);
    water.upload(waterB);
    sky.upload(skyB);

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
                "      F = phun lua | G = phun nuoc | 1 = toan canh | 2 = dau rong | 3 = goc mat song | Esc = thoat\n");
    setView(view);

    Lighting L = PRESETS[timeMode];
    double last = glfwGetTime(), fpsTimer = 0;
    int frames = 0, fpsCount = 0;

    // ======================== VONG LAP CHINH ========================
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = shotFile ? 1.0f / 60 : (float)std::fmin(0.1, now - last);
        last = now;
        glfwPollEvents();

        // ---- Cap nhat ----
        blend(L, PRESETS[timeMode], std::fmin(1.0f, dt * 2));
        updateParticles(dt);
        float flicker = 0.8f + 0.2f * std::sin(now * 37) * std::sin(now * 23);
        vec3 fireColor = fireOn ? vec3(1.0f, 0.55f, 0.18f) * (3.5f * flicker) : vec3(0);

        // Camera quy dao: mat nam tren mat cau quanh diem camTarget
        int fbw, fbh;
        glfwGetFramebufferSize(win, &fbw, &fbh);
        vec3 eye = camTarget + vec3(std::cos(camPitch) * std::cos(camYaw), std::sin(camPitch),
                                    std::cos(camPitch) * std::sin(camYaw)) * camDist;
        mat4 proj = perspective(55 * PI / 180, (float)fbw / fbh, 1.0f, 9000);
        mat4 viewProj = proj * lookAt(eye, camTarget, vec3(0, 1, 0));

        // ---- Ve ----
        glViewport(0, 0, fbw, fbh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(sceneShader);
        setMat4(sceneShader, "uViewProj", viewProj);
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
        setFloat(sceneShader, "uTime", (float)now);
        setFloat(sceneShader, "uLedOn", L.led);

        // uMode: 0 = vat the thuong, 1 = rong (co den LED), 2 = mat nuoc, 3 = bau troi
        setInt(sceneShader, "uMode", 3);
        glDepthMask(GL_FALSE);
        sky.draw();
        glDepthMask(GL_TRUE);
        setInt(sceneShader, "uMode", 0);
        setVec3(sceneShader, "uEmissive", vec3(0));
        world.draw();
        hangers.draw();
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
            float t = q.age / q.life;  // tuoi chuan hoa 0..1
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

        // Hien FPS tren thanh tieu de
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
