cmake_minimum_required(VERSION 3.20)
project(sdl3_app C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL3 package
find_package(SDL3 REQUIRED CONFIG)

add_executable(sdl3_app
	src/main.c
	src/math3d.c
	src/render3d.c
	src/camera.c
	src/bird.c
	src/terrain.c
	src/ring.c
	src/audio.c
)
target_link_libraries(sdl3_app PRIVATE SDL3::SDL3 m)
