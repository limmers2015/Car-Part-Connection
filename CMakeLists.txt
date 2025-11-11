cmake_minimum_required(VERSION 3.15)
project(car_part_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add near the top, after project(...)
find_package(PostgreSQL REQUIRED)

# ...

add_executable(api
  src/main.c
  src/http.c
  src/json.c
  src/db.c
  src/sessions.c
  src/auth.c
  src/vehicles.c
  src/util.c
)

# Dependencies:
# - jansson (JSON)
# - hiredis (Redis client)
# - pq (PostgreSQL client)
# - sodium (libsodium for Argon2id)
# On Debian/Ubuntu: sudo apt-get install -y libjansson-dev libhiredis-dev libpq-dev libsodium-dev

# Replace the plain "pq" link with the variables from find_package:
target_link_libraries(api
  jansson
  hiredis
  sodium
  ${PostgreSQL_LIBRARIES}
)

# Ensure headers are on the include path:
target_include_directories(api PRIVATE ${PostgreSQL_INCLUDE_DIRS})

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# On Linux, link rt if necessary (older glibc)
if(UNIX AND NOT APPLE)
  target_link_libraries(api rt)
endif()
