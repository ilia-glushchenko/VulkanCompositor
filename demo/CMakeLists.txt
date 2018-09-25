# Copyright (C) 2018 by Ilya Glushchenko
# This code is licensed under the MIT license (MIT)
# (http://opensource.org/licenses/MIT)

list(APPEND CMAKE_MODULE_PATH "${VULKAN_COMPOSITOR_ROOT}/demo/cmake")
include(VulkanCompositorDemoConfig)
project(${VULKAN_COMPOSITOR_DEMO_PROJECT})

set(VULKAN_COMPOSITOR_DEMO_SOURCES
    Main.cpp
)

add_executable(${VULKAN_COMPOSITOR_DEMO_NAME}
    ${VULKAN_COMPOSITOR_DEMO_SOURCES}
)

target_link_libraries(${VULKAN_COMPOSITOR_DEMO_NAME}
    ${VULKAN_COMPOSITOR_LIB}
)
