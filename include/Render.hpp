/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <Structs.hpp>
#include <Device.hpp>
#include <Window.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>

namespace vkc
{

class Render
{
public:
    Render(Device& device, Window& window);

    ~Render();

    bool Init();

    void Shutdown();

    bool Frame();

    vk::Result status = vk::Result::eErrorInitializationFailed;

private:
    Device& m_device;
    Window& m_window;
    Buffer m_vertexBuffer;
    Shader m_vertexShader;
    Shader m_fragmentShader;
    vk::RenderPass m_renderPass;
    vk::Framebuffer m_framebuffers[2];
    uint32_t const m_framebufferCount = 2;
    uint32_t m_currentFrameBuffer = 0;
    uint32_t const m_attachmentCount = 1;
    vk::ClearValue m_colorClearValue{ vk::ClearColorValue(std::array<float, 4>{ 0.0f, 1.0f, 0.0f, 1.0f }) };
    vk::PipelineLayout m_pipelineLayout;
    vk::PipelineCache m_pipelineCache;
    vk::Pipeline m_pipeline;
    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    vk::Semaphore m_renderDoneSemaphore;
    vk::Semaphore m_imageAvailableSemaphore;
    vk::Fence m_presentFence;

    bool CreateSemaphores();

    bool CreateShaders();

    bool CreateVertexBuffer();

    bool CreateRenderPass();

    bool CreateFramebuffers();

    bool CreatePipeline();

    bool CreateCommandBuffers();
};

} // vkc namespace