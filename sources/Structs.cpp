/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Helpers.hpp>
#include <Structs.hpp>

namespace vkc
{

vk::VertexInputBindingDescription const Vertex::s_inputBindingDescription = {
    0, sizeof(Vertex), vk::VertexInputRate::eVertex };
vk::VertexInputAttributeDescription const Vertex::s_inputAttributeDescription = {
    0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, x) };

bool Buffer::Stage(Device & device, void const * data, size_t size, vk::BufferUsageFlagBits usage)
{
    return CreateBuffer(device, size, usage)
        && AllocateDeviceMemory(device, size, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        && CopyMemory(device, data, size);
}

bool Buffer::CreateBuffer(Device & device, size_t size, vk::BufferUsageFlagBits usage)
{
    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.setQueueFamilyIndexCount(1);
    bufferCreateInfo.setPQueueFamilyIndices(&device.queue.familyIndex);
    bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    bufferCreateInfo.setSize(size);
    bufferCreateInfo.setUsage(usage);

    vk::Result result;
    std::tie(result, buffer) = device.logical.createBuffer(bufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create buffer." << std::endl;
        return false;
    }

    return true;
}

bool Buffer::AllocateDeviceMemory(Device & device, size_t size, vk::MemoryPropertyFlags flags)
{
    bool memoryAvailable = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryAvailable, memoryTypeIndex) = FindMemoryTypeIndex(device.physical, flags);

    if (!memoryAvailable)
    {
        std::cerr << "Failed to find memory type." << std::endl;
        return false;
    }


    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements memoryRequirements = device.logical.getBufferMemoryRequirements(buffer);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    vk::Result result;
    std::tie(result, memory) = device.logical.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate memory." << std::endl;
        return false;
    }

    result = device.logical.bindBufferMemory(buffer, memory, 0);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to bind memory to buffer." << std::endl;
        return false;
    }

    return true;
}

bool Buffer::CopyMemory(Device & device, void const * data, size_t size)
{
    vk::Result result;
    void* mappedMemory = nullptr;
    std::tie(result, mappedMemory) = device.logical.mapMemory(memory, 0, size);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to map memory" << std::endl;
        return false;
    }

    memcpy(mappedMemory, data, size);

    device.logical.unmapMemory(memory);

    return true;
}

bool Shader::Init(Device & device, char const * name, char const * path, vk::ShaderStageFlagBits stage)
{
    char* shaderCodeBuffer;
    long shaderCodeSize;
    if (!LoadShader(path, shaderCodeBuffer, shaderCodeSize))
    {
        std::cerr << "Failed to load shader: " << name << " " << path << std::endl;
        return false;
    }

    vk::ShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.setCodeSize(static_cast<size_t>(shaderCodeSize));
    shaderModuleCreateInfo.setPCode(reinterpret_cast<uint32_t*>(shaderCodeBuffer));
    std::tie(state, shaderModule) = device.logical.createShaderModule(shaderModuleCreateInfo);
    if (state != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create shader module: " << name << " " << path << std::endl;
        return false;
    }

    shaderStage.setPName(name);
    shaderStage.setStage(stage);
    shaderStage.setModule(shaderModule);

    return true;
}

} // vkc namespace