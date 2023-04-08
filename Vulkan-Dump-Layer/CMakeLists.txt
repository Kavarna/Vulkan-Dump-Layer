cmake_minimum_required(VERSION 3.14)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("Vulkan-Dump-Layer-Library")

FILE(GLOB SOURCES "src/*")

add_library("Vulkan-Dump-Layer-Library" ${SOURCES})
target_link_libraries("Vulkan-Dump-Layer-Library" ${CONAN_LIBS})

make_filters("${SOURCES}")

set_property(TARGET "Vulkan-Dump-Layer-Library" PROPERTY CXX_STANDARD 20)

set(CMAKE_INSTALL_PREFIX ../bin)