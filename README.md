# Mô phỏng Cầu Rồng Đà Nẵng – OpenGL

Mô phỏng 3D Cầu Rồng bằng C++ và OpenGL 3.3: thân rồng uốn lượn theo đường cong với các vòm
tựa trên trụ và dây treo, bầu trời có mây, 3 khung cảnh ngày / hoàng hôn / đêm (đèn LED vàng),
phun lửa / phun nước, camera xoay quanh cầu.

Báo cáo: `BaoCao_CauRong.docx`.

## Chạy (Linux)
```bash
sudo apt install build-essential libglfw3-dev libglew-dev
make
./cau_rong
```
Chạy từ thư mục dự án (chương trình đọc shader trong `shaders/`).

Windows (vcpkg): `vcpkg install glfw3 glew`, sau đó build bằng `CMakeLists.txt`
và chạy file exe từ thư mục dự án.

## Điều khiển
| Phím | Chức năng |
|---|---|
| Chuột trái kéo | Xoay camera |
| Con lăn | Phóng to / thu nhỏ |
| N | Đổi khung cảnh: ngày → hoàng hôn → đêm |
| F | Bật/tắt phun lửa |
| G | Bật/tắt phun nước |
| 1 / 2 / 3 | Toàn cảnh / cận cảnh đầu rồng / từ mặt sông (giống ảnh chụp) |
| Esc | Thoát |

Khi mở, chương trình bắt đầu ở cảnh hoàng hôn, góc nhìn 3.

## Cấu trúc
```
src/main.cpp    ánh sáng 3 khung cảnh, dựng cầu, dựng rồng, hệ thống hạt, điều khiển, vòng lặp chính
src/mesh.h      sinh hình học (hộp, elipxoit, hình nón, hình trụ, ống) và đưa lên GPU
src/shader.h    nạp shader GLSL
src/math3d.h    vector, ma trận, phép chiếu, lookAt
shaders/        scene.vert/.frag (bầu trời, chiếu sáng, nước), particle.vert/.frag (hạt)
```
