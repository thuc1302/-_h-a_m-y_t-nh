cmake_minimum_required(VERSION 3.16)
project(CauRong CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)

add_executable(cau_rong src/main.cpp)
target_link_libraries(cau_rong PRIVATE glfw GLEW::GLEW OpenGL::GL)
