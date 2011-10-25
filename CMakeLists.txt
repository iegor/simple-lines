cmake_minimum_required(VERSION 2.6)

project(Lines C)

set(CMAKE_C_FLAGS_DEBUG "-Wall -g")

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	message("DEBUG build type")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DVISUAL_DEBUG")
endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

add_executable(lines_for_mom lines.c)
target_link_libraries(lines_for_mom X11 GL)
#add_executable(some_test ../cb_app_gametest_opengl/main.c)
