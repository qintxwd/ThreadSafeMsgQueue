# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Common test configuration
set(TEST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Function to create a test executable
function(add_test_executable test_name test_file)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE ThreadSafeMsgQueue::ThreadSafeMsgQueue)
    target_include_directories(${test_name} PRIVATE ${TEST_INCLUDE_DIRS})
    
    # Set compiler flags
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -pedantic)
    endif()
    
    # Add test to CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# ODR Compliance Test
add_test_executable(odr_test odr_test.cpp)

# Performance Benchmark Test
add_test_executable(performance_test performance_test.cpp)

# Comprehensive PubSub System Test
add_test_executable(comprehensive_pubsub_test comprehensive_pubsub_test.cpp)

# Message Queue Unit Tests
add_test_executable(msgqueue_unit_test msgqueue_unit_test.cpp)

# Callback System Tests
add_test_executable(callback_test callback_test.cpp)

# Integration Tests
add_test_executable(integration_test integration_test.cpp)

# Print test configuration
message(STATUS "Tests Configuration:")
message(STATUS "  Test Include Dirs: ${TEST_INCLUDE_DIRS}")
message(STATUS "  Available Tests: odr_test, performance_test, comprehensive_pubsub_test, msgqueue_unit_test, callback_test, integration_test")
