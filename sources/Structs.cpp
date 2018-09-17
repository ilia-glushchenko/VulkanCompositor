#include <Structs.hpp>
#include <iostream>

bool DeviceData::InitializeInstace()
{
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
    std::tie(result, instance) = vk::createInstance(instanceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Vulkan instance creation failed." << std::endl;
        return false;
    }

    return true;
}

bool DeviceData::InitializePhysicalDevice()
{
    vk::Result result;
    std::vector<vk::PhysicalDevice> physicalDevices;
    std::tie(result, physicalDevices) = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        std::cerr << "System has no Vulkan capable physical devices." << std::endl;
        return false;
    }

    physical = physicalDevices.front();
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
            physical = *physicalDevice;
            break;
        }
    }

    return true;
}

bool DeviceData::InitializeLogicalDevice(VkSurfaceKHR& surface)
{
    //Get transfer and graphics queue families
    auto queueFamilyProperties = physical.getQueueFamilyProperties();
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

    //Get windows surface presentation capable queue
    auto presentationQueueFamilyProperties = queueFamilyProperties.end();
    for (uint32_t index = 0; index < queueFamilyProperties.size(); ++index)
    {
        vk::Result result;
        vk::Bool32 support;
        std::tie(result, support) = physical.getSurfaceSupportKHR(index, surface);
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
    vk::DeviceQueueCreateInfo graphicsQueueCreateInfo;
    graphicsQueueCreateInfo.setQueueFamilyIndex(
        static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperties)));
    graphicsQueueCreateInfo.setQueueCount(1);
    graphicsQueueCreateInfo.setPQueuePriorities(&queuePriorities);
    graphicsQueueData.familyIndex = graphicsQueueCreateInfo.queueFamilyIndex;

    vk::DeviceQueueCreateInfo presentationQueueCreateInfo;
    presentationQueueCreateInfo.setQueueFamilyIndex(
        static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), presentationQueueFamilyProperties)));
    presentationQueueCreateInfo.setQueueCount(1);
    presentationQueueCreateInfo.setPQueuePriorities(&queuePriorities);
    presentQueueData.familyIndex = presentationQueueCreateInfo.queueFamilyIndex;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos = { {
        graphicsQueueCreateInfo, presentationQueueCreateInfo
    } };
    std::sort(queueCreateInfos.begin(), queueCreateInfos.end(), [](auto a, auto b) -> bool {
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
    std::tie(result, logical) = physical.createDevice(deviceCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create vulkan device." << std::endl;
        return false;
    }

    //Get queue handles
    graphicsQueueData.queue = logical.getQueue(graphicsQueueCreateInfo.queueFamilyIndex, 0);
    presentQueueData.queue = logical.getQueue(presentationQueueCreateInfo.queueFamilyIndex, 0);

    return true;
}

bool Buffer::CreateBuffer(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data)
{
    return CreateBufferObject(deviceData, bufferCreateInfo, data);
}

bool Buffer::CreateBuffer(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data, vk::DescriptorSetLayoutBinding setLayoutBinding)
{
    if (CreateBuffer(deviceData, bufferCreateInfo, data))
    {
        if (CreateDescriptorSetLayout(deviceData, setLayoutBinding)
            && CreateDescriptorPool(deviceData, setLayoutBinding)
            && CreateDescriptorSet(deviceData))
        {
            UpdateDescriptorSet(deviceData, setLayoutBinding, bufferCreateInfo.size);
            return true;
        }
    }

    return false;
}

bool Buffer::CreateBufferObject(DeviceData& deviceData, vk::BufferCreateInfo bufferCreateInfo, void* data)
{
    //Create buffer
    vk::Result result;
    std::tie(result, buffer) = deviceData.logical.createBuffer(bufferCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create buffer." << std::endl;
        return false;
    }

    //Allocate buffer memory
    bool memoryTypeFound = false;
    uint32_t memoryTypeIndex = 0;
    std::tie(memoryTypeFound, memoryTypeIndex) = FindMemoryTypeIndex(deviceData.physical, vk::MemoryPropertyFlagBits::eHostVisible);
    if (!memoryTypeFound)
    {
        std::cerr << "Failed to find host visible memory type." << std::endl;
        return false;
    }

    vk::MemoryAllocateInfo memoryAllocateInfo;
    vk::MemoryRequirements const memoryRequirements = deviceData.logical.getBufferMemoryRequirements(buffer);
    memoryAllocateInfo.setAllocationSize(memoryRequirements.size);
    memoryAllocateInfo.setMemoryTypeIndex(memoryTypeIndex);
    std::tie(result, memory) = deviceData.logical.allocateMemory(memoryAllocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate buffer memory." << std::endl;
        return false;
    }

    //Bind buffer to memory
    result = deviceData.logical.bindBufferMemory(buffer, memory, 0);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to bind buffer to memory." << std::endl;
        return false;
    }

    //Write to buffer
    void* mappedMemory = nullptr;
    std::tie(result, mappedMemory) = deviceData.logical.mapMemory(memory, 0, bufferCreateInfo.size);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to map buffer memory." << std::endl;
        return false;
    }

    memcpy(mappedMemory, data, bufferCreateInfo.size);
    deviceData.logical.unmapMemory(memory);

    return true;
}

bool Buffer::CreateDescriptorSetLayout(DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding)
{
    //Create uniform descriptor set layout
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setBindingCount(1);
    layoutCreateInfo.setPBindings(&setLayoutBinding);

    vk::Result result;
    descriptors.layouts.resize(1);
    std::tie(result, descriptors.layouts[0]) = deviceData.logical.createDescriptorSetLayout(layoutCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create buffer descriptor set layout." << std::endl;
        return false;
    }

    return true;
}

bool Buffer::CreateDescriptorPool(DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding)
{
    //Specify descriptor sizes
    vk::DescriptorPoolSize poolSize[1];
    poolSize[0].setType(setLayoutBinding.descriptorType);
    poolSize[0].setDescriptorCount(setLayoutBinding.descriptorCount);

    //Allocate pool
    vk::DescriptorPoolCreateInfo poolCreateInfo;
    poolCreateInfo.setPPoolSizes(poolSize);
    poolCreateInfo.setPoolSizeCount(1);
    poolCreateInfo.setMaxSets(1);

    vk::Result result;
    std::tie(result, descriptors.pool) = deviceData.logical.createDescriptorPool(poolCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create descriptor pool." << std::endl;
        return false;
    }

    descriptors.descriptorsCount.push_back(poolCreateInfo.poolSizeCount);

    return true;
}

bool Buffer::CreateDescriptorSet(DeviceData& deviceData)
{
    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.setDescriptorPool(descriptors.pool);
    allocateInfo.setPSetLayouts(descriptors.layouts.data());
    allocateInfo.setDescriptorSetCount(static_cast<uint32_t>(descriptors.layouts.size()));

    vk::Result result;
    std::tie(result, descriptors.sets) = deviceData.logical.allocateDescriptorSets(allocateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to allocate descriptor sets." << std::endl;
        return false;
    }

    return true;
}

void Buffer::UpdateDescriptorSet(
    DeviceData& deviceData, vk::DescriptorSetLayoutBinding setLayoutBinding, vk::DeviceSize range)
{
    for (uint32_t i = 0; i < descriptors.sets.size(); ++i)
    {
        vk::DescriptorBufferInfo descriptorBufferInfo;
        descriptorBufferInfo.setBuffer(buffer);
        descriptorBufferInfo.setOffset(0);
        descriptorBufferInfo.setRange(range);

        vk::WriteDescriptorSet writeDescriptorSet;
        writeDescriptorSet.setDescriptorCount(descriptors.descriptorsCount[i]);
        writeDescriptorSet.setDescriptorType(setLayoutBinding.descriptorType);
        writeDescriptorSet.setDstSet(descriptors.sets[i]);
        writeDescriptorSet.setDstArrayElement(0);
        writeDescriptorSet.setDstBinding(setLayoutBinding.binding);
        writeDescriptorSet.setPBufferInfo(&descriptorBufferInfo);

        deviceData.logical.updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
    }
}

bool Shader::CreateShader(DeviceData & deviceData, char const * name, char const * filename, vk::ShaderStageFlagBits stage)
{
    //Load fragment shader spir-v byte code
    char* pShaderBuffer = nullptr;
    long shaderBufferSize = 0;
    if (!LoadShader(filename, pShaderBuffer, shaderBufferSize))
    {
        std::cerr << "Failed to load SPIR-V " << filename << " shader code." << std::endl;
        return false;
    }

    //Create fragment shader module
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.setCodeSize(shaderBufferSize);
    shaderModuleCreateInfo.setPCode(reinterpret_cast<uint32_t*>(pShaderBuffer));
    vk::Result result;
    std::tie(result, shaderModule) = deviceData.logical.createShaderModule(shaderModuleCreateInfo);
    if (result != vk::Result::eSuccess)
    {
        std::cerr << "Failed to create " << filename << " shader module." << std::endl;
        return false;
    }
    free(pShaderBuffer);

    //Fill fragment shader stage create info
    shaderStageCreateInfo.setStage(stage);
    shaderStageCreateInfo.setModule(shaderModule);
    shaderStageCreateInfo.setPName(name);

    return true;
}

vk::VertexInputBindingDescription const VertexBuffer::s_bindingDescriptions
    = vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);

vk::VertexInputAttributeDescription const VertexBuffer::s_attributeDescriptions[3]
{
    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, 0),
    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, normX)),
    vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, r)),
};

float const VertexBuffer::s_vertices[6 * 12] = {
   -0.5, -0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0.5,  0,    0.5,  1,

   -0.5,  0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0,    0.5,  0.5,  1,

    0.5,  0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0,    0,    0.5,  1,

    0.5, -0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0.5,  0.5,  0.5,  1,

   -0.5, -0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0.5,  0,    0.5,  1,

    0.5,  0.5,  0.5,  1,
    0,    0.5,  0.5,  1,
    0,    0,    0.5,  1,
};

