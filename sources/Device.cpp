/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Device.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

namespace vkc
{

Device::Device(const char * appName, uint32_t appVersion, const char * engineName, uint32_t engineVersion, std::vector<vk::PhysicalDeviceType> gpuTypes)
    : applicationName(appName)
    , applicationVersion(appVersion)
    , engineName(engineName)
    , engineVersion(engineVersion)
    , gpuTypes(gpuTypes)
{
}

Device::~Device() { Shutdown(); }

bool Device::Init()
{
    return CreateInstance() && FindPhysicalDevice() && CreateLogicalDevice();
}

void Device::Shutdown()
{
    if (logical)
        logical.destroy();

    if (instance)
        instance.destroy();
}

bool Device::CreateInstance()
{
    glfwInit();
    if (!glfwVulkanSupported())
    {
        std::cerr << "Vulkan is not supported" << std::endl;
        return false;
    }

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<char const*> enabledInstanceExtensions(glfwExts, glfwExts + glfwExtCount);
    enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    std::vector<char const*> enabledInstanceLayers{ "VK_LAYER_LUNARG_standard_validation" };

    vk::ApplicationInfo appInfo;
    appInfo.setApiVersion(VK_API_VERSION_1_1);
    appInfo.setApplicationVersion(applicationVersion);
    appInfo.setPApplicationName(applicationName.c_str());
    appInfo.setPEngineName(engineName.c_str());
    appInfo.setEngineVersion(engineVersion);

    vk::InstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.setPApplicationInfo(&appInfo);
    instanceCreateInfo.setEnabledLayerCount(1);
    instanceCreateInfo.setEnabledLayerCount(static_cast<uint32_t>(enabledInstanceLayers.size()));
    instanceCreateInfo.setPpEnabledLayerNames(enabledInstanceLayers.data());
    instanceCreateInfo.setEnabledExtensionCount(static_cast<uint32_t>(enabledInstanceExtensions.size()));
    instanceCreateInfo.setPpEnabledExtensionNames(enabledInstanceExtensions.data());

    std::tie(status, instance) = vk::createInstance(instanceCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create Vulkan instance." << std::endl;
        return false;
    }

    return true;
}

bool Device::FindPhysicalDevice()
{
    std::vector<vk::PhysicalDevice> physicalDevices;
    std::tie(status, physicalDevices) = instance.enumeratePhysicalDevices();
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to find Vulkan capable devices." << std::endl;
        return false;
    }

    for (auto gpu : gpuTypes)
    {
        auto physicalDevice = std::find_if(physicalDevices.begin(), physicalDevices.end(),
            [gpu](vk::PhysicalDevice& device) {
            return (device.getProperties().deviceType == gpu);
        });

        if (physicalDevice != physicalDevices.end())
        {
            auto const queueFamilyProperties = physicalDevice->getQueueFamilyProperties();

            for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
            {
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
                    && glfwGetPhysicalDevicePresentationSupport(instance, *physicalDevice, i))
                {
                    physical = *physicalDevice;
                    queue.familyIndex = i;
                    return true;
                }
            }

        }
    }

    status = vk::Result::eErrorInitializationFailed;
    std::cerr << "Failed to find discrete gpu." << std::endl;
    return false;
}

bool Device::CreateLogicalDevice()
{
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
    float const priority = 1.0f;
    deviceQueueCreateInfo.setPQueuePriorities(&priority);
    deviceQueueCreateInfo.setQueueCount(1);
    deviceQueueCreateInfo.setQueueFamilyIndex(queue.familyIndex);

    char const* deviceExtensionNames = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.setQueueCreateInfoCount(1);
    deviceCreateInfo.setPQueueCreateInfos(&deviceQueueCreateInfo);
    deviceCreateInfo.setEnabledExtensionCount(1);
    deviceCreateInfo.setPpEnabledExtensionNames(&deviceExtensionNames);

    std::tie(status, logical) = physical.createDevice(deviceCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create logical device." << std::endl;
        return false;
    }

    queue.queue = logical.getQueue(queue.familyIndex, 0);
    return true;
}

} // vkc namespace