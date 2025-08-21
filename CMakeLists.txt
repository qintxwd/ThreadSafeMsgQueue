cmake_minimum_required(VERSION 3.14)
project(ThreadSafeMsgQueue VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Options
option(THREADSAFE_MSGQUEUE_BUILD_TESTS "Build tests" ON)
option(THREADSAFE_MSGQUEUE_BUILD_EXAMPLES "Build examples" ON)
option(THREADSAFE_MSGQUEUE_INSTALL "Install ThreadSafeMsgQueue" ON)

# Find dependencies
find_package(Threads REQUIRED)

# Create the header-only library target
add_library(ThreadSafeMsgQueue INTERFACE)
add_library(ThreadSafeMsgQueue::ThreadSafeMsgQueue ALIAS ThreadSafeMsgQueue)

# Set target properties
target_include_directories(ThreadSafeMsgQueue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(ThreadSafeMsgQueue INTERFACE cxx_std_14)
target_link_libraries(ThreadSafeMsgQueue INTERFACE Threads::Threads)

# Compiler-specific options
target_compile_options(ThreadSafeMsgQueue INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
)

# Add subdirectories
if(THREADSAFE_MSGQUEUE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(THREADSAFE_MSGQUEUE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
if(THREADSAFE_MSGQUEUE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

    # Install targets
    install(TARGETS ThreadSafeMsgQueue
        EXPORT ThreadSafeMsgQueueTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install export targets
    install(EXPORT ThreadSafeMsgQueueTargets
        FILE ThreadSafeMsgQueueTargets.cmake
        NAMESPACE ThreadSafeMsgQueue::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ThreadSafeMsgQueue
    )

    # Configure package config files
    configure_package_config_file(
        cmake/ThreadSafeMsgQueueConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeMsgQueueConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ThreadSafeMsgQueue
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeMsgQueueConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeMsgQueueConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeMsgQueueConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ThreadSafeMsgQueue
    )

    # Install documentation
    install(FILES README.md LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
    
    install(DIRECTORY docs/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        FILES_MATCHING PATTERN "*.md"
    )
endif()

# Print configuration info
message(STATUS "ThreadSafeMsgQueue Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Build Tests: ${THREADSAFE_MSGQUEUE_BUILD_TESTS}")
message(STATUS "  Build Examples: ${THREADSAFE_MSGQUEUE_BUILD_EXAMPLES}")
message(STATUS "  Install: ${THREADSAFE_MSGQUEUE_INSTALL}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")

# Export targets for use in build tree
export(EXPORT ThreadSafeMsgQueueTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/ThreadSafeMsgQueueTargets.cmake"
    NAMESPACE ThreadSafeMsgQueue::
)
