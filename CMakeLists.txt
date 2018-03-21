# Copyright (C) 2018 by Ilya Glushchenko
# This code is licensed under the MIT license (MIT)
# (http://opensource.org/licenses/MIT)

cmake_minimum_required(VERSION 3.2)
cmake_policy(VERSION 3.2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VulkanCompositorConfig)
project(${VULKAN_COMPOSITOR_NAME})

#Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif(MSVC)

#Build options
option(VULKAN_COMPOSITOR_BUILD_DEMO "Building demo" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

#GLFW
include(GlfwConfig)
add_subdirectory(${GLFW_SOURCE_DIR} ${GLFW_BINARY_DIR})
include_directories(${GLFW_INCLUDE_DIR})

#Vulkan configs
add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS
    -DGLM_ENABLE_EXPERIMENTAL
    -DVULKAN_HPP_TYPESAFE_CONVERSION
    -DVULKAN_HPP_NO_SMART_HANDLE
)

if(UNIX)
    find_library(Vulkan REQUIRED)
endif()

include_directories(
    ${VULKAN_INCLUDE_DIR}
)

#Library configs
include_directories(
    ${VULKAN_COMPOSITOR_INCLUDE_DIRS}
)

set(VULKAN_COMPOSITOR_HEADERS
    include/Compositor.hpp
)

set(VULKAN_COMPOSITOR_SOURCES
    sources/Compositor.cpp
)

add_library(${VULKAN_COMPOSITOR_LIB}
    ${VULKAN_COMPOSITOR_HEADERS}
    ${VULKAN_COMPOSITOR_SOURCES}
)

target_link_libraries(${VULKAN_COMPOSITOR_LIB}
    ${VULKAN_LIBRARY}
    ${GLFW_LIB}
)

if(${VULKAN_COMPOSITOR_BUILD_DEMO})
    add_subdirectory("${VULKAN_COMPOSITOR_ROOT}/demo")
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Werror -pedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb3 -O0")
elseif(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14 /MP /W3")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
elseif(WIN32)
    if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
        set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
    else()
        set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
    endif()
endif()
