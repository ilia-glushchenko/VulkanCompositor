/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Render.hpp>
#include <iostream>

namespace vkc
{

Render::Render(Device & device, Window & window)
    : m_device(device)
    , m_window(window)
{
}

Render::~Render()
{
    Shutdown();
}

bool Render::Init()
{
    return CreateSemaphores()
        && CreateShaders()
        && CreateVertexBuffer()
        && CreateRenderPass()
        && CreateFramebuffers()
        && CreatePipeline()
        && CreateCommandBuffers();
}

void Render::Shutdown()
{
    m_device.logical.waitIdle();

    if (m_presentFence)
        m_device.logical.destroyFence(m_presentFence);
    if (m_commandPool)
        m_device.logical.freeCommandBuffers(
            m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
    if (m_commandPool)
        m_device.logical.destroyCommandPool(m_commandPool);
    if (m_imageAvailableSemaphore)
        m_device.logical.destroySemaphore(m_imageAvailableSemaphore);
    if (m_renderDoneSemaphore)
        m_device.logical.destroySemaphore(m_renderDoneSemaphore);
    if (m_vertexShader.shaderModule)
        m_device.logical.destroyShaderModule(m_vertexShader.shaderModule);
    if (m_fragmentShader.shaderModule)
        m_device.logical.destroyShaderModule(m_fragmentShader.shaderModule);
    if (m_vertexBuffer.buffer)
        m_device.logical.destroyBuffer(m_vertexBuffer.buffer);
    if (m_vertexBuffer.memory)
        m_device.logical.freeMemory(m_vertexBuffer.memory);
    if (m_renderPass)
        m_device.logical.destroyRenderPass(m_renderPass);
    for (auto fb : m_framebuffers)
        if (fb) m_device.logical.destroyFramebuffer(fb);
    if (m_pipeline)
        m_device.logical.destroyPipeline(m_pipeline);
    if (m_pipelineCache)
        m_device.logical.destroyPipelineCache(m_pipelineCache);
    if (m_pipelineLayout)
        m_device.logical.destroyPipelineLayout(m_pipelineLayout);
}

bool Render::Frame()
{
    std::tie(status, m_currentFrameBuffer) = m_device.logical.acquireNextImageKHR(
        m_window.swapchain, UINT64_MAX, m_imageAvailableSemaphore, {});
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to acquire framebuffer image." << std::endl;
        return false;
    }

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::Result result = m_commandBuffers.back().begin(beginInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to begin command buffer." << std::endl;
        return false;
    }

    vk::RenderPassBeginInfo renderPassBegin;
    renderPassBegin.setFramebuffer(m_framebuffers[m_currentFrameBuffer]);
    renderPassBegin.setRenderArea(vk::Rect2D({ 0, 0 }, { m_window.width, m_window.height }));
    renderPassBegin.setRenderPass(m_renderPass);
    renderPassBegin.setClearValueCount(m_attachmentCount);
    renderPassBegin.setPClearValues(&m_colorClearValue);
    m_commandBuffers.back().beginRenderPass(renderPassBegin, vk::SubpassContents::eInline);
    {
        m_commandBuffers.back().bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
        vk::DeviceSize offsets[1] = { 0 };
        m_commandBuffers.back().bindVertexBuffers(0, 1, &m_vertexBuffer.buffer, offsets);
        m_commandBuffers.back().draw(6, 1, 0, 0);
    }
    m_commandBuffers.back().endRenderPass();

    result = m_commandBuffers.back().end();
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to end command buffer." << std::endl;
        return false;
    }

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(static_cast<uint32_t>(m_commandBuffers.size()));
    submitInfo.setPCommandBuffers(m_commandBuffers.data());
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    submitInfo.setPWaitDstStageMask(&waitStage);
    submitInfo.setWaitSemaphoreCount(1);
    submitInfo.setPWaitSemaphores(&m_imageAvailableSemaphore);
    submitInfo.setSignalSemaphoreCount(1);
    submitInfo.setPSignalSemaphores(&m_renderDoneSemaphore);

    result = m_device.queue.queue.submit(1, &submitInfo, m_presentFence);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to submit cmd." << std::endl;
        return false;
    }

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setPWaitSemaphores(&m_renderDoneSemaphore);
    presentInfo.setPSwapchains(&m_window.swapchain);
    presentInfo.setSwapchainCount(1);
    presentInfo.setPImageIndices(&m_currentFrameBuffer);
    result = m_device.queue.queue.presentKHR(presentInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to present queue." << std::endl;
        return false;
    }

    while (m_device.logical.waitForFences(1, &m_presentFence, true, UINT64_MAX) == vk::Result::eTimeout);
    m_device.logical.resetFences(1, &m_presentFence);
    m_commandBuffers.back().reset({});

    return true;
}

bool Render::CreateSemaphores()
{
    vk::SemaphoreCreateInfo createInfo;
    std::tie(status, m_renderDoneSemaphore) = m_device.logical.createSemaphore(createInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create semaphore." << std::endl;
        return false;
    }

    std::tie(status, m_imageAvailableSemaphore) = m_device.logical.createSemaphore(createInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create semaphore." << std::endl;
        return false;
    }

    vk::FenceCreateInfo fenceCreateInfo;
    std::tie(status, m_presentFence) = m_device.logical.createFence(fenceCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create fence." << std::endl;
        return false;
    }

    return true;
}

bool Render::CreateShaders()
{
    return m_vertexShader.Init(m_device, "main", "../shaders/shader.vert.spv", vk::ShaderStageFlagBits::eVertex)
        && m_fragmentShader.Init(m_device, "main", "../shaders/shader.frag.spv", vk::ShaderStageFlagBits::eFragment);
}

bool Render::CreateVertexBuffer()
{
    return m_vertexBuffer.Stage(m_device, s_vertices, sizeof(s_vertices), vk::BufferUsageFlagBits::eVertexBuffer);
}

bool Render::CreateRenderPass()
{
    vk::AttachmentDescription attachmentDescription;
    attachmentDescription.setFormat(m_window.surfaceFormat.format);
    attachmentDescription.setSamples(vk::SampleCountFlagBits::e1);
    attachmentDescription.setInitialLayout(vk::ImageLayout::eUndefined);
    attachmentDescription.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
    attachmentDescription.setLoadOp(vk::AttachmentLoadOp::eClear);
    attachmentDescription.setStoreOp(vk::AttachmentStoreOp::eStore);
    attachmentDescription.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachmentDescription.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

    vk::AttachmentReference attachmentReference;
    attachmentReference.setAttachment(0);
    attachmentReference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setColorAttachmentCount(m_attachmentCount);
    subpass.setPColorAttachments(&attachmentReference);
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

    vk::RenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.setAttachmentCount(m_attachmentCount);
    renderPassCreateInfo.setPAttachments(&attachmentDescription);
    renderPassCreateInfo.setSubpassCount(1);
    renderPassCreateInfo.setPSubpasses(&subpass);

    std::tie(status, m_renderPass) = m_device.logical.createRenderPass(renderPassCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create render pass." << std::endl;
        return false;
    }

    return true;
}

bool Render::CreateFramebuffers()
{
    for (uint8_t i = 0; i < m_framebufferCount; ++i)
    {
        vk::FramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.setRenderPass(m_renderPass);
        framebufferCreateInfo.setAttachmentCount(m_attachmentCount);
        framebufferCreateInfo.setPAttachments(&m_window.swapchainImages[i].view);
        framebufferCreateInfo.setHeight(m_window.height);
        framebufferCreateInfo.setWidth(m_window.width);
        framebufferCreateInfo.setLayers(1);

        std::tie(status, m_framebuffers[i]) = m_device.logical.createFramebuffer(framebufferCreateInfo);
        if (status != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create framebuffer." << std::endl;
            return false;
        }
    }

    return true;
}

bool Render::CreatePipeline()
{
    vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo;
    vertexInputCreateInfo.setVertexAttributeDescriptionCount(1);
    vertexInputCreateInfo.setPVertexAttributeDescriptions(&Vertex::s_inputAttributeDescription);
    vertexInputCreateInfo.setVertexBindingDescriptionCount(1);
    vertexInputCreateInfo.setPVertexBindingDescriptions(&Vertex::s_inputBindingDescription);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
    inputAssemblyCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssemblyCreateInfo.setPrimitiveRestartEnable(false);

    vk::PipelineShaderStageCreateInfo const shaderStages[2] = {
        m_vertexShader.shaderStage, m_fragmentShader.shaderStage
    };

    vk::Viewport viewport;
    viewport.setWidth(static_cast<float>(m_window.width));
    viewport.setHeight(static_cast<float>(m_window.height));
    viewport.setX(0);
    viewport.setY(0);
    viewport.setMaxDepth(1.0f);
    viewport.setMinDepth(0);

    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 });
    scissor.setExtent({ m_window.width, m_window.height });

    vk::PipelineViewportStateCreateInfo viewportCreateInfo;
    viewportCreateInfo.setScissorCount(1);
    viewportCreateInfo.setPScissors(&scissor);
    viewportCreateInfo.setViewportCount(1);
    viewportCreateInfo.setPViewports(&viewport);

    vk::PipelineRasterizationStateCreateInfo rasterizationCreateInfo;
    rasterizationCreateInfo.setCullMode(vk::CullModeFlagBits::eNone);
    rasterizationCreateInfo.setPolygonMode(vk::PolygonMode::eFill);
    rasterizationCreateInfo.setFrontFace(vk::FrontFace::eClockwise);
    rasterizationCreateInfo.setLineWidth(1.0f);
    rasterizationCreateInfo.setDepthClampEnable(false);
    rasterizationCreateInfo.setDepthBiasEnable(false);
    rasterizationCreateInfo.setRasterizerDiscardEnable(false);

    vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo;
    multisamplingCreateInfo.setSampleShadingEnable(false);
    multisamplingCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
    colorBlendAttachmentState.setBlendEnable(false);
    colorBlendAttachmentState.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo;
    colorBlendCreateInfo.setAttachmentCount(1);
    colorBlendCreateInfo.setPAttachments(&colorBlendAttachmentState);
    colorBlendCreateInfo.setLogicOpEnable(false);

    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    std::tie(status, m_pipelineLayout) = m_device.logical.createPipelineLayout(layoutCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create pipeline layout." << std::endl;
        return false;
    }

    vk::PipelineCacheCreateInfo cacheCreateInfo;
    std::tie(status, m_pipelineCache) = m_device.logical.createPipelineCache(cacheCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create pipeline cache." << std::endl;
        return false;
    }

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.setPVertexInputState(&vertexInputCreateInfo);
    pipelineCreateInfo.setPInputAssemblyState(&inputAssemblyCreateInfo);
    pipelineCreateInfo.setStageCount(2);
    pipelineCreateInfo.setPStages(shaderStages);
    pipelineCreateInfo.setPViewportState(&viewportCreateInfo);
    pipelineCreateInfo.setPRasterizationState(&rasterizationCreateInfo);
    pipelineCreateInfo.setPMultisampleState(&multisamplingCreateInfo);
    pipelineCreateInfo.setPColorBlendState(&colorBlendCreateInfo);
    pipelineCreateInfo.setRenderPass(m_renderPass);
    pipelineCreateInfo.setSubpass(0);
    pipelineCreateInfo.setLayout(m_pipelineLayout);

    std::tie(status, m_pipeline) = m_device.logical.createGraphicsPipeline(m_pipelineCache, pipelineCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create graphics pipeline." << std::endl;
        return false;
    }

    return true;
}

bool Render::CreateCommandBuffers()
{
    vk::CommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    cmdPoolCreateInfo.setQueueFamilyIndex(m_device.queue.familyIndex);

    std::tie(status, m_commandPool) = m_device.logical.createCommandPool(cmdPoolCreateInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate command buffer pool." << std::endl;
        return false;
    }

    vk::CommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.setCommandBufferCount(1);
    cmdAllocInfo.setCommandPool(m_commandPool);
    cmdAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);

    std::tie(status, m_commandBuffers) = m_device.logical.allocateCommandBuffers(cmdAllocInfo);
    if (status != vk::Result::eSuccess)
    {
        std::cout << "Failed to allocate command buffers." << std::endl;
        return false;
    }

    return true;
}

} // vkc namespace