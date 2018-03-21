/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>
#include <iostream>

namespace
{
std::tuple<bool, uint32_t> FindMemoryTypeIndex(vk::PhysicalDevice device, vk::MemoryPropertyFlags flags)
{
    vk::PhysicalDeviceMemoryProperties const memoryProperties = device.getMemoryProperties();
    uint32_t memoryTypeIndex = 0;
    bool memoryTypeFound = false;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if (static_cast<bool>(memoryProperties.memoryTypes[i].propertyFlags == flags))
        {
            memoryTypeIndex = i;
            memoryTypeFound = true;
            break;
        }
    }

    return std::make_tuple(memoryTypeFound, memoryTypeIndex);
}
}

Compositor::~Compositor()
{
    //Destroy uniform buffer
    m_deviceData.device.destroyBufferView(m_renderData.uniformBuffer.view);
    m_deviceData.device.freeMemory(m_renderData.uniformBuffer.memory);
    m_deviceData.device.destroyBuffer(m_renderData.uniformBuffer.buffer);

    //Destroy depth buffer
    m_deviceData.device.destroyImageView(m_renderData.depthBufferImage.view);
    m_deviceData.device.freeMemory(m_renderData.depthBufferImage.memory);
    m_deviceData.device.destroyImage(m_renderData.depthBufferImage.image);

    //Destroy instance device and window
    m_deviceData.device.destroy();
    glfwDestroyWindow(m_windowData.pWindow);
    m_deviceData.instance.destroy();
}

bool Compositor::Initialize()
{
    m_initialized = InitializeInstace();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeWindowSurface();
    assert(m_initialized);
    m_initialized = m_initialized && InitializePhysicalDevice();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeLogicalDevice();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeCommandPools();
    assert(m_initialized);
    m_initialized = m_initialized && InititlizeCommandBuffers();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeSwapchain();
    assert(m_initialized);

    m_initialized = m_initialized && IntitilizeDepthBufferImage();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeUniformBuffer();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeVeretexBuffer();
    assert(m_initialized);

    m_initialized = m_initialized && InitializeDescriptorSetLayout();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeCommandPools();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeDescriptorPool();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeDescriptorSets();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeShaders();
    assert(m_initialized);

    m_initialized = m_initialized && InitializeSemaphors();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeRenderPass();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeFramebuffers();
    assert(m_initialized);
    m_initialized = m_initialized && InitializePipeline();
    assert(m_initialized);

    return m_initialized;
}

bool Compositor::IsValid() const
{
    return m_initialized && !glfwWindowShouldClose(m_windowData.pWindow);
}

void Compositor::RenderFrame()
{    
    vk::Semaphore swapchainImageAcquireSemaphore;
    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    vk::Result result;
    std::tie(result, swapchainImageAcquireSemaphore) = m_deviceData.device.createSemaphore(semaphoreCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create semaphore." << std::endl;
    }

    vk::Fence swapchainImageAcquireFence;
    vk::FenceCreateInfo fenceCreateInfo;
    std::tie(result, swapchainImageAcquireFence) = m_deviceData.device.createFence(fenceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create fence." << std::endl;
    }

    vk::Fence drawFence;
    std::tie(result, drawFence) = m_deviceData.device.createFence(fenceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create draw fence." << std::endl;
    }

    //Write command buffer
    {
        vk::CommandBufferBeginInfo cmdBeginInfo;
        m_renderData.graphicsQueueData.commandBuffers.back().begin(cmdBeginInfo);

        vk::Result result;
        std::tie(result, m_windowData.currentBuffer) = m_deviceData.device.acquireNextImageKHR(
            m_windowData.swapchain, UINT64_MAX, swapchainImageAcquireSemaphore, swapchainImageAcquireFence);
        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to acquire next image index." << std::endl;
            return;
        }

        vk::SurfaceCapabilitiesKHR surfaceCapabilities;
        m_deviceData.physicalDevice.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = m_renderData.renderPass;
        renderPassBeginInfo.framebuffer = m_windowData.framebuffers[m_windowData.currentBuffer];
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = surfaceCapabilities.currentExtent.width;
        renderPassBeginInfo.renderArea.extent.height = surfaceCapabilities.currentExtent.height;
        renderPassBeginInfo.clearValueCount = 2;
        static vk::ClearValue const colorClearValue{ vk::ClearColorValue(std::array<float, 4>{1.0f, 0.0f, 1.0f, 1.0f}) };
        static vk::ClearValue const depthStencilClearValue{ vk::ClearDepthStencilValue(1.0f, 0) };
        vk::ClearValue const clearValues[2] = { colorClearValue, depthStencilClearValue };
        renderPassBeginInfo.pClearValues = clearValues;
  
        m_renderData.graphicsQueueData.commandBuffers.back().bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelineData.pipeline);
        m_renderData.graphicsQueueData.commandBuffers.back().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, m_pipelineData.layout, 0, 1, m_renderData.descriptors.sets.data(), 0, nullptr);
        m_renderData.graphicsQueueData.commandBuffers.back().beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        const vk::DeviceSize offsets[1] = { 0 };
        m_renderData.graphicsQueueData.commandBuffers.back().bindVertexBuffers(0, 1, &m_renderData.vertexBuffer.buffer, offsets);
        m_renderData.graphicsQueueData.commandBuffers.back().setViewport(0, 1, &m_renderData.viewport);
        m_renderData.graphicsQueueData.commandBuffers.back().setScissor(0, 1, &m_renderData.scissors);
        m_renderData.graphicsQueueData.commandBuffers.back().draw(6, 1, 0, 0);
        m_renderData.graphicsQueueData.commandBuffers.back().endRenderPass();

        m_renderData.graphicsQueueData.commandBuffers.back().end();
    }

    //Submit graphics queue
    {
        const vk::CommandBuffer cmd_bufs[] = { m_renderData.graphicsQueueData.commandBuffers.back() };
        vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eBottomOfPipe;
        vk::SubmitInfo submit_info[1] = {};
        submit_info[0].waitSemaphoreCount = 1;
        submit_info[0].pWaitSemaphores = &swapchainImageAcquireSemaphore;
        submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
        submit_info[0].commandBufferCount = 1;
        submit_info[0].pCommandBuffers = cmd_bufs;
        submit_info[0].signalSemaphoreCount = 0;
        submit_info[0].pSignalSemaphores = nullptr;
        vk::Result result = m_renderData.graphicsQueueData.queue.submit(1, submit_info, drawFence);
        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to submit graphics queue for execution." << std::endl;
            return;
        }
    }

    //Present image to present queue
    {
        vk::Result result;
        do {
            result = m_deviceData.device.waitForFences(1, &drawFence, true, static_cast<uint32_t>(1e3));
        } while (result == vk::Result::eTimeout);

        vk::PresentInfoKHR presentInfo;
        presentInfo.setSwapchainCount(1);
        presentInfo.setPSwapchains(&m_windowData.swapchain);
        presentInfo.setPImageIndices(&m_windowData.currentBuffer);

        m_renderData.presentQueueData.queue.presentKHR(presentInfo);
    }

    glfwSwapBuffers(m_windowData.pWindow);
    glfwPollEvents();
}

bool Compositor::InitializeInstace()
{
    glfwInit();
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<char const*> enabledInstanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    std::vector<char const*> enabledInstanceLayers;
#ifndef NDEBUG
    enabledInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
    enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    static vk::ApplicationInfo applicationInfo;
    applicationInfo.setPApplicationName("Vulkan Compositor");
    applicationInfo.setApplicationVersion(1);
    applicationInfo.setApiVersion(VK_API_VERSION_1_1);

    vk::InstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.setPApplicationInfo(&applicationInfo);
    instanceCreateInfo.setEnabledExtensionCount(static_cast<uint32_t>(enabledInstanceExtensions.size()));
    instanceCreateInfo.setPpEnabledExtensionNames(&enabledInstanceExtensions[0]);
#ifndef NDEBUG
    instanceCreateInfo.setEnabledLayerCount(static_cast<uint32_t>(enabledInstanceLayers.size()));
    instanceCreateInfo.setPpEnabledLayerNames(&enabledInstanceLayers[0]);
#endif

    vk::Result result;
    std::tie(result, m_deviceData.instance) = vk::createInstance(instanceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Vulkan instance creation failed." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializePhysicalDevice()
{
    vk::Result result;
    std::vector<vk::PhysicalDevice> physicalDevices;
    std::tie(result, physicalDevices) = m_deviceData.instance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        std::cerr << "System has no Vulkan capable physical devices." << std::endl;
        return false;
    }

    m_deviceData.physicalDevice = physicalDevices.front();
    std::array<vk::PhysicalDeviceType, 2> physicalDeviceTypes = {{
        vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu
    }};
    for (auto type : physicalDeviceTypes)
    {
        auto const physicalDevice = std::find_if(physicalDevices.begin(), physicalDevices.end(),
            [type](auto& pd) ->bool {
                return pd.getProperties().deviceType == type;
        });
        if (physicalDevice != physicalDevices.end())
        {
            m_deviceData.physicalDevice = *physicalDevice;
            break;
        }
    }

    return true;
}

bool Compositor::InitializeWindowSurface()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_windowData.pWindow = glfwCreateWindow(640, 480, "Compositor", nullptr, nullptr);
    VkResult const result = glfwCreateWindowSurface(
        m_deviceData.instance, m_windowData.pWindow, nullptr, &m_windowData.surface
    );
    if (result != VK_SUCCESS)
    {
        std::cerr << "Window surface creation failed." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeLogicalDevice()
{
    //Get transfer and graphics queue families
    auto queueFamilyProperties = m_deviceData.physicalDevice.getQueueFamilyProperties();
    auto graphicsQueueFamilyProperties = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
        [](auto& family) -> bool {
            auto const mask = vk::QueueFlagBits::eGraphics & family.queueFlags;
            return static_cast<bool>(mask);
    });
    if (graphicsQueueFamilyProperties == queueFamilyProperties.end())
    {
        std::cerr << "Physical devices does not provide graphical queues." << std::endl;
        return false;
    }
    auto transferQueueFamilyProperties = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
        [](auto& family) -> bool {
            return static_cast<bool>(vk::QueueFlagBits::eTransfer & family.queueFlags)
                && !static_cast<bool>(vk::QueueFlagBits::eGraphics & family.queueFlags);
    });
    //If there is no dedicated transfer queue, take any other that supports transfer
    if (transferQueueFamilyProperties == queueFamilyProperties.end())
    {
        transferQueueFamilyProperties = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
            [graphicsQueueFamilyProperties](auto& family) -> bool {
                return static_cast<bool>(vk::QueueFlagBits::eTransfer & family.queueFlags)
                    && &family != &*graphicsQueueFamilyProperties;
        });

        if (transferQueueFamilyProperties == queueFamilyProperties.end())
        {
            transferQueueFamilyProperties = graphicsQueueFamilyProperties;
            if (!static_cast<bool>(transferQueueFamilyProperties->queueFlags & vk::QueueFlagBits::eTransfer))
            {
                std::cerr << "Physical device does not provide transfer queues." << std::endl;
                return false;
            }
        }
    }
    //Get windows surface presentation capable queue
    auto presentationQueueFamilyProperties = queueFamilyProperties.end();
    for (uint32_t index = 0; index < queueFamilyProperties.size(); ++index)
    {
        vk::Result result;
        vk::Bool32 support;
        std::tie(result, support) = m_deviceData.physicalDevice.getSurfaceSupportKHR(index, m_windowData.surface);        
        if (support)
        {
            presentationQueueFamilyProperties = std::next(queueFamilyProperties.begin(), index);
            break;
        }
    }
    if (presentationQueueFamilyProperties == queueFamilyProperties.end())
    {
        std::cerr << "Physical device does not provide presentation capable queues." << std::endl;
        return false;
    }
    
    //Create logical device
    static float const queuePriorities = { 1.0 };
    vk::DeviceQueueCreateInfo transferQueueCreateInfo;
    transferQueueCreateInfo.setQueueFamilyIndex(
        static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), transferQueueFamilyProperties)));
    transferQueueCreateInfo.setQueueCount(1);
    transferQueueCreateInfo.setPQueuePriorities(&queuePriorities);
    m_renderData.transferQueueData.familyIndex = transferQueueCreateInfo.queueFamilyIndex;

    vk::DeviceQueueCreateInfo graphicsQueueCreateInfo;
    graphicsQueueCreateInfo.setQueueFamilyIndex(
        static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperties)));
    graphicsQueueCreateInfo.setQueueCount(1);
    graphicsQueueCreateInfo.setPQueuePriorities(&queuePriorities);
    m_renderData.graphicsQueueData.familyIndex = graphicsQueueCreateInfo.queueFamilyIndex;

    vk::DeviceQueueCreateInfo presentationQueueCreateInfo;
    presentationQueueCreateInfo.setQueueFamilyIndex(
        static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), presentationQueueFamilyProperties)));
    presentationQueueCreateInfo.setQueueCount(1);
    presentationQueueCreateInfo.setPQueuePriorities(&queuePriorities);
    m_renderData.presentQueueData.familyIndex = presentationQueueCreateInfo.queueFamilyIndex;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos = {{
        transferQueueCreateInfo, graphicsQueueCreateInfo, presentationQueueCreateInfo 
    }};
    std::sort(queueCreateInfos.begin(), queueCreateInfos.end(), [](auto a, auto b) -> bool{
        return a.queueFamilyIndex < b.queueFamilyIndex;
    });
    queueCreateInfos.erase(std::unique(queueCreateInfos.begin(), queueCreateInfos.end(), 
        [](auto a, auto b) -> bool { 
            return a.queueFamilyIndex == b.queueFamilyIndex;  
        }), queueCreateInfos.end());

    std::vector<char const*> deviceExtensionNames = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.setPQueueCreateInfos(queueCreateInfos.data());
    deviceCreateInfo.setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()));
    deviceCreateInfo.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensionNames.size()));
    deviceCreateInfo.setPpEnabledExtensionNames(deviceExtensionNames.data());
    vk::Result result;
    std::tie(result, m_deviceData.device) = m_deviceData.physicalDevice.createDevice(deviceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create vulkan device." << std::endl;
        return false;
    }

    //Get queue handles
    m_renderData.graphicsQueueData.queue = m_deviceData.device.getQueue(graphicsQueueCreateInfo.queueFamilyIndex, 0);
    m_renderData.transferQueueData.queue = m_deviceData.device.getQueue(transferQueueCreateInfo.queueFamilyIndex, 0);
    m_renderData.presentQueueData.queue = m_deviceData.device.getQueue(presentationQueueCreateInfo.queueFamilyIndex, 0);

    return true;
}

bool Compositor::InitializeCommandPools()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.setQueueFamilyIndex(m_renderData.transferQueueData.familyIndex);
    vk::Result result;
    std::tie(result, m_renderData.transferQueueData.commandPool) = m_deviceData.device.createCommandPool(commandPoolCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create transfer command pool." << std::endl;
        return false;
    }

    commandPoolCreateInfo.setQueueFamilyIndex(m_renderData.presentQueueData.familyIndex);
    std::tie(result, m_renderData.presentQueueData.commandPool) = m_deviceData.device.createCommandPool(commandPoolCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create presentation command pool." << std::endl;
        return false;
    }

    commandPoolCreateInfo.setQueueFamilyIndex(m_renderData.graphicsQueueData.familyIndex);
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    std::tie(result, m_renderData.graphicsQueueData.commandPool) = m_deviceData.device.createCommandPool(commandPoolCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create graphics command pool." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InititlizeCommandBuffers()
{
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.setCommandPool(m_renderData.transferQueueData.commandPool);
    commandBufferAllocateInfo.setCommandBufferCount(1);
    commandBufferAllocateInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    vk::Result result;
    std::tie(result, m_renderData.transferQueueData.commandBuffers) = m_deviceData.device.allocateCommandBuffers(commandBufferAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate transfer queue command buffers." << std::endl;
        return false;
    }

    commandBufferAllocateInfo.setCommandPool(m_renderData.graphicsQueueData.commandPool);
    std::tie(result, m_renderData.graphicsQueueData.commandBuffers) = m_deviceData.device.allocateCommandBuffers(commandBufferAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate graphics queue command buffers." << std::endl;
        return false;
    }

    commandBufferAllocateInfo.setCommandPool(m_renderData.presentQueueData.commandPool);
    std::tie(result, m_renderData.presentQueueData.commandBuffers) = m_deviceData.device.allocateCommandBuffers(commandBufferAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate present queue command buffers." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeSwapchain()
{
    //Query surface for capabilities
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physicalDevice.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
    if (surfaceCapabilities.minImageCount < m_windowData.imageCount)
    {
        std::cerr << "Physical device does not support " << m_windowData.imageCount << " buffering." << std::endl;
        return false;
    }
    if (!static_cast<bool>(surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity))
    {
        std::cerr << "Physical device does not support identity surface transformation." << std::endl;
        return false;
    }
    auto compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
    if (!static_cast<bool>(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied))
    {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
        if (!static_cast<bool>(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied))
        {
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            if (!static_cast<bool>(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque))
            {
                std::cerr << "Physical device does not support post alpha multiplication." << std::endl;
                return false;
            }
        }
    }
    
    //Acquire surface formates
    vk::Result result;
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    std::tie(result, surfaceFormats) = m_deviceData.physicalDevice.getSurfaceFormatsKHR(m_windowData.surface);
    if (result != vk::Result::eSuccess || surfaceFormats.empty())
    {
        std::cerr << "Failed to get physical device's surface formats." << std::endl;
        return false;
    }
    m_windowData.surfaceFormat = surfaceFormats.front();

    //Find surface with propper presentation modes
    std::vector<vk::PresentModeKHR> surfacePresentModes;
    std::tie(result, surfacePresentModes) = m_deviceData.physicalDevice.getSurfacePresentModesKHR(m_windowData.surface);
    if (result != vk::Result::eSuccess || surfacePresentModes.empty())
    {
        std::cerr << "Failed to get physical devices's surface presentation modes." << std::endl;
        return false;
    }
    auto presentMode = vk::PresentModeKHR::eImmediate;
    if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), vk::PresentModeKHR::eImmediate) == surfacePresentModes.end())
    {
        presentMode = vk::PresentModeKHR::eMailbox;
        if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), vk::PresentModeKHR::eMailbox) == surfacePresentModes.end())
        {
            std::cerr << "Failed to get surface with support of the immediate presentation mode." << std::endl;
            return false;
        }
    }

    //Clear repeated queue family indices
    std::vector<uint32_t> queueFamilyIndices = {{
        m_renderData.transferQueueData.familyIndex, m_renderData.graphicsQueueData.familyIndex, m_renderData.presentQueueData.familyIndex
    }};
    std::sort(queueFamilyIndices.begin(), queueFamilyIndices.end());
    queueFamilyIndices.erase(std::unique(queueFamilyIndices.begin(), queueFamilyIndices.end()), queueFamilyIndices.end());
    
    //Create swapchain
    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.setSurface(m_windowData.surface);
    swapchainCreateInfo.setMinImageCount(m_windowData.imageCount);
    swapchainCreateInfo.setImageFormat(m_windowData.surfaceFormat.format);
    swapchainCreateInfo.setImageColorSpace(m_windowData.surfaceFormat.colorSpace);
    swapchainCreateInfo.setImageExtent(surfaceCapabilities.currentExtent);
    swapchainCreateInfo.setImageArrayLayers(1);
    swapchainCreateInfo.setImageUsage(surfaceCapabilities.supportedUsageFlags);
    swapchainCreateInfo.setImageSharingMode(queueFamilyIndices.size() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent);
    swapchainCreateInfo.setQueueFamilyIndexCount(static_cast<uint32_t>(queueFamilyIndices.size()));
    swapchainCreateInfo.setPQueueFamilyIndices(queueFamilyIndices.data());
    swapchainCreateInfo.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity);
    swapchainCreateInfo.setCompositeAlpha(compositeAlpha);
    swapchainCreateInfo.setPresentMode(presentMode);
    swapchainCreateInfo.setClipped(VK_TRUE);
    std::tie(result, m_windowData.swapchain) = m_deviceData.device.createSwapchainKHR(swapchainCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create swapchain." << std::endl;
        return false;
    }

    //Create swapchain images
    std::tie(result, m_windowData.swapchainImages) = m_deviceData.device.getSwapchainImagesKHR(m_windowData.swapchain);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to get swapchain images." << std::endl;
        return false;
    }

    //Create swapchain views
    m_windowData.swapchainImageViews.resize(m_windowData.swapchainImages.size());
    for (size_t i = 0; i < m_windowData.swapchainImageViews.size(); ++i)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.setImage(m_windowData.swapchainImages[i]);
        imageViewCreateInfo.setViewType(vk::ImageViewType::e2D);
        imageViewCreateInfo.setFormat(m_windowData.surfaceFormat.format);
        imageViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
        imageViewCreateInfo.subresourceRange.setBaseMipLevel(0);
        imageViewCreateInfo.subresourceRange.setLevelCount(1);
        imageViewCreateInfo.subresourceRange.setBaseArrayLayer(0);
        imageViewCreateInfo.subresourceRange.setLayerCount(1);

        std::tie(result, m_windowData.swapchainImageViews[i]) = m_deviceData.device.createImageView(imageViewCreateInfo);
        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create swapchain image view." << std::endl;
            return false;
        }
    }

    return true;
}

bool Compositor::IntitilizeDepthBufferImage()
{
    //Create depth buffer image
    vk::ImageCreateInfo bufferCreateInfo;
    bufferCreateInfo.imageType = vk::ImageType::e2D;
    bufferCreateInfo.format = m_renderData.m_depthBufferImageFormat;
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physicalDevice.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
    bufferCreateInfo.extent.width = surfaceCapabilities.currentExtent.width;
    bufferCreateInfo.extent.height = surfaceCapabilities.currentExtent.height;
    bufferCreateInfo.extent.depth = 1;
    bufferCreateInfo.mipLevels = 1;
    bufferCreateInfo.arrayLayers = 1;
    bufferCreateInfo.samples = m_renderData.m_depthBufferSamples;
    bufferCreateInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Result result;
    std::tie(result, m_renderData.depthBufferImage.image) = m_deviceData.device.createImage(bufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create depth buffer image." << std::endl;
        return false;
    }

    //Find propper physical device memory type heap index
    bool memoryTypeFound = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryTypeFound, memoryTypeIndex) = FindMemoryTypeIndex(
        m_deviceData.physicalDevice, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (!memoryTypeFound)
    {
        std::cerr << "Failed to find physical device memory heap for the depth buffer." << std::endl;
        return false;
    }
    
    //Allocate memory for the depth buffer image
    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements const memoryRequirements = m_deviceData.device.getImageMemoryRequirements(m_renderData.depthBufferImage.image);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    std::tie(result, m_renderData.depthBufferImage.memory) =  m_deviceData.device.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate depth buffer memory." << std::endl;
        return false;
    }

    //Bind Image to Memory
    result = m_deviceData.device.bindImageMemory(m_renderData.depthBufferImage.image, m_renderData.depthBufferImage.memory, 0);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to bind depth buffer memory to the image." << std::endl;
        return false;
    }

    //Create depth buffer image view
    vk::ImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.image = m_renderData.depthBufferImage.image;
    imageViewCreateInfo.format = vk::Format::eD16Unorm;
    imageViewCreateInfo.components.r = vk::ComponentSwizzle::eR;
    imageViewCreateInfo.components.g = vk::ComponentSwizzle::eG;
    imageViewCreateInfo.components.b = vk::ComponentSwizzle::eB;
    imageViewCreateInfo.components.a = vk::ComponentSwizzle::eA;
    imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    std::tie(result, m_renderData.depthBufferImage.view) = m_deviceData.device.createImageView(imageViewCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create depth buffer image view." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeUniformBuffer()
{
    //Create uniform buffer
    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.setSize(sizeof(MVP));
    bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    bufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
    vk::Result result;
    std::tie(result, m_renderData.uniformBuffer.buffer) = m_deviceData.device.createBuffer(bufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create uniform buffer." << std::endl;
        return false;
    }

    //Allocate uniform buffer memory
    bool memoryTypeFound = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryTypeFound, memoryTypeIndex) = FindMemoryTypeIndex(m_deviceData.physicalDevice, 
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements const memoryRequirements = m_deviceData.device.getBufferMemoryRequirements(m_renderData.uniformBuffer.buffer);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    std::tie(result, m_renderData.uniformBuffer.memory) = m_deviceData.device.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate uniform buffer memory." << std::endl;
        return false;
    }

    //Bind buffer to memory
    result = m_deviceData.device.bindBufferMemory(m_renderData.uniformBuffer.buffer, m_renderData.uniformBuffer.memory, 0);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to bind buffer to memory." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeVeretexBuffer()
{
    vk::BufferCreateInfo vertexBufferCreateInfo;
    vertexBufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);
    vertexBufferCreateInfo.setSize(sizeof(VertexBuffer::s_vertices));
    vertexBufferCreateInfo.setQueueFamilyIndexCount(0);
    vertexBufferCreateInfo.setPQueueFamilyIndices(nullptr);
    vertexBufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);

    vk::Result result;
    std::tie(result, m_renderData.vertexBuffer.buffer) = m_deviceData.device.createBuffer(vertexBufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create vertex buffer." << std::endl;
        return false;
    }

    //Allocate vertex buffer memory
    bool memoryTypeFound = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryTypeFound, memoryTypeIndex) = FindMemoryTypeIndex(m_deviceData.physicalDevice,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements const memoryRequirements = m_deviceData.device.getBufferMemoryRequirements(m_renderData.vertexBuffer.buffer);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    std::tie(result, m_renderData.vertexBuffer.memory) = m_deviceData.device.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate vertex buffer memory." << std::endl;
        return false;
    }

    //Bind buffer to memory
    result = m_deviceData.device.bindBufferMemory(m_renderData.vertexBuffer.buffer, m_renderData.vertexBuffer.memory, 0);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to bind buffer to memory." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeDescriptorSetLayout()
{
    //Create uniform buffer descriptor set binding
    vk::DescriptorSetLayoutBinding setLayoutBinding;
    setLayoutBinding.setBinding(0);
    setLayoutBinding.setDescriptorCount(1);
    setLayoutBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    setLayoutBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

    //Create uniform descriptor set layout
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setBindingCount(1);
    layoutCreateInfo.setPBindings(&setLayoutBinding);
    
    vk::Result result;
    m_renderData.descriptors.layouts.resize(1);
    std::tie(result, m_renderData.descriptors.layouts[0]) = m_deviceData.device.createDescriptorSetLayout(layoutCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create uniform buffer descriptor set layout." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeDescriptorPool()
{
    //Specify descriptor sizes
    vk::DescriptorPoolSize poolSize[1];
    poolSize[0].setType(vk::DescriptorType::eUniformBuffer);
    poolSize[0].setDescriptorCount(1);

    //Allocate pool
    vk::DescriptorPoolCreateInfo poolCreateInfo;
    poolCreateInfo.setPPoolSizes(poolSize);
    poolCreateInfo.setPoolSizeCount(1);
    poolCreateInfo.setMaxSets(1);

    vk::Result result;
    std::tie(result, m_renderData.descriptors.pool) = m_deviceData.device.createDescriptorPool(poolCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create descriptor pool." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeDescriptorSets()
{
    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.setDescriptorPool(m_renderData.descriptors.pool);
    allocateInfo.setPSetLayouts(m_renderData.descriptors.layouts.data());
    allocateInfo.setDescriptorSetCount(static_cast<uint32_t>(m_renderData.descriptors.layouts.size()));

    vk::Result result;
    std::tie(result, m_renderData.descriptors.sets) = m_deviceData.device.allocateDescriptorSets(allocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate descriptor sets." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeShaders()
{
    //Load vertex shader spir-v byte code
    char* pVertexShaderBuffer = nullptr;
    long vertexShaderBufferSize = 0;
    char const* pVertexShaderFilename = "../shaders/shader.vert.spv";
    if (!LoadShader(pVertexShaderFilename, pVertexShaderBuffer, vertexShaderBufferSize)) {
        std::cerr << "Failed to load SPIR-V vertex shader code." << std::endl;
        return false;
    }

    //Create vertex shader module
    vk::ShaderModuleCreateInfo vertexShaderModuleCreateInfo;
    vertexShaderModuleCreateInfo.setCodeSize(vertexShaderBufferSize);
    vertexShaderModuleCreateInfo.setPCode(reinterpret_cast<uint32_t*>(pVertexShaderBuffer));
    vk::Result result;
    vk::ShaderModule vertexShaderModule;
    std::tie(result, vertexShaderModule) = m_deviceData.device.createShaderModule(vertexShaderModuleCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create vertex shader module." << std::endl;
        return false;
    }
    free(pVertexShaderBuffer);

    //Fill vertex shader stage create info
    vk::PipelineShaderStageCreateInfo vertexShaderStageCreateInfo;
    vertexShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexShaderStageCreateInfo.setModule(vertexShaderModule);
    vertexShaderStageCreateInfo.setPName("main");

    //Load fragment shader spir-v byte code
    char* pFragmentShaderBuffer = nullptr;
    long fragmentShaderBufferSize = 0;
    char const* pFragmentShaderFilename = "../shaders/shader.frag.spv";
    if (!LoadShader(pFragmentShaderFilename, pFragmentShaderBuffer, fragmentShaderBufferSize))
    {
        std::cerr << "Failed to load SPIR-V fragment shader code." << std::endl;
        return false;
    }

    //Create fragment shader module
    vk::ShaderModuleCreateInfo fragmentShaderModuleCreateInfo;
    fragmentShaderModuleCreateInfo.setCodeSize(fragmentShaderBufferSize);
    fragmentShaderModuleCreateInfo.setPCode(reinterpret_cast<uint32_t*>(pFragmentShaderBuffer));
    vk::ShaderModule fragmentShaderModule;
    std::tie(result, fragmentShaderModule) = m_deviceData.device.createShaderModule(fragmentShaderModuleCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create fragment shader module." << std::endl;
        return false;
    }
    free(pFragmentShaderBuffer);

    //Fill fragment shader stage create info
    vk::PipelineShaderStageCreateInfo fragmentShaderStageCreateInfo;
    fragmentShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentShaderStageCreateInfo.setModule(fragmentShaderModule);
    fragmentShaderStageCreateInfo.setPName("main");

    //Fill vertex input state create info
    m_pipelineData.shaderStagesCreateInfo[0] = vertexShaderStageCreateInfo;
    m_pipelineData.shaderStagesCreateInfo[1] = fragmentShaderStageCreateInfo;

    return true;
}

bool Compositor::InitializeSemaphors()
{
    //vk::SemaphoreCreateInfo semaphoreCreateInfo;
    //vk::Result result;    
    //std::tie(result, m_windowData.swapchainImageAcquireSemaphore) = m_deviceData.device.createSemaphore(semaphoreCreateInfo);
    //if (result != vk::Result::eSuccess)
    //{
    //    std::cerr << "Failed to create semaphore." << std::endl;
    //    return false;
    //}

    //vk::FenceCreateInfo fenceCreateInfo;
    //std::tie(result, m_windowData.swapchainImageAcquireFence) = m_deviceData.device.createFence(fenceCreateInfo);
    //if (result != vk::Result::eSuccess)
    //{
    //    std::cerr << "Failed to create fence." << std::endl;
    //    return false;
    //}

    //std::tie(result, m_renderData.drawFence) = m_deviceData.device.createFence(fenceCreateInfo);
    //if (result != vk::Result::eSuccess)
    //{
    //    std::cerr << "Failed to create draw fence." << std::endl;
    //    return false;
    //}

    return true;
}

bool Compositor::InitializeRenderPass()
{
    //Define render pass attachments
    vk::AttachmentDescription attachments[2];
    attachments[0].setFormat(m_windowData.surfaceFormat.format);
    attachments[0].setSamples(m_renderData.m_depthBufferSamples);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[0].setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    attachments[1].setFormat(m_renderData.m_depthBufferImageFormat);
    attachments[1].setSamples(m_renderData.m_depthBufferSamples);
    attachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[1].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[1].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[1].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[1].setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    //Define Subpass
    vk::AttachmentReference colorReference;
    colorReference.setAttachment(0);
    colorReference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference depthReference;
    depthReference.setAttachment(1);
    depthReference.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(1);
    subpass.setPColorAttachments(&colorReference);
    subpass.setPDepthStencilAttachment(&depthReference);

    //Define render pass
    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachmentCount(2);
    createInfo.setPAttachments(attachments);
    createInfo.setSubpassCount(1);
    createInfo.setPSubpasses(&subpass);
    vk::Result result;
    std::tie(result, m_renderData.renderPass) = m_deviceData.device.createRenderPass(createInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create render pass." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeFramebuffers()
{
    vk::ImageView attachments[2];
    attachments[1] = m_renderData.depthBufferImage.view;

    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physicalDevice.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);

    vk::FramebufferCreateInfo frameBufferCreateInfo;
    frameBufferCreateInfo.setRenderPass(m_renderData.renderPass);
    frameBufferCreateInfo.setAttachmentCount(2);
    frameBufferCreateInfo.setPAttachments(attachments);
    frameBufferCreateInfo.setWidth(surfaceCapabilities.currentExtent.width);
    frameBufferCreateInfo.setHeight(surfaceCapabilities.currentExtent.height);
    frameBufferCreateInfo.setLayers(1);

    m_windowData.framebuffers.resize(m_windowData.swapchainImages.size());
    for (uint8_t i = 0; i < m_windowData.swapchainImageViews.size(); ++i)
    {
        attachments[0] = m_windowData.swapchainImageViews[i];
        vk::Result result;
        std::tie(result, m_windowData.framebuffers[i]) = m_deviceData.device.createFramebuffer(frameBufferCreateInfo);
        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create framebuffer object." << std::endl;
            return false;
        }
    }

    return true;
}

bool Compositor::InitializePipeline()
{
    //Vertex input state create info
    m_pipelineData.vertexInputStateCreateInfo.setVertexBindingDescriptionCount(1);
    m_pipelineData.vertexInputStateCreateInfo.setPVertexBindingDescriptions(&m_renderData.vertexBuffer.s_bindingDescriptions);
    m_pipelineData.vertexInputStateCreateInfo.setVertexAttributeDescriptionCount(m_renderData.vertexBuffer.attributeDescriptionSize);
    m_pipelineData.vertexInputStateCreateInfo.setPVertexAttributeDescriptions(m_renderData.vertexBuffer.s_attributeDescriptions);

    //Input assembly
    m_pipelineData.inputAssemblyStateCreateInfo.setPrimitiveRestartEnable(false);
    m_pipelineData.inputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);

    //Dynamic state
    vk::DynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];;
    memset(dynamicStateEnables, 0, sizeof(dynamicStateEnables));
    m_pipelineData.dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
    m_pipelineData.dynamicStateCreateInfo.dynamicStateCount = 0;
    dynamicStateEnables[m_pipelineData.dynamicStateCreateInfo.dynamicStateCount++] = vk::DynamicState::eViewport;
    dynamicStateEnables[m_pipelineData.dynamicStateCreateInfo.dynamicStateCount++] = vk::DynamicState::eScissor;

    //Viewport and scissors
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physicalDevice.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
    m_renderData.viewport.setX(0.0f);
    m_renderData.viewport.setY(0.0f);
    m_renderData.viewport.setWidth (static_cast<float>(surfaceCapabilities.currentExtent.width));
    m_renderData.viewport.setHeight(static_cast<float>(surfaceCapabilities.currentExtent.height));
    m_renderData.viewport.setMinDepth(0.0f);
    m_renderData.viewport.setMaxDepth(1.0f);

    m_renderData.scissors.setOffset({ 0, 0 });
    m_renderData.scissors.setExtent(surfaceCapabilities.currentExtent);

    m_pipelineData.viewportStateCreateInfo.setScissorCount(1);
    m_pipelineData.viewportStateCreateInfo.setPScissors(&m_renderData.scissors);
    m_pipelineData.viewportStateCreateInfo.setViewportCount(1);
    m_pipelineData.viewportStateCreateInfo.setPViewports(&m_renderData.viewport);


    //Rasterization
    m_pipelineData.rasterizationStateCreateInfo.setDepthClampEnable(false);
    m_pipelineData.rasterizationStateCreateInfo.setRasterizerDiscardEnable(false);
    m_pipelineData.rasterizationStateCreateInfo.setPolygonMode(vk::PolygonMode::eFill);
    m_pipelineData.rasterizationStateCreateInfo.setCullMode(vk::CullModeFlagBits::eBack);
    m_pipelineData.rasterizationStateCreateInfo.setFrontFace(vk::FrontFace::eClockwise);
    m_pipelineData.rasterizationStateCreateInfo.setDepthBiasEnable(false);
    m_pipelineData.rasterizationStateCreateInfo.setDepthBiasConstantFactor(0.0f);
    m_pipelineData.rasterizationStateCreateInfo.setDepthBiasClamp(0.0f);
    m_pipelineData.rasterizationStateCreateInfo.setDepthBiasSlopeFactor(0.0f);
    m_pipelineData.rasterizationStateCreateInfo.setLineWidth(1.0f);

    //Mulptisampling
    m_pipelineData.multisampleStateCreateInfo.setSampleShadingEnable(false);
    m_pipelineData.multisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    m_pipelineData.multisampleStateCreateInfo.setMinSampleShading(1.0f);
    m_pipelineData.multisampleStateCreateInfo.setPSampleMask(nullptr);
    m_pipelineData.multisampleStateCreateInfo.setAlphaToCoverageEnable(false);
    m_pipelineData.multisampleStateCreateInfo.setAlphaToOneEnable(false);

    //Color blending
    m_pipelineData.alphaBlendAttachmentState.setBlendEnable(true);
    m_pipelineData.alphaBlendAttachmentState.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
    m_pipelineData.alphaBlendAttachmentState.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
    m_pipelineData.alphaBlendAttachmentState.setColorBlendOp(vk::BlendOp::eAdd);
    m_pipelineData.alphaBlendAttachmentState.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    m_pipelineData.alphaBlendAttachmentState.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    m_pipelineData.alphaBlendAttachmentState.setAlphaBlendOp(vk::BlendOp::eAdd);

    m_pipelineData.colorBlendStateCreateInfo.setLogicOpEnable(false);
    m_pipelineData.colorBlendStateCreateInfo.setLogicOp(vk::LogicOp::eCopy);
    m_pipelineData.colorBlendStateCreateInfo.setAttachmentCount(1);
    m_pipelineData.colorBlendStateCreateInfo.setPAttachments(&m_pipelineData.alphaBlendAttachmentState);
    m_pipelineData.colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    m_pipelineData.colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    m_pipelineData.colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    m_pipelineData.colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    //Depth stencil 
    m_pipelineData.depthStencilStateCreateInfo.setDepthTestEnable(true);
    m_pipelineData.depthStencilStateCreateInfo.setDepthWriteEnable(true);
    m_pipelineData.depthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    m_pipelineData.depthStencilStateCreateInfo.setDepthBoundsTestEnable(false);
    m_pipelineData.depthStencilStateCreateInfo.setMinDepthBounds(0.0f);
    m_pipelineData.depthStencilStateCreateInfo.setMaxDepthBounds(0.0f);
    m_pipelineData.depthStencilStateCreateInfo.setStencilTestEnable(false);
    vk::StencilOpState back;
    back.setFailOp(vk::StencilOp::eKeep);
    back.setPassOp(vk::StencilOp::eKeep);
    back.setCompareOp(vk::CompareOp::eAlways);
    back.setCompareMask(0);
    back.setReference(0);
    back.setDepthFailOp(vk::StencilOp::eKeep);
    back.setWriteMask(0);
    m_pipelineData.depthStencilStateCreateInfo.setBack(back);
    m_pipelineData.depthStencilStateCreateInfo.setFront(back);

    //Create pipeline layout
    m_pipelineData.layoutCreateInfo.setSetLayoutCount(1);
    m_pipelineData.layoutCreateInfo.setPSetLayouts(m_renderData.descriptors.layouts.data());
    vk::Result result; 
    std::tie(result, m_pipelineData.layout) = m_deviceData.device.createPipelineLayout(m_pipelineData.layoutCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create pipeline layout." << std::endl;
        return false;
    }

    //Create pipline
    m_pipelineData.graphicsPipelineCreateInfo.setLayout(m_pipelineData.layout);
    m_pipelineData.graphicsPipelineCreateInfo.setPVertexInputState(&m_pipelineData.vertexInputStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPInputAssemblyState(&m_pipelineData.inputAssemblyStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPRasterizationState(&m_pipelineData.rasterizationStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPColorBlendState(&m_pipelineData.colorBlendStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPMultisampleState(&m_pipelineData.multisampleStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPDynamicState(&m_pipelineData.dynamicStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPViewportState(&m_pipelineData.viewportStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPDepthStencilState(&m_pipelineData.depthStencilStateCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setPStages(m_pipelineData.shaderStagesCreateInfo);
    m_pipelineData.graphicsPipelineCreateInfo.setStageCount(m_pipelineData.shaderStagesSize);
    m_pipelineData.graphicsPipelineCreateInfo.setRenderPass(m_renderData.renderPass);

    std::tie(result, m_pipelineData.pipeline) = m_deviceData.device.createGraphicsPipeline(m_pipelineData.cache, m_pipelineData.graphicsPipelineCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create pipline." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::LoadShader(char const* filename, char*& buffer, long& size)
{
    FILE* pFile;
    size_t result;

    pFile = fopen(filename, "rb");
    if (pFile == NULL)
    {
        std::cerr << "File error." << stderr;
        return false;
    }

    fseek(pFile, 0, SEEK_END);
    size = ftell(pFile);
    rewind(pFile);

    buffer = (char*)malloc(sizeof(char)* size);
    if (buffer == NULL)
    {
        std::cerr << "Memory error." << stderr;
        return false;
    }

    result = fread(buffer, 1, size, pFile);
    if (result != size)
    {
        std::cerr << "Reading error." << stderr;
        return false;
    }

    fclose(pFile);
    return true;
}

vk::VertexInputBindingDescription const Compositor::VertexBuffer::s_bindingDescriptions
= vk::VertexInputBindingDescription(0, 0, vk::VertexInputRate::eVertex);

vk::VertexInputAttributeDescription const Compositor::VertexBuffer::s_attributeDescriptions[3]
{
    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, 0),
    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, normX)),
    vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, r)),
};

float const Compositor::VertexBuffer::s_vertices[6 * 12] = {
    -1, -1,  0,  0,
    0,  1,  0,  0,
    1,  0,  0,  1,

    -1,  1,  0,  0,
    0,  1,  0,  0,
    0,  1,  0,  1,

    1,  1,  0,  0,
    0,  1,  0,  0,
    0,  0,  1,  1,

    1, -1,  0,  0,
    0,  1,  0,  0,
    1,  1,  1,  1,

    -1, -1,  0,  0,
    0,  1,  0,  0,
    1,  0,  0,  1,

    1,  1,  0,  0,
    0,  1,  0,  0,
    0,  0,  1,  1,
};