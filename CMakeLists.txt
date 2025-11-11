cmake_minimum_required(VERSION 3.15)
project(car_part_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable extra warnings (tweak as you like)
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(api
  src/main.c
)

# On Linux, link with rt if necessary (old glibc); harmless otherwise
if(UNIX AND NOT APPLE)
  target_link_libraries(api rt)
endif()
