/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <Structs.hpp>
#include <Device.hpp>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

namespace vkc
{

class Window
{
public:
    struct SurfaceData
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> imageFormats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    Window(Device& device);

    Window(Device& device, char const* title, uint32_t width, uint32_t height);

    ~Window();

    Window(Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&) = delete;
    Window& operator=(Window&&) = delete;

    bool Init();

    bool IsValid() const;

    void SwapBuffers();

    void Shutdown();

    std::tuple<bool, SurfaceData> GetSurfaceData();

    Device& device;
    vk::Result status = vk::Result::eErrorInitializationFailed;
    std::string const title{ "Compositor" };
    uint32_t const width = 640;
    uint32_t const height = 640;
    vk::SurfaceFormatKHR const surfaceFormat{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    std::array<Image, 2> swapchainImages;
    uint32_t const swapchainImageCount = 2;

private:
    GLFWwindow* m_pWindow = nullptr;

    bool CreateSurface();

    bool CreateSwapchain();

    bool CreateSwapchainImageViews();
};

} // vkc namespace