.PHONY: all clean
PROJECT_FOLDER := $(notdir $(CURDIR))

all:
	cmake --build build -j$(($(nproc) + 1))

cm:
	cmake -S . -B build

d:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build -j$(($(nproc) + 1))
r:
	./build/$(PROJECT_FOLDER)

s:
	./build/file_server_server

gs:
	gdb ./build/file_server_server

c:
	./build/file_server_client

gc:
	gdb ./build/file_server_client

bt:
	cmake --build build -j$(($(nproc) + 1)) --target file_server_tests
t:
	./build/tests/file_server_tests

g:
	gdb ./build/$(PROJECT_FOLDER)

clean:
	rm -rf build