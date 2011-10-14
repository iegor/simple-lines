cmake_minimum_required(VERSION 2.6)

project(Lines C CXX)

set(CMAKE_C_FLAGS_DEBUG "-Wall -g")

add_executable(lines_for_mom lines.c)
target_link_libraries(lines_for_mom X11 GL)
#add_executable(some_test ../cb_app_gametest_opengl/main.c)
