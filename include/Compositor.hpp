/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <Structs.hpp>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <vector>

class Compositor
{
public:
    ~Compositor();

    bool Initialize();

    bool IsValid() const;

    void RenderFrame();

private:
    DeviceData m_deviceData;
    WindowData m_windowData;
    PipelineData m_pipelineData;
    RenderData m_renderData;
    bool m_initialized = false;


    bool InitializeWindowSurface();

    bool InitializeSwapchain();

    bool InitializeCommandPools();

    bool InititlizeCommandBuffers();
    

    bool IntitilizeDepthBufferImage();

    bool InitializeUniformBuffer();

    bool InitializeVeretexBuffer();

    bool InitializeShaders();


    bool InitializeSemaphors();

    bool InitializeRenderPass();

    bool InitializeFramebuffers();
   
    bool InitializePipeline();

    bool UpdateCurrentFramebuffer();
};
