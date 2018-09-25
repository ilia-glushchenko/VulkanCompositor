/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Window.hpp>
#include <iostream>

namespace vkc
{

Window::Window(Device & device)
    : device(device)
{
}

Window::Window(Device & device, char const * title, uint32_t width, uint32_t height)
    : device(device)
    , title(title)
    , width(width)
    , height(height)
{
}

Window::~Window()
{
    Shutdown();
}

bool Window::Init()
{
    return CreateSurface() && CreateSwapchain() && CreateSwapchainImageViews();
}

bool Window::IsValid() const
{
    return !glfwWindowShouldClose(m_pWindow);
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_pWindow);
}

void Window::Shutdown()
{
    for (auto image : swapchainImages)
    {
        if (image.view)
            device.logical.destroyImageView(image.view);
    }

    if (swapchain)
        device.logical.destroySwapchainKHR(swapchain);

    glfwDestroyWindow(m_pWindow);
}

std::tuple<bool, Window::SurfaceData> Window::GetSurfaceData()
{
    SurfaceData data;
    std::tie(status, data.capabilities) = device.physical.getSurfaceCapabilitiesKHR(surface);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to get surface capabilities." << std::endl;
        return { false, data };
    }

    std::tie(status, data.imageFormats) = device.physical.getSurfaceFormatsKHR(surface);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to get surface image formats." << std::endl;
        return { false, data };
    }

    std::tie(status, data.presentModes) = device.physical.getSurfacePresentModesKHR(surface);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to get surface present modes." << std::endl;
        return { false, data };
    }

    return { true, data };
}

bool Window::CreateSurface()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_pWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    VkSurfaceKHR surfaceOld;
    VkResult error = glfwCreateWindowSurface(device.instance, m_pWindow, nullptr, &surfaceOld);
    if (error)
    {
        std::cerr << "Failed to create surface." << std::endl;
        return false;
    }

    surface = surfaceOld;
    vk::Bool32 suport;
    std::tie(status, suport) = device.physical.getSurfaceSupportKHR(device.queue.familyIndex, surface);
    if (status != vk::Result::eSuccess || !suport)
    {
        std::cerr << "Surface does not support presentation for this device." << std::endl;
        return false;
    }

    return true;
}

bool Window::CreateSwapchain()
{
    bool result = true;
    SurfaceData surfaceData;
    std::tie(result, surfaceData) = GetSurfaceData();

    if (!result)
    {
        std::cerr << "Failed to fetch surface data." << std::endl;
        status = vk::Result::eErrorInitializationFailed;
        return false;
    }

    if (!(surfaceData.capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque))
    {
        std::cerr << "Surface does not support opaque composite alpha." << std::endl;
        status = vk::Result::eErrorInitializationFailed;
        return false;
    }

    if (surfaceData.capabilities.maxImageCount < swapchainImageCount)
    {
        std::cerr << "Surface does not support double buffering." << std::endl;
        status = vk::Result::eErrorInitializationFailed;
        return false;
    }

    if (std::find(surfaceData.imageFormats.begin(), surfaceData.imageFormats.end(), surfaceFormat) == surfaceData.imageFormats.end())
    {
        std::cerr << "Device does not support eSrgbNonlinear eB8G8R8A8Unorm image format." << std::endl;
        status = vk::Result::eErrorInitializationFailed;
        return false;
    }

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    if (std::find(surfaceData.presentModes.begin(), surfaceData.presentModes.end(), vk::PresentModeKHR::eFifo) == surfaceData.presentModes.end())
    {
        std::cerr << "Device does not support fifo presetn mode." << std::endl;
        status = vk::Result::eErrorInitializationFailed;
        return false;
    }

    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.setSurface(surface);
    swapchainCreateInfo.setMinImageCount(swapchainImageCount);
    swapchainCreateInfo.setImageFormat(surfaceFormat.format);
    swapchainCreateInfo.setImageColorSpace(surfaceFormat.colorSpace);
    swapchainCreateInfo.setImageExtent(vk::Extent2D(width, height));
    swapchainCreateInfo.setImageArrayLayers(1);
    swapchainCreateInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    swapchainCreateInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    swapchainCreateInfo.setQueueFamilyIndexCount(1);
    swapchainCreateInfo.setPQueueFamilyIndices(&device.queue.familyIndex);
    swapchainCreateInfo.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity);
    swapchainCreateInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    swapchainCreateInfo.setPresentMode(presentMode);
    swapchainCreateInfo.setClipped(true);

    std::tie(status, swapchain) = device.logical.createSwapchainKHR(swapchainCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create swapchain" << std::endl;
        return false;
    }

    return true;
}

bool Window::CreateSwapchainImageViews()
{
    std::vector<vk::Image> images;
    std::tie(status, images) = device.logical.getSwapchainImagesKHR(swapchain);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to get swapchain images." << std::endl;
        return false;
    }

    for (size_t i = 0; i < images.size(); ++i)
    {
        swapchainImages[i].image = images[i];

        vk::ImageSubresourceRange range;
        range.setBaseArrayLayer(0);
        range.setBaseMipLevel(0);
        range.setLayerCount(1);
        range.setLevelCount(1);
        range.setAspectMask(vk::ImageAspectFlagBits::eColor);

        vk::ImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.setFormat(surfaceFormat.format);
        imageViewCreateInfo.setImage(images[i]);
        imageViewCreateInfo.setViewType(vk::ImageViewType::e2D);
        imageViewCreateInfo.setSubresourceRange(range);
        imageViewCreateInfo.setComponents({
            vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA
            });

        std::tie(status, swapchainImages[i].view) = device.logical.createImageView(imageViewCreateInfo);
        if (status != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create swapchain image view." << std::endl;
            return false;
        }
    }

    return true;
}

} // vkc namespace