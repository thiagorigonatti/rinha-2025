cmake_minimum_required(VERSION 3.25.1)
project(rinha C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Adiciona o executável
add_executable(rinha
        main.c
        http_server.h
        http_server.c
        queue.c
        queue.h
        timestamp.c
        timestamp.h
        http_client.c
        http_client.h
        range_array.c
        range_array.h
        aux.h
)

# Define as opções de compilação
target_compile_options(rinha PRIVATE -Wall)
