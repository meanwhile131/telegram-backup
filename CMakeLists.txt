cmake_minimum_required(VERSION 3.10)
project(TelegramBackup)

add_subdirectory(tdlib)
include_directories(src)
add_compile_options(-Wall)
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE Td::TdStatic)