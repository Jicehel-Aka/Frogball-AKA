# Frogball — version PC (SDL2).   cmake -S pc -B build-pc -DCMAKE_BUILD_TYPE=Release && cmake --build build-pc
cmake_minimum_required(VERSION 3.16)
project(frogball_pc CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
set(SRC ${CMAKE_SOURCE_DIR}/../main)
add_executable(frogball
  pc_main.cpp
  ${SRC}/game/frogball_core.cpp
  ${SRC}/game/frog_sound.cpp
  ${SRC}/ui/gfx_prims.cpp
  ${SRC}/ui/render.cpp
  ${SRC}/ui/sprites.cpp)
# Les sprites et le code supposent char non signe (comme sur l'ESP32-S3).
target_compile_options(frogball PRIVATE -funsigned-char -O2 -Wall -Wextra -Wno-unused-parameter)
if(TARGET SDL2::SDL2)
  target_link_libraries(frogball PRIVATE SDL2::SDL2)
else()
  target_include_directories(frogball PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(frogball PRIVATE ${SDL2_LIBRARIES})
endif()
if(WIN32)
  set_target_properties(frogball PROPERTIES WIN32_EXECUTABLE ON)
  target_link_options(frogball PRIVATE -static-libgcc -static-libstdc++)
endif()
