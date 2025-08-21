cmake_minimum_required(VERSION 3.10)
project(ThreadSafeMsgQueue VERSION 2.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threads package
find_package(Threads REQUIRED)

# Add executable for ODR test
add_executable(odr_test odr_test.cpp)

# Link with threads
target_link_libraries(odr_test Threads::Threads)

# Set compiler flags
if(MSVC)
    target_compile_options(odr_test PRIVATE /W4)
else()
    target_compile_options(odr_test PRIVATE -Wall -Wextra -pedantic)
endif()

# Add include directory (current directory contains all headers)
target_include_directories(odr_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable testing
enable_testing()
add_test(NAME ODRComplianceTest COMMAND odr_test)

# Optional: Create a simple example executable
add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example Threads::Threads)
target_include_directories(simple_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Performance benchmark executable
add_executable(performance_test performance_test.cpp)
target_link_libraries(performance_test Threads::Threads)
target_include_directories(performance_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Legacy test executable (keep for compatibility)
add_executable(legacy_test test.cpp)
target_link_libraries(legacy_test Threads::Threads)
target_include_directories(legacy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Print configuration info
message(STATUS "ThreadSafeMsgQueue Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")

# Header-only library target (for use with find_package)
add_library(ThreadSafeMsgQueue INTERFACE)
target_include_directories(ThreadSafeMsgQueue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(ThreadSafeMsgQueue INTERFACE cxx_std_14)
target_link_libraries(ThreadSafeMsgQueue INTERFACE Threads::Threads)
