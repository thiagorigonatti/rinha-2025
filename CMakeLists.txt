cmake_minimum_required(VERSION 3.25.1)
project(rinha-2025 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(rinha-2025
        src/main.c
        src/lock_free_queue.c
        src/lock_free_queue.h
        src/time.h
        src/http_client.c
        src/http_client.h
        src/shm_array.c
        src/shm_array.h
        src/settings.h
        src/payment.h
        src/http_server.c
        src/http_server.h
)

target_compile_options(rinha-2025 PRIVATE -Wall)
