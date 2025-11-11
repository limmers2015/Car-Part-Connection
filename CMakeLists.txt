cmake_minimum_required(VERSION 3.15)
project(car_part_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies:
# - jansson (JSON)
# - hiredis (Redis client)
# - pq (PostgreSQL client)
# - sodium (libsodium for Argon2id)
# On Debian/Ubuntu: sudo apt-get install -y libjansson-dev libhiredis-dev libpq-dev libsodium-dev

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

target_link_libraries(api
  jansson
  hiredis
  pq
  sodium
)

# On Linux, link rt if necessary (older glibc)
if(UNIX AND NOT APPLE)
  target_link_libraries(api rt)
endif()
