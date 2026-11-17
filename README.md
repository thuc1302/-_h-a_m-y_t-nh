# Mô phỏng Cầu Rồng Đà Nẵng – OpenGL

Mô phỏng 3D Cầu Rồng bằng C++ và OpenGL 3.3:
- Thân rồng là giàn 3 ống thép vàng quét dọc spline Catmull-Rom, giằng tam giác, 2 hàng vây lưng;
  các vòm chui xuống dưới mặt cầu qua bệ bê tông trắng, dây treo trắng hình chữ V.
- Đầu rồng ghép từ các tấm thép (mắt đĩa có vòng lỗ, bờm xoè, sừng, râu), phun lửa / phun nước.
- Mặt cầu 6 làn, dải phân cách và vỉa hè lát gạch đỏ, lan can xanh, đèn đường cần cong, cờ đuôi nheo.
- Xe ô tô, xe buýt, xe máy chạy trên cầu; thuyền trên sông.
- Thành phố hai bờ (cửa sổ sáng đèn ban đêm), hàng cây ven sông, dãy núi xa sinh theo tổng sin.
- Bóng đổ (shadow map), mặt nước phản chiếu trời + thân rồng, sương mù khí quyển,
  3 khung cảnh ngày / hoàng hôn / đêm (LED vàng, đèn đường).

Báo cáo: `BaoCao_CauRong.docx`.

## Chạy (macOS)
```bash
brew install glfw glew
make
./cau_rong
```

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
| 1 / 2 / 3 / 4 | Toàn cảnh trên cao / đầu rồng từ mặt đường / từ mặt sông / flycam dọc cầu |
| Esc | Thoát |

Khi mở, chương trình bắt đầu ở cảnh ban ngày, góc nhìn 4.
Tuỳ chọn dòng lệnh: `--mode 0|1|2`, `--view 1..4`, `--fire`, `--water`, `--shot anh.ppm`.

## Cấu trúc
```
src/main.cpp    ánh sáng 3 khung cảnh, dựng cầu, dựng rồng, hệ thống hạt, điều khiển, vòng lặp chính
src/mesh.h      sinh hình học (hộp, elipxoit, nón, trụ, thanh, lăng trụ, ống) và đưa lên GPU
src/shader.h    nạp shader GLSL
src/math3d.h    vector, ma trận, phép chiếu, lookAt
shaders/        scene.vert/.frag (trời, chiếu sáng, bóng đổ, nước, cửa sổ), shadow.vert/.frag, particle.vert/.frag
```
