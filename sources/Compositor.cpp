/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>
#include <iostream>

Compositor::~Compositor()
{
    m_deviceData.logical.destroyPipeline(m_pipelineData.pipeline);

    //Destroy uniform buffer
    m_deviceData.logical.destroyBufferView(m_renderData.uniformBuffer.view);
    m_deviceData.logical.freeMemory(m_renderData.uniformBuffer.memory);
    m_deviceData.logical.destroyBuffer(m_renderData.uniformBuffer.buffer);

    //Destroy depth buffer
    m_deviceData.logical.destroyImageView(m_renderData.depthBufferImage.view);
    m_deviceData.logical.freeMemory(m_renderData.depthBufferImage.memory);
    m_deviceData.logical.destroyImage(m_renderData.depthBufferImage.image);

    //Destroy instance device and window
    m_deviceData.logical.destroy();
    glfwDestroyWindow(m_windowData.pWindow);
    m_deviceData.instance.destroy();
}

bool Compositor::Initialize()
{
    glfwInit();

    m_initialized = m_deviceData.InitializeInstace();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeWindowSurface();
    assert(m_initialized);
    m_initialized = m_initialized && m_deviceData.InitializePhysicalDevice();
    assert(m_initialized);
    m_initialized = m_initialized && m_deviceData.InitializeLogicalDevice(m_windowData.surface);
    assert(m_initialized);
    m_initialized = m_initialized && InitializeSwapchain();
    assert(m_initialized);

    m_initialized = m_initialized && InitializeCommandPools();
    assert(m_initialized);
    m_initialized = m_initialized && InititlizeCommandBuffers();
    assert(m_initialized);

    m_initialized = m_initialized && IntitilizeDepthBufferImage();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeUniformBuffer();
    assert(m_initialized);
    m_initialized = m_initialized && InitializeVeretexBuffer();
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
    {
        //Prepare render pass
        UpdateCurrentFramebuffer();

        vk::SurfaceCapabilitiesKHR surfaceCapabilities;
        m_deviceData.physical.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = m_renderData.renderPass;
        renderPassBeginInfo.framebuffer = m_windowData.framebuffers[m_windowData.currentBuffer];
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = surfaceCapabilities.currentExtent.width;
        renderPassBeginInfo.renderArea.extent.height = surfaceCapabilities.currentExtent.height;
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(m_renderData.clearValues.size());
        renderPassBeginInfo.pClearValues = m_renderData.clearValues.data();

        //Fill command buffers
        vk::CommandBufferBeginInfo cmdBeginInfo;
        vk::CommandBuffer& cmd = m_deviceData.graphicsQueueData.commandBuffers.back();
        cmd.reset({});
        cmd.begin(cmdBeginInfo);

        cmd.bindPipeline(m_pipelineData.bindPoint, m_pipelineData.pipeline);
        cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        cmd.setViewport(0, 1, &m_renderData.viewport);
        cmd.setScissor(0, 1, &m_renderData.scissors);
        cmd.bindDescriptorSets(m_pipelineData.bindPoint, m_pipelineData.layout, 0, 1, m_renderData.uniformBuffer.descriptors.sets.data(), 0, nullptr);
        const vk::DeviceSize offsets[1] = { 0 };
        cmd.bindVertexBuffers(0, 1, &m_renderData.vertexBuffer.buffer, offsets);
        cmd.draw(6, 1, 0, 0);
        cmd.endRenderPass();

        cmd.end();
    }

    //Submit graphics queue
    {
        vk::PipelineStageFlags pipelineStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        
        vk::SubmitInfo submitInfo;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_windowData.swapchainImageAcquireSemaphore;
        submitInfo.pWaitDstStageMask = &pipelineStageFlags;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_deviceData.graphicsQueueData.commandBuffers.back();
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
        vk::Result result = m_deviceData.graphicsQueueData.queue.submit(1, &submitInfo, m_renderData.drawFence);

        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to submit graphics queue for execution." << std::endl;
        }
    }

    //Present image to present queue
    {
        while (m_deviceData.logical.waitForFences(1, &m_renderData.drawFence, true, UINT32_MAX) == vk::Result::eTimeout);
        m_deviceData.logical.resetFences(1, &m_renderData.drawFence);

        vk::PresentInfoKHR presentInfo;
        presentInfo.setSwapchainCount(1);
        presentInfo.setPSwapchains(&m_windowData.swapchain);
        presentInfo.setPImageIndices(&m_windowData.currentBuffer);
        m_deviceData.presentQueueData.queue.presentKHR(presentInfo);

        glfwSwapBuffers(m_windowData.pWindow);
        glfwPollEvents();

    }
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

bool Compositor::InitializeSwapchain()
{
    //Query surface for capabilities
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physical.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
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
    std::tie(result, surfaceFormats) = m_deviceData.physical.getSurfaceFormatsKHR(m_windowData.surface);
    if (result != vk::Result::eSuccess || surfaceFormats.empty())
    {
        std::cerr << "Failed to get physical device's surface formats." << std::endl;
        return false;
    }
    m_windowData.surfaceFormat = surfaceFormats.front();

    //Find surface with propper presentation modes
    std::vector<vk::PresentModeKHR> surfacePresentModes;
    std::tie(result, surfacePresentModes) = m_deviceData.physical.getSurfacePresentModesKHR(m_windowData.surface);
    if (result != vk::Result::eSuccess || surfacePresentModes.empty())
    {
        std::cerr << "Failed to get physical devices's surface presentation modes." << std::endl;
        return false;
    }
    auto presentMode = vk::PresentModeKHR::eFifo;
    if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), vk::PresentModeKHR::eFifo) == surfacePresentModes.end())
    {
        presentMode = vk::PresentModeKHR::eMailbox;
        if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), vk::PresentModeKHR::eMailbox) == surfacePresentModes.end())
        {
            presentMode = vk::PresentModeKHR::eImmediate;
            if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), vk::PresentModeKHR::eImmediate) == surfacePresentModes.end())
            {
                std::cerr << "Failed to get surface with FIFO, MailBox or Immediate presentation mode." << std::endl;
                return false;
            }
        }
    }

    //Clear repeated queue family indices
    std::vector<uint32_t> queueFamilyIndices = {{
        m_deviceData.graphicsQueueData.familyIndex, m_deviceData.presentQueueData.familyIndex
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
    swapchainCreateInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    swapchainCreateInfo.setQueueFamilyIndexCount(static_cast<uint32_t>(queueFamilyIndices.size()));
    swapchainCreateInfo.setPQueueFamilyIndices(queueFamilyIndices.data());
    swapchainCreateInfo.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity);
    swapchainCreateInfo.setCompositeAlpha(compositeAlpha);
    swapchainCreateInfo.setPresentMode(presentMode);
    swapchainCreateInfo.setClipped(VK_TRUE);
    std::tie(result, m_windowData.swapchain) = m_deviceData.logical.createSwapchainKHR(swapchainCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create swapchain." << std::endl;
        return false;
    }

    //Create swapchain images
    std::tie(result, m_windowData.swapchainImages) = m_deviceData.logical.getSwapchainImagesKHR(m_windowData.swapchain);
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
        imageViewCreateInfo.subresourceRange.setLayerCount(1);
        imageViewCreateInfo.subresourceRange.setBaseArrayLayer(0);

        std::tie(result, m_windowData.swapchainImageViews[i]) = m_deviceData.logical.createImageView(imageViewCreateInfo);
        if (result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create swapchain image view." << std::endl;
            return false;
        }
    }

    return true;
}

bool Compositor::InitializeCommandPools()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.setQueueFamilyIndex(m_deviceData.graphicsQueueData.familyIndex);
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    vk::Result result;
    std::tie(result, m_deviceData.graphicsQueueData.commandPool) = m_deviceData.logical.createCommandPool(commandPoolCreateInfo);
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
    commandBufferAllocateInfo.setCommandBufferCount(1);
    commandBufferAllocateInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    commandBufferAllocateInfo.setCommandPool(m_deviceData.graphicsQueueData.commandPool);
    vk::Result result;
    std::tie(result, m_deviceData.graphicsQueueData.commandBuffers) = m_deviceData.logical.allocateCommandBuffers(commandBufferAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate graphics queue command buffers." << std::endl;
        return false;
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
    m_deviceData.physical.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
    bufferCreateInfo.extent.width = surfaceCapabilities.currentExtent.width;
    bufferCreateInfo.extent.height = surfaceCapabilities.currentExtent.height;
    bufferCreateInfo.extent.depth = 1;
    bufferCreateInfo.mipLevels = 1;
    bufferCreateInfo.arrayLayers = 1;
    bufferCreateInfo.samples = m_renderData.m_depthBufferSamples;
    bufferCreateInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Result result;
    std::tie(result, m_renderData.depthBufferImage.image) = m_deviceData.logical.createImage(bufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create depth buffer image." << std::endl;
        return false;
    }

    //Find propper physical device memory type heap index
    bool memoryTypeFound = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryTypeFound, memoryTypeIndex) = FindMemoryTypeIndex(
        m_deviceData.physical, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (!memoryTypeFound)
    {
        std::cerr << "Failed to find physical device memory heap for the depth buffer." << std::endl;
        return false;
    }
    
    //Allocate memory for the depth buffer image
    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements const memoryRequirements = m_deviceData.logical.getImageMemoryRequirements(m_renderData.depthBufferImage.image);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    std::tie(result, m_renderData.depthBufferImage.memory) =  m_deviceData.logical.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate depth buffer memory." << std::endl;
        return false;
    }

    //Bind Image to Memory
    result = m_deviceData.logical.bindImageMemory(m_renderData.depthBufferImage.image, m_renderData.depthBufferImage.memory, 0);
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
    std::tie(result, m_renderData.depthBufferImage.view) = m_deviceData.logical.createImageView(imageViewCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create depth buffer image view." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeUniformBuffer()
{
    vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding;
    descriptorSetLayoutBinding.setBinding(0);
    descriptorSetLayoutBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    descriptorSetLayoutBinding.setDescriptorCount(1);
    descriptorSetLayoutBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.setSize(sizeof(MVP));
    bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    bufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

    return m_renderData.uniformBuffer.CreateBuffer(m_deviceData, bufferCreateInfo, &m_renderData.mvp, descriptorSetLayoutBinding);
}

bool Compositor::InitializeVeretexBuffer()
{
    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.setSize(sizeof(VertexBuffer::s_vertices));
    bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    bufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);

    return m_renderData.vertexBuffer.CreateBuffer(m_deviceData, bufferCreateInfo, (void*)VertexBuffer::s_vertices);
}

bool Compositor::InitializeShaders()
{
    
    if (!m_renderData.vertexShader.CreateShader(m_deviceData, "main", "../shaders/shader.vert.spv", vk::ShaderStageFlagBits::eVertex))
    {
        std::cerr << "Failed to create vertex shader." << std::endl;
        return false;
    }

    if (!m_renderData.fragmentShader.CreateShader(m_deviceData, "main", "../shaders/shader.frag.spv", vk::ShaderStageFlagBits::eFragment))
    {
        std::cerr << "Failed to create fragment shader." << std::endl;
        return false;
    }

    //Fill vertex input state create info
    m_pipelineData.shaderStagesCreateInfo[0] = m_renderData.vertexShader.shaderStageCreateInfo;
    m_pipelineData.shaderStagesCreateInfo[1] = m_renderData.fragmentShader.shaderStageCreateInfo;

    return true;
}

bool Compositor::InitializeSemaphors()
{
    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    vk::Result result;    
    std::tie(result, m_windowData.swapchainImageAcquireSemaphore) = m_deviceData.logical.createSemaphore(semaphoreCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create semaphore." << std::endl;
        return false;
    }

    vk::FenceCreateInfo fenceCreateInfo;
    std::tie(result, m_windowData.swapchainImageAcquireFence) = m_deviceData.logical.createFence(fenceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create fence." << std::endl;
        return false;
    }

    std::tie(result, m_renderData.drawFence) = m_deviceData.logical.createFence(fenceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create draw fence." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeRenderPass()
{
    //Define render pass attachments
    vk::AttachmentDescription attachments[2];
    attachments[0].setFormat(m_windowData.surfaceFormat.format);
    attachments[0].setSamples(vk::SampleCountFlagBits::e1);
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

    //Render pass dependencies
    vk::SubpassDependency dependencies[2];

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;							
    dependencies[0].dstSubpass = 0;													
    dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[0].dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
    dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    dependencies[1].srcSubpass = 0;													
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;								
    dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[1].srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
    dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    //Define render pass
    vk::RenderPassCreateInfo createInfo;
    createInfo.setAttachmentCount(2);
    createInfo.setPAttachments(attachments);
    createInfo.setSubpassCount(1);
    createInfo.setPSubpasses(&subpass);
    createInfo.setDependencyCount(2);
    createInfo.setPDependencies(dependencies);

    vk::Result result;
    std::tie(result, m_renderData.renderPass) = m_deviceData.logical.createRenderPass(createInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create render pass." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::InitializeFramebuffers()
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physical.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
    m_windowData.framebuffers.resize(m_windowData.swapchainImages.size());

    for (uint8_t i = 0; i < m_windowData.swapchainImageViews.size(); ++i)
    {
        vk::ImageView const attachments[2] = { m_windowData.swapchainImageViews[i], m_renderData.depthBufferImage.view };
          
        vk::FramebufferCreateInfo frameBufferCreateInfo;
        frameBufferCreateInfo.setRenderPass(m_renderData.renderPass);
        frameBufferCreateInfo.setAttachmentCount(2);
        frameBufferCreateInfo.setPAttachments(attachments);
        frameBufferCreateInfo.setWidth(surfaceCapabilities.currentExtent.width);
        frameBufferCreateInfo.setHeight(surfaceCapabilities.currentExtent.height);
        frameBufferCreateInfo.setLayers(1);

        vk::Result result;
        std::tie(result, m_windowData.framebuffers[i]) = m_deviceData.logical.createFramebuffer(frameBufferCreateInfo);
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
    vk::DynamicState dynamicStateEnables[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    m_pipelineData.dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
    m_pipelineData.dynamicStateCreateInfo.dynamicStateCount = 2;

    //Viewport and scissors
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    m_deviceData.physical.getSurfaceCapabilitiesKHR(m_windowData.surface, &surfaceCapabilities);
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
    m_pipelineData.rasterizationStateCreateInfo.setCullMode(vk::CullModeFlagBits::eNone);
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
    m_pipelineData.alphaBlendAttachmentState.setBlendEnable(false);
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
    m_pipelineData.depthStencilStateCreateInfo.setDepthTestEnable(false);
    m_pipelineData.depthStencilStateCreateInfo.setDepthWriteEnable(false);
    m_pipelineData.depthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    m_pipelineData.depthStencilStateCreateInfo.setDepthBoundsTestEnable(false);
    m_pipelineData.depthStencilStateCreateInfo.setMinDepthBounds(0.0f);
    m_pipelineData.depthStencilStateCreateInfo.setMaxDepthBounds(1.0f);
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
    m_pipelineData.layoutCreateInfo.setPSetLayouts(m_renderData.uniformBuffer.descriptors.layouts.data());
    vk::Result result; 
    std::tie(result, m_pipelineData.layout) = m_deviceData.logical.createPipelineLayout(m_pipelineData.layoutCreateInfo);
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
    m_pipelineData.bindPoint = vk::PipelineBindPoint::eGraphics;

    std::tie(result, m_pipelineData.pipeline) = m_deviceData.logical.createGraphicsPipeline(m_pipelineData.cache, m_pipelineData.graphicsPipelineCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create pipline." << std::endl;
        return false;
    }

    return true;
}

bool Compositor::UpdateCurrentFramebuffer()
{
    vk::Result result;
    std::tie(result, m_windowData.currentBuffer) = m_deviceData.logical.acquireNextImageKHR(
        m_windowData.swapchain, UINT64_MAX, m_windowData.swapchainImageAcquireSemaphore, m_windowData.swapchainImageAcquireFence);
    
    while (m_deviceData.logical.waitForFences(1, &m_windowData.swapchainImageAcquireFence, true, UINT32_MAX) == vk::Result::eTimeout);
    m_deviceData.logical.resetFences(1, &m_windowData.swapchainImageAcquireFence);

    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to acquire next image index." << std::endl;
        return false;
    }

    return true;
}

