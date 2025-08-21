# Examples CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Common example configuration
set(EXAMPLE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Function to create an example executable
function(add_example_executable example_name example_file)
    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} PRIVATE ThreadSafeMsgQueue::ThreadSafeMsgQueue)
    target_include_directories(${example_name} PRIVATE ${EXAMPLE_INCLUDE_DIRS})
    
    # Set compiler flags
    if(MSVC)
        target_compile_options(${example_name} PRIVATE /W4)
    else()
        target_compile_options(${example_name} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

# Simple Example
add_example_executable(simple_example simple_example.cpp)

# Basic Usage Example
add_example_executable(basic_usage basic_usage.cpp)

# SLAM System Example
add_example_executable(slam_example slam_example.cpp)

# Performance Demo
add_example_executable(performance_demo performance_demo.cpp)

# Comprehensive PubSub Demo
add_example_executable(comprehensive_pubsub_demo comprehensive_pubsub_demo.cpp)

# Print examples configuration
message(STATUS "Examples Configuration:")
message(STATUS "  Example Include Dirs: ${EXAMPLE_INCLUDE_DIRS}")
message(STATUS "  Available Examples: simple_example, basic_usage, slam_example, performance_demo, comprehensive_pubsub_demo")
