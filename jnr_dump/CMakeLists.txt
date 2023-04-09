cmake_minimum_required(VERSION 3.14)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("jnr_dump")

# Add Vulkan
set(VULKAN_SDK "$ENV{VULKAN_SDK}")

include_directories("${VULKAN_SDK}/Include/vma/")
find_package(Vulkan REQUIRED FATAL_ERROR)

FILE(GLOB SOURCES "src/*")

add_library("jnr_dump" SHARED ${SOURCES})

include_directories("src")

target_link_libraries("jnr_dump" ${CONAN_LIBS})
target_link_libraries("jnr_dump" Vulkan::Vulkan)

make_filters("${SOURCES}")

set_property(TARGET "jnr_dump" PROPERTY CXX_STANDARD 20)

set(CMAKE_INSTALL_PREFIX ../bin)