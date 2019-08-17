cmake_minimum_required(VERSION 3.13.1)

project(modular-glsl VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(LOCAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src)

add_compile_options(-Wall -Wextra -Wpedantic -g)

configure_file(
  ${CMAKE_SOURCE_DIR}/src/ShaderBuilder.h
  ${CMAKE_SOURCE_DIR}/include/
  COPYONLY)

add_library(modular-glsl.a STATIC
  ${CMAKE_SOURCE_DIR}/src/ShaderBuilder.h
  ${CMAKE_SOURCE_DIR}/src/ShaderBuilder.cpp
  )


add_executable(test ${CMAKE_SOURCE_DIR}/test/test.cpp )
target_include_directories(test PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_link_libraries(test modular-glsl.a stdc++fs )
