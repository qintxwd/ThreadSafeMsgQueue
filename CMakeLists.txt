cmake_minimum_required(VERSION 2.8.3)

add_compile_options(-std=c++11)

find_package(Threads)

add_executable(test test.cpp)

target_link_libraries(test ${CMAKE_THREAD_LIBS_INIT})
