cmake_minimum_required(VERSION 3.10)
project(cbuild)
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
add_executable(cbuild ${CMAKE_SOURCE_DIR}/src/main.c)
# 安装规则
install(TARGETS cbuild
    RUNTIME DESTINATION bin
)