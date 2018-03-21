# Copyright (C) 2017 by Godlike
# This code is licensed under the MIT license (MIT)
# (http://opensource.org/licenses/MIT)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Flag to override default GLFW_BUILD_EXAMPLES value")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Flag to override default GLFW_BUILD_TESTS value")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Flag to override default GLFW_BUILD_DOCS value")
set(GLFW_INSTALL OFF CACHE BOOL "Flag to override default GLFW_INSTALL value")
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "Flag to override default GLFW_VULKAN_STATIC value")
set(GLFW_DOCUMENT_INTERNALS OFF CACHE BOOL "Flag to override default GLFW_DOCUMENT_INTERNALS value")

set(GLFW_SOURCE_DIR "${VULKAN_COMPOSITOR_ROOT}/third_party/glfw" CACHE STRING "Path to GLFW root directory")
set(GLFW_BINARY_DIR "${CMAKE_BINARY_DIR}/glfw" CACHE STRING "Path to GLFW binary directory")
set(GLFW_LIB "glfw" CACHE STRING "Name of GLFW library")

set(GLFW_INCLUDE_DIR
    "${GLFW_SOURCE_DIR}/include"
    "${GLFW_SOURCE_DIR}/deps"
    CACHE LIST "List of GLFW include directories")
