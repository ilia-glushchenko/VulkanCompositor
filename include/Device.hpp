/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <vulkan/vulkan.hpp>

namespace vkc
{

class Device
{
public:
    struct Queue
    {
        vk::Queue queue;
        uint32_t index = 0;
        uint32_t familyIndex = 0;
    };

    Device() = default;

    Device(const char* appName, uint32_t appVersion, const char* engineName, uint32_t engineVersion, std::vector<vk::PhysicalDeviceType> gpuTypes);

    ~Device();

    Device(Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&) = delete;
    Device& operator=(Device&&) = delete;

    bool Init();

    void Shutdown();

    std::string const applicationName = "NewCompositor";
    uint32_t const applicationVersion = 1;
    std::string const engineName = "Compositor";
    uint32_t const engineVersion = 1;
    std::vector<vk::PhysicalDeviceType> const gpuTypes{ vk::PhysicalDeviceType::eDiscreteGpu };
    vk::Result status = vk::Result::eErrorInitializationFailed;

    vk::Instance instance;
    vk::PhysicalDevice physical;
    vk::Device logical;
    Queue queue;

private:
    bool CreateInstance();

    bool FindPhysicalDevice();

    bool CreateLogicalDevice();
};

} // vkc namespace