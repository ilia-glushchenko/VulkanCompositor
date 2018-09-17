#ifndef VULKAN_COMPOSITOR_HELPERS_HPP
#define VULKAN_COMPOSITOR_HELPERS_HPP

#include <vulkan/vulkan.hpp>
#include <tuple>
#include <iostream>
#include <cstdint>

inline std::tuple<bool, uint32_t> FindMemoryTypeIndex(vk::PhysicalDevice device, vk::MemoryPropertyFlags flags)
{
    vk::PhysicalDeviceMemoryProperties const memoryProperties = device.getMemoryProperties();
    uint32_t memoryTypeIndex = 0;
    bool memoryTypeFound = false;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if (static_cast<bool>(memoryProperties.memoryTypes[i].propertyFlags & flags))
        {
            memoryTypeIndex = i;
            memoryTypeFound = true;
            break;
        }
    }

    return std::make_tuple(memoryTypeFound, memoryTypeIndex);
}

inline bool LoadShader(char const* filename, char*& buffer, long& size)
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

#endif