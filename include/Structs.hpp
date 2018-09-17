#ifndef VULKAN_COMPOSITOR_STRUCTS_HPP
#define VULKAN_COMPOSITOR_STRUCTS_HPP

#include <Helpers.hpp>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

struct MVP
{
    float model[16];
    float view[16];
    float projection[16];
};

struct CommandData
{
    uint32_t familyIndex;
    vk::Queue queue;
    vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> commandBuffers;
};

struct DeviceData
{
    bool InitializeInstace();

    bool InitializePhysicalDevice();

    bool InitializeLogicalDevice(VkSurfaceKHR& surface);

    vk::Instance instance;
    vk::PhysicalDevice physical;
    vk::Device logical;
    CommandData graphicsQueueData;
    CommandData presentQueueData;
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
    vk::Semaphore swapchainImageAcquireSemaphore;
    vk::Fence swapchainImageAcquireFence;
};

struct Descriptors
{
    std::vector<vk::DescriptorSetLayout> layouts;
    std::vector<vk::DescriptorSet> sets;
    std::vector<uint32_t> descriptorsCount;
    vk::DescriptorPool pool;
};

struct Image
{
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageView view;
};

class Buffer
{
public:
    bool CreateBuffer(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data);

    bool CreateBuffer(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data, vk::DescriptorSetLayoutBinding setLayoutBinding);

    vk::Buffer buffer;
    vk::DeviceMemory memory;
    vk::BufferView view;
    Descriptors descriptors;

private:
    bool CreateBufferObject(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data);

    bool CreateDescriptorSetLayout(DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding);

    bool CreateDescriptorPool(DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding);

    bool CreateDescriptorSet(DeviceData& deviceData);

    void UpdateDescriptorSet(DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding, vk::DeviceSize range);
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

struct Shader
{
    bool CreateShader(DeviceData& deviceData, char const* name, char const* filename, vk::ShaderStageFlagBits stage);

    vk::ShaderModule shaderModule;
    vk::PipelineShaderStageCreateInfo shaderStageCreateInfo;
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
    vk::PipelineBindPoint bindPoint;
};

struct RenderData
{
    vk::SampleCountFlagBits const m_depthBufferSamples = vk::SampleCountFlagBits::e1;
    vk::Format const m_depthBufferImageFormat = vk::Format::eD16Unorm;
    vk::ClearValue const colorClearValue{ vk::ClearColorValue(std::array<float, 4>{0.2f, 0.7f, 0.3f, 1.0f}) };
    vk::ClearValue const depthStencilClearValue{ vk::ClearDepthStencilValue(1.0f, 0) };
    std::vector<vk::ClearValue> const clearValues{ colorClearValue, depthStencilClearValue };

    Shader vertexShader;
    Shader fragmentShader;

    Image depthBufferImage;
    Buffer uniformBuffer;
    VertexBuffer vertexBuffer;

    vk::Viewport viewport;
    vk::Rect2D scissors;
    vk::RenderPass renderPass;
    vk::Fence drawFence;
    MVP mvp = {};
};

#endif