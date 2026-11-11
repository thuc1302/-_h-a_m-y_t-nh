# Can: sudo apt install build-essential libglfw3-dev libglew-dev
CXX     ?= g++
LIBS    ?= -lglfw -lGLEW -lGL

cau_rong: src/*.cpp src/*.h
	$(CXX) -std=c++17 -O2 -Wall src/main.cpp -o cau_rong $(LIBS)

run: cau_rong
	./cau_rong

clean:
	rm -f cau_rong
