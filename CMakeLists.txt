cmake_minimum_required(VERSION 3.14)

project("Vulkan-Dump-Layer")

# Useful functions
macro(make_filters _source_list)
    foreach(_source IN ITEMS ${_source_list})
        get_filename_component(_source_path "${_source}" PATH)
        string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
        string(REPLACE "/" "\\" _group_path "${_group_path}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endmacro()


if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/JMC) # Just My Code
    add_compile_options(/MP)  # Multiprocess compilation
endif()

# Start building
message("-- Build type = ${CMAKE_BUILD_TYPE}")
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-DDEBUG)
else()
    add_definitions(-DRELEASE)
endif()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_INSTALL_PREFIX ../bin)
set(CURRENT_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DAPPLICATION_VERSION="0.0.0")
add_definitions(-DNOMINMAX)
add_definitions(-DWIN32_LEAN_AND_MEAN)

add_subdirectory("Vulkan-Dump-Layer")
