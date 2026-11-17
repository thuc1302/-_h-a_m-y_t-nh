#version 330 core
// Chieu sang Blinn-Phong + bong do (shadow map) + den duong + LED rong
// + mat nuoc phan chieu (troi, mat cau, than rong) + bau troi co may + suong mu
in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
in float vS;
out vec4 FragColor;

uniform vec3 uCamPos;
uniform vec3 uLightDir;   // huong toi mat troi / mat trang
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uSkyTop;     // mau dinh troi
uniform vec3 uFogColor;   // mau chan troi (cung la mau suong mu)
uniform vec3 uSunGlow;    // quang sang quanh mat troi
uniform vec3 uWaterColor; // mau nuoc sau
uniform vec3 uFirePos;    // vi tri ngon lua (den diem)
uniform vec3 uFireColor;
uniform vec3 uEmissive;   // mau tu phat sang (bong den, mat rong)
uniform float uEmitVtx;   // >0: dinh tu phat sang bang chinh mau cua no (den xe)
uniform int uMode;        // 0 = thuong, 1 = rong, 2 = mat nuoc, 3 = bau troi, 4 = nha (cua so)
uniform float uLedOn;     // 0 = tat, 1 = bat (ban dem)
uniform float uTime;
uniform float uDeckY;     // do cao mat cau

uniform mat4 uLightVP;              // ma tran the gioi -> khong gian anh sang
uniform sampler2DShadow uShadowMap; // do sau nhin tu mat troi
uniform float uShadowBias;
uniform sampler2D uDragonProfile;   // x -> (do cao tam than rong, ban kinh): ve bong rong duoi nuoc

const float HALF_LEN = 333.0;  // phai khop voi main.cpp
const float HALF_W = 18.75;
const float LAMP_SP = 36.0;

// ---------------- Nhieu (value noise + fbm) ----------------
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        s += a * noise(p);
        p = p * 2.03 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return s;
}

// ---------------- Bau troi ----------------
vec3 skyColor(vec3 dir) {
    float h = max(dir.y, 0.0);
    vec3 sky = mix(uFogColor, uSkyTop, pow(h, 0.45));
    float sd = max(dot(dir, normalize(uLightDir)), 0.0);
    sky += uSunGlow * (0.45 * pow(sd, 8.0) + 0.3 * pow(sd, 64.0) + 4.0 * pow(sd, 2000.0));
    // May: chieu huong nhin len "tran may" phang, lay fbm
    vec2 p = dir.xz / (h + 0.06) * 0.9 + vec2(uTime * 0.004, 0.0);
    float cl = smoothstep(0.42, 0.78, fbm(p)) * smoothstep(0.0, 0.12, h);
    vec3 cloudCol = uFogColor * 0.55 + uLightColor * 0.5 + uSunGlow * 0.35;
    cloudCol *= 0.78 + 0.3 * fbm(p * 2.3 + 5.0);  // mat duoi may toi hon
    return mix(sky, cloudCol, cl * 0.85);
}

// ---------------- Bong do: PCF 3x3 tren shadow map ----------------
float shadowAt(vec3 P, vec3 N) {
    vec4 lp = uLightVP * vec4(P + N * 0.12, 1.0);
    vec3 c = lp.xyz / lp.w * 0.5 + 0.5;
    if (c.x <= 0.0 || c.x >= 1.0 || c.y <= 0.0 || c.y >= 1.0 || c.z >= 1.0) return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float s = 0.0;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) s += texture(uShadowMap, vec3(c.xy + vec2(i, j) * texel, c.z - uShadowBias));
    return s / 9.0;
}

// ---------------- Den duong ban dem (cot den moi LAMP_SP met, 2 ben) ----------------
vec3 streetLights(vec3 P, vec3 N) {
    if (uLedOn < 0.01 || abs(P.x) > 920.0 || abs(P.z) > 24.0 || P.y > uDeckY + 11.0 || P.y < uDeckY - 1.0)
        return vec3(0.0);
    float lx = floor(P.x / LAMP_SP + 0.5) * LAMP_SP;
    vec3 sum = vec3(0.0);
    for (int k = -1; k <= 1; k++)
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 d = vec3(lx + float(k) * LAMP_SP, uDeckY + 10.8, float(sd) * (HALF_W - 3.9)) - P;
            sum += max(dot(N, normalize(d)), 0.0) / (1.0 + dot(d, d) / 70.0);
        }
    return sum * vec3(1.0, 0.78, 0.5) * 2.2 * uLedOn;
}

// ---------------- Phan chieu cau tren mat nuoc (do tia phan xa voi mat cau va than rong) ----------------
vec4 bridgeReflection(vec3 P, vec3 r) {
    if (r.y <= 0.0) return vec4(0.0);
    float D = uDeckY;
    vec3 lit = uAmbient + uLightColor * 0.6;
    if (abs(P.z) > HALF_W) {
        // Mep be tong + lan can nhin tu ben canh (mat phang z = +-HALF_W)
        if (abs(r.z) > 1e-4 && sign(r.z) != sign(P.z)) {
            vec3 h = P + r * ((sign(P.z) * HALF_W - P.z) / r.z);
            if (abs(h.x) < HALF_LEN && h.y > D - 2.6 && h.y < D + 1.4) {
                return vec4(vec3(0.33, 0.32, 0.3) * lit + vec3(1.0, 0.8, 0.5) * 0.25 * uLedOn, 1.0);
            }
        }
    } else {
        // Duoi gam cau: thay mat duoi dam toi
        vec3 h = P + r * ((D - 2.6 - P.y) / r.y);
        if (abs(h.x) < HALF_LEN) return vec4(vec3(0.25, 0.25, 0.24) * uAmbient, 1.0);
    }
    // Than rong nam trong mat phang z = 0
    if (abs(r.z) > 1e-4 && sign(r.z) != sign(P.z)) {
        vec3 h = P + r * (-P.z / r.z);
        float u = (h.x + 340.0) / 680.0;
        if (u > 0.0 && u < 1.0) {
            vec2 pr = texture(uDragonProfile, vec2(u, 0.5)).rg;
            float m = 1.0 - smoothstep(pr.y * 0.75, pr.y * 1.1, abs(h.y - pr.x));
            vec3 c = vec3(1.0, 0.76, 0.06) * lit * 0.75 + vec3(1.0, 0.75, 0.2) * 1.3 * uLedOn;
            return vec4(c, m);
        }
    }
    return vec4(0.0);
}

void main() {
    vec3 V = normalize(uCamPos - vPos);
    if (uMode == 3) {  // bau troi: khong chieu sang, khong suong mu
        FragColor = vec4(skyColor(normalize(vPos - uCamPos)), 1.0);
        return;
    }
    vec3 N = normalize(vNormal);
    vec3 color = vColor;
    vec3 emissive = uEmissive + vColor * uEmitVtx;
    float shininess = 32.0, specStrength = 0.12;

    if (uMode == 0) color *= 0.92 + 0.08 * noise(vPos.xz * 1.3 + vPos.y * 0.7);  // van be mat nhe
    if (uMode == 1) {
        // Son vang bong + den LED vang gon nhe chay doc than rong ban dem
        vec3 led = vec3(1.0, 0.75, 0.2) * (0.75 + 0.25 * sin(vS * 40.0 - uTime * 2.0));
        emissive += led * uLedOn;
        specStrength = 0.55;
        shininess = 48.0;
    }
    if (uMode == 4 && vS > 0.0 && abs(N.y) < 0.5) {
        // Nha cao tang: luoi cua so theo tang; vS > 0.5 = toa nha kinh
        bool glassy = vS > 0.5;
        vec2 uv = vec2(abs(N.x) > 0.5 ? vPos.z : vPos.x, vPos.y - uDeckY);
        vec2 cell = uv / vec2(glassy ? 2.0 : 3.4, 3.5);
        vec2 f = fract(cell);
        float win = glassy ? step(0.05, f.x) * step(0.12, f.y) * step(f.y, 0.92)
                           : step(0.2, f.x) * step(f.x, 0.8) * step(0.28, f.y) * step(f.y, 0.78);
        if (uv.y < 4.0) win = 0.0;  // tang tret
        vec3 glassCol = mix(vec3(0.08, 0.11, 0.15), skyColor(reflect(-V, N)), 0.45);
        color = mix(color, glassCol, win * (glassy ? 0.7 : 0.85));
        float lit = step(0.55, hash(floor(cell) + vec2(vS * 173.0, vS * 91.0)));
        emissive += win * lit * vec3(1.0, 0.8, 0.52) * 0.85 * uLedOn;
        specStrength = 0.35 * win;
        shininess = 64.0;
    }
    if (uMode == 2) {
        // Mat nuoc: song bang tong cac ham cos lam lech phap tuyen
        // song nho mo dan theo khoang cach de tranh rang cua (aliasing) o xa
        vec2 q = vPos.xz;
        float dc = length(uCamPos - vPos);
        float near1 = 0.55 * clamp(1.0 - dc / 900.0, 0.2, 1.0), near2 = 0.5 * clamp(1.0 - dc / 350.0, 0.0, 1.0);
        N = normalize(vec3(near1 * (0.03 * cos(q.x * 0.11 + q.y * 0.05 + uTime * 1.2) + 0.02 * cos(q.x * 0.37 + q.y * 0.21 + uTime * 1.9)) +
                               near2 * 0.015 * cos(q.x * 1.3 - q.y * 0.9 + uTime * 3.1),
                           1.0,
                           near1 * (0.03 * cos(q.y * 0.13 - q.x * 0.04 + uTime * 0.9) + 0.02 * cos(q.y * 0.43 - q.x * 0.27 + uTime * 2.2)) +
                               near2 * 0.015 * cos(q.y * 1.1 + q.x * 0.7 + uTime * 2.7)));
        float fres = 0.3 + 0.65 * pow(1.0 - max(dot(N, V), 0.0), 3.0);
        vec3 r = reflect(-V, N);
        color = mix(uWaterColor, skyColor(r), fres);
        vec4 obj = bridgeReflection(vPos, r);
        color = mix(color, obj.rgb, obj.a * (0.45 + 0.35 * fres));
        shininess = 220.0;
        specStrength = 1.5;
    }

    // ---- Blinn-Phong voi nguon sang chinh + bong do ----
    vec3 L = normalize(uLightDir);
    float sh = shadowAt(vPos, N);
    vec3 H = normalize(L + V);
    float diff = max(dot(N, L), 0.0) * sh;
    float spec = pow(max(dot(N, H), 0.0), shininess) * specStrength * sh;
    vec3 hemi = uAmbient * (0.62 + 0.38 * N.y);  // anh sang troi chieu tu tren nhieu hon
    float waterShade = 1.0 - 0.3 * (1.0 - sh) * smoothstep(0.2, 0.45, L.y);  // mat troi thap: bong qua dai, bo qua
    vec3 result = (uMode == 2) ? color * waterShade + spec * uLightColor
                               : color * (hemi + diff * uLightColor) + spec * uLightColor;

    // ---- Den diem tu ngon lua ----
    vec3 Lf = uFirePos - vPos;
    float d = length(Lf);
    result += vColor * max(dot(N, Lf / d), 0.0) * uFireColor / (1.0 + d * d / 1600.0);

    if (uMode != 2) result += vColor * streetLights(vPos, N);
    // Anh LED vang cua rong hat xuong mat cau ban dem
    if (uMode == 0 && abs(vPos.x) < HALF_LEN && vPos.y > uDeckY - 0.5 && vPos.y < uDeckY + 4.0)
        result += vColor * vec3(1.0, 0.72, 0.25) * (0.6 * uLedOn * exp(-abs(vPos.z) / 4.5));

    result += emissive;

    // ---- Suong mu ham mu: vat cang xa cang hoa vao mau chan troi (phoi canh khi quyen) ----
    float fog = 1.0 - exp(-max(length(uCamPos - vPos) - 150.0, 0.0) / 2400.0);
    FragColor = vec4(mix(result, uFogColor, fog), 1.0);
}
