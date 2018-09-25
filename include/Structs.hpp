/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <Device.hpp>
#include <vulkan/vulkan.hpp>

namespace vkc
{

struct Vertex
{
    float x, y, z, w;
    static vk::VertexInputBindingDescription const s_inputBindingDescription;
    static vk::VertexInputAttributeDescription const s_inputAttributeDescription;
};

constexpr float s_vertices[4 * 6] = {
    -0.5, -0.5,  0.5,  1,
    -0.5,  0.5,  0.5,  1,
     0.5,  0.5,  0.5,  1,
     0.5, -0.5,  0.5,  1,
    -0.5, -0.5,  0.5,  1,
     0.5,  0.5,  0.5,  1,
};

class Image
{
public:
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageView view;
};

class Buffer
{
public:
    bool Stage(Device& device, void const* data, size_t size, vk::BufferUsageFlagBits usage);

    vk::Buffer buffer;
    vk::DeviceMemory memory;

private:
    bool CreateBuffer(Device& device, size_t size, vk::BufferUsageFlagBits usage);

    bool AllocateDeviceMemory(Device& device, size_t size, vk::MemoryPropertyFlags flags);

    bool CopyMemory(Device& device, void const* data, size_t size);
};

class Shader
{
public:
    bool Init(Device& device, char const* name, char const* path, vk::ShaderStageFlagBits stage);

    vk::Result state = vk::Result::eErrorInitializationFailed;
    vk::ShaderStageFlagBits stage;
    vk::ShaderModule shaderModule;
    vk::PipelineShaderStageCreateInfo shaderStage;
};

} // vkc namespace