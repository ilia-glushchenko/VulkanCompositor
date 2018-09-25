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
	include/Helpers.hpp
    include/Render.hpp
    include/Structs.hpp
    include/Device.hpp
    include/Window.hpp
)

set(VULKAN_COMPOSITOR_SOURCES
    sources/Compositor.cpp
    sources/Render.cpp
    sources/Structs.cpp
    sources/Device.cpp
    sources/Window.cpp
)

add_library(${VULKAN_COMPOSITOR_LIB}
    ${VULKAN_COMPOSITOR_HEADERS}
    ${VULKAN_COMPOSITOR_SOURCES}
)

target_link_libraries(${VULKAN_COMPOSITOR_LIB}
    ${VULKAN_LIBRARY}
    ${GLFW_LIB}
)

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

#Compile GLSL shaders to SPIRV
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${VULKAN_COMPOSITOR_LIB} Shaders)

add_custom_command(TARGET ${VULKAN_COMPOSITOR_LIB} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${VULKAN_COMPOSITOR_LIB}>/shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${VULKAN_COMPOSITOR_LIB}>/shaders"
)

if(${VULKAN_COMPOSITOR_BUILD_DEMO})
    add_subdirectory("${VULKAN_COMPOSITOR_ROOT}/demo")
endif()
