/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <vector>

struct MVP
{
    float model[16];
    float view[16];
    float projection[16];
};

class Compositor
{
public:
    ~Compositor();

    bool Initialize();

    bool IsValid() const;

    void RenderFrame();

private:
    struct DeviceData
    {
        vk::Instance instance;
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
    };

    struct WindowData
    {
        GLFWwindow* pWindow = nullptr;
        VkSurfaceKHR surface;
        uint8_t const imageCount = 2;
        vk::SurfaceFormatKHR surfaceFormat;
        vk::SwapchainKHR swapchain;
        std::vector<vk::Image> swapchainImages;
        std::vector<vk::ImageView> swapchainImageViews;
        std::vector<vk::Framebuffer> framebuffers;
        uint32_t currentBuffer = 0;
    };

    struct CommandData
    {
        uint32_t familyIndex;
        vk::Queue queue;
        vk::CommandPool commandPool;
        std::vector<vk::CommandBuffer> commandBuffers;
    };

    struct Descriptors
    {
        std::vector<vk::DescriptorSetLayout> layouts;
        std::vector<vk::DescriptorSet> sets;
        vk::DescriptorPool pool;
    };

    struct Image
    {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::ImageView view;
    };

    struct Buffer
    {
        vk::Buffer buffer;
        vk::DeviceMemory memory;
        vk::BufferView view;
    };

    struct VertexBuffer : Buffer
    {
        struct Vertex {
            float posX, posY, posZ, posW; 
            float normX, normY, normZ, normW;
            float r, g, b, a;             
        };

        uint8_t const attributeDescriptionSize = 3;
        static vk::VertexInputAttributeDescription const s_attributeDescriptions[3];
        static vk::VertexInputBindingDescription const s_bindingDescriptions;
        static float const s_vertices[6 * 12];
    };
   
    struct PipelineData
    {
        uint8_t const shaderStagesSize = 2;
        vk::PipelineShaderStageCreateInfo shaderStagesCreateInfo[2];
        vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
        vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
        vk::PipelineViewportStateCreateInfo viewportStateCreateInfo;
        vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
        vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
        vk::PipelineColorBlendAttachmentState alphaBlendAttachmentState;
        vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
        vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
        vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
        vk::PipelineLayoutCreateInfo layoutCreateInfo;
        vk::PipelineLayout layout;
        vk::PipelineCache cache;
        vk::Pipeline pipeline;
    };

    struct RenderData
    {
        CommandData transferQueueData;
        CommandData graphicsQueueData;
        CommandData presentQueueData;
        Descriptors descriptors;

        vk::Format const m_depthBufferImageFormat = vk::Format::eD16Unorm;
        vk::SampleCountFlagBits const m_depthBufferSamples = vk::SampleCountFlagBits::e1;
        Image depthBufferImage;
        Buffer uniformBuffer;
        VertexBuffer vertexBuffer;

        vk::Viewport viewport;
        vk::Rect2D scissors;
        vk::RenderPass renderPass;
    };

    DeviceData m_deviceData;
    WindowData m_windowData;
    PipelineData m_pipelineData;
    RenderData m_renderData;
    bool m_initialized = false;


    bool InitializeInstace();

    bool InitializePhysicalDevice();

    bool InitializeWindowSurface();

    bool InitializeLogicalDevice();

    bool InitializeCommandPools();

    bool InititlizeCommandBuffers();

    bool InitializeSwapchain();


    bool IntitilizeDepthBufferImage();

    bool InitializeUniformBuffer();

    bool InitializeVeretexBuffer();


    bool InitializeDescriptorSetLayout();

    bool InitializeDescriptorPool();

    bool InitializeDescriptorSets();

    bool InitializeShaders();


    bool InitializeSemaphors();

    bool InitializeRenderPass();

    bool InitializeFramebuffers();
   
    bool InitializePipeline();


    bool LoadShader(char const* filename, char*& buffer, long& size);
};
