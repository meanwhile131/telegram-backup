cmake_minimum_required(VERSION 3.10)
project(TelegramBackup)

add_subdirectory(td)
add_compile_options(-Wall)
add_executable(main src/main.cpp src/TelegramBackup.cpp)
target_include_directories(main PRIVATE include)
target_link_libraries(main PRIVATE Td::TdStatic)