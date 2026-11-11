#version 330 core
// Chieu sang Blinn-Phong + den diem (ngon lua) + den LED + mat nuoc + bau troi + suong mu
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
uniform int uMode;        // 0 = thuong, 1 = rong, 2 = mat nuoc, 3 = bau troi
uniform float uLedOn;     // 0 = tat, 1 = bat (ban dem)
uniform float uTime;

// Nhieu gia tri (value noise) don gian de ve may
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

// Mau bau troi theo huong nhin dir
vec3 skyColor(vec3 dir) {
    float h = max(dir.y, 0.0);
    vec3 sky = mix(uFogColor, uSkyTop, pow(h, 0.5));
    float sd = max(dot(dir, normalize(uLightDir)), 0.0);
    sky += uSunGlow * (0.6 * pow(sd, 6.0) + 3.0 * pow(sd, 1500.0));  // quang + dia mat troi
    // May: chieu huong nhin len mot "tran may" phang roi lay nhieu 2 tang
    vec2 p = dir.xz / (h + 0.08) * 1.5 + vec2(uTime * 0.01, 0.0);
    float cl = 0.65 * noise(p) + 0.35 * noise(p * 2.7);
    cl = smoothstep(0.5, 0.85, cl) * smoothstep(0.02, 0.2, h);
    vec3 cloudCol = mix(uSkyTop * 1.4, uFogColor * 1.1 + uSunGlow * 0.3, 0.6);
    return mix(sky, cloudCol, cl * 0.7);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 color = vColor;
    vec3 emissive = uEmissive;
    float shininess = 32.0, specStrength = 0.3;
    vec3 V = normalize(uCamPos - vPos);

    if (uMode == 3) {  // bau troi: khong chieu sang, khong suong mu
        FragColor = vec4(skyColor(normalize(vPos)), 1.0);
        return;
    }
    if (uMode == 1) {
        // Den LED mau vang: do sang gon nhe chay doc than rong (theo s va thoi gian)
        vec3 led = vec3(1.0, 0.75, 0.2) * (0.75 + 0.25 * sin(vS * 40.0 - uTime * 2.0));
        emissive += led * uLedOn;
        specStrength = 0.7;
    }
    if (uMode == 2) {
        // Mat nuoc: lam gon song bang cach lam lech phap tuyen theo ham sin
        N = normalize(vec3(0.05 * cos(vPos.x * 0.15 + uTime * 1.5) + 0.03 * cos(vPos.x * 0.6 + vPos.z * 0.3 + uTime * 2.3),
                           1.0,
                           0.05 * cos(vPos.z * 0.2 + uTime * 1.1)));
        // Mau nuoc = tron mau nuoc sau voi mau troi phan chieu (nhin cang nghieng cang phan chieu nhieu)
        float fres = 0.3 + 0.6 * pow(1.0 - max(dot(N, V), 0.0), 3.0);
        color = mix(uWaterColor, skyColor(reflect(-V, N)), fres);
        shininess = 128.0;
        specStrength = 0.8;
    }

    // ---- Blinn-Phong voi nguon sang chinh ----
    vec3 L = normalize(uLightDir);
    vec3 H = normalize(L + V);
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), shininess) * specStrength;
    vec3 result = (uMode == 2) ? color + spec * uLightColor  // nuoc da co mau troi, chi them vet nang
                               : color * (uAmbient + diff * uLightColor) + spec * uLightColor;

    // ---- Den diem tu ngon lua, suy giam theo khoang cach ----
    vec3 Lf = uFirePos - vPos;
    float d = length(Lf);
    Lf /= d;
    float att = 1.0 / (1.0 + d * d / 1600.0);
    result += vColor * max(dot(N, Lf), 0.0) * uFireColor * att;

    result += emissive;

    // ---- Suong mu tuyen tinh: vat cang xa cang hoa vao mau chan troi ----
    float fog = clamp((length(uCamPos - vPos) - 400.0) / 2000.0, 0.0, 1.0);
    FragColor = vec4(mix(result, uFogColor, fog), 1.0);
}
