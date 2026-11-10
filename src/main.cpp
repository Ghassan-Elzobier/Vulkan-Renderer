#define GLFW_INCLUDE_VULKAN //Include vulkan header
#include <GLFW/glfw3.h> // GLFW header

#include <cstdint> // fixed width integers
#include <cstring> // C-style strings
#include <iostream> // input and output logs
#include <stdexcept> // Allows you throw errors
#include <vector> // usually used to manage vulkan array as it is dynamic
#include <optional> // Represents a value or nothing in a type safe way
#include <algorithm>
#include <fstream>
#include <string>

#ifdef NDEBUG // Disable validation in production build
    constexpr bool enableValidationLayers = false;
#else // enable validation in debugging build
    constexpr bool enableValidationLayers = true;
#endif

// Store vk validation variable name as c-string
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool checkValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const VkLayerProperties& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, availableExtensions.data());

    for (const char* name : deviceExtensions) {
        bool found = false;
        for (const VkExtensionProperties& extensionProperties : availableExtensions) {
            if (std::strcmp(name, extensionProperties.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

VkInstance createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Ray Tracer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    return instance;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
        std::cerr << "Validation: " << pCallbackData -> pMessage << "\n" ;
        return VK_FALSE;
    }

VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr){
        throw std::runtime_error("Could not load vkCreateDebugUtilsMessengerEXT");
    }

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    if (func(instance, &createInfo, nullptr, &messenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug Messenger");
    }
    return messenger;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> compute;
    std::optional<uint32_t> present;
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());
    for (uint32_t i=0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) indices.compute = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) indices.present = i;
    }
    return indices;
}

VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) throw std::runtime_error("no GPU with Vulkan Support");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for (VkPhysicalDevice device : devices) {
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        if (checkDeviceExtensionSupport(device) && indices.compute && indices.present ) {
            return device;
        }
    }
    throw std::runtime_error("no suitable GPU found");
}

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex) {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    return device;

}

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;
    uint32_t formatCount = 0;
    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
    details.presentModes.resize(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const VkSurfaceFormatKHR& format : availableFormats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (VkPresentModeKHR presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actual = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actual.width = std::clamp(actual.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual.height = std::clamp(actual.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual;
}

struct Swapchain {
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    VkFormat format;
    VkExtent2D extent;
};

Swapchain createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkSurfaceKHR surface, GLFWwindow* window,
                          const QueueFamilyIndices& indices) {
    SwapchainSupportDetails support = querySwapchainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }
    

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    uint32_t queueFamilyIndices[] = {
        indices.compute.value(), 
        indices.present.value()
    };
    if (indices.compute != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR handle = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &handle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, handle, &count, nullptr);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(device, handle, &count, images.data());

    Swapchain swapchain{};
    swapchain.handle = handle;
    swapchain.images = images;
    swapchain.format = surfaceFormat.format;
    swapchain.extent = extent;

    return swapchain;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    return commandPool;
}

VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
    return commandBuffer;
}

VkSemaphore createSemaphore(VkDevice device) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore");
    }
    return semaphore;
}

VkFence createFence(VkDevice device) {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence");
    }
    return fence;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type");
}

struct StorageImage {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
};

StorageImage createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat imageFormat) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    uint32_t memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }
    vkBindImageMemory(device, image, memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image view");
    }

    StorageImage storageImage{};
    storageImage.image = image;
    storageImage.memory = memory;
    storageImage.view = view;

    return storageImage;
}

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    return layout;
}


VkDescriptorPool createDescriptorPool(VkDevice device) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
    return pool;
}

VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    return set;
}

void updateDescriptorSet(VkDevice device, VkDescriptorSet set, VkImageView outputView, VkImageView accumView) {
    VkDescriptorImageInfo imageInfos[2]{};
    imageInfos[0].imageView = outputView;
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfos[1].imageView = accumView;
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write[2]{};
    write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[0].dstSet = set;
    write[0].dstBinding = 0;
    write[0].dstArrayElement = 0;
    write[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write[0].descriptorCount = 1;
    write[0].pImageInfo = &imageInfos[0];

    write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[1].dstSet = set;
    write[1].dstBinding = 1;
    write[1].dstArrayElement = 0;
    write[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write[1].descriptorCount = 1;
    write[1].pImageInfo = &imageInfos[1];

    vkUpdateDescriptorSets(device, 2, write, 0, nullptr);
}

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;


    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
    return layout;
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::string& filename) {
    std::vector<char> code = readFile(filename);

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

VkPipeline createPipeline(VkDevice device, VkShaderModule shaderModule, VkPipelineLayout pipelineLayout) {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline");
    }
    return pipeline;
}

VkQueryPool createQueryPool(VkDevice device) {
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryCount = 2;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;

    VkQueryPool queryPool = VK_NULL_HANDLE;
    if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create query pool");
    }
    return queryPool;
}


int main() {
    try {
        if (!glfwInit()) {
            std::cerr << "Installation failed\n";
            return 1;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Ray Tracer", nullptr, nullptr);
        if (window == nullptr) {
            std::cerr << "Failed to create window\n";
            glfwTerminate();
            return 1;
        }

        VkInstance instance = createInstance();

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        if (enableValidationLayers) {
            debugMessenger = setupDebugMessenger(instance);
        }

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }

        VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, surface);
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        float timestampPeriod = props.limits.timestampPeriod;
        std::cout << "GPU: " << props.deviceName << ", timestampPeriod: " << timestampPeriod << "ns\n";

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        VkDevice device = createLogicalDevice(physicalDevice, indices.compute.value());

        VkQueue computeQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, indices.compute.value(), 0, &computeQueue);
        VkQueue presentQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);

        Swapchain swapchain = createSwapchain(device, physicalDevice, surface, window, indices);

        StorageImage storageImage = createStorageImage(device, physicalDevice, swapchain.extent, VK_FORMAT_R8G8B8A8_UNORM);
        StorageImage accumImage = createStorageImage(device, physicalDevice, swapchain.extent, VK_FORMAT_R32G32B32A32_SFLOAT);

        VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device);
        VkDescriptorPool descriptorPool = createDescriptorPool(device);
        VkDescriptorSet descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);
        updateDescriptorSet(device, descriptorSet, storageImage.view, accumImage.view);

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
        VkShaderModule shaderModule = createShaderModule(device, "build/raytrace.comp.spv");
        VkPipeline computePipeline = createPipeline(device, shaderModule, pipelineLayout);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        VkCommandPool commandPool = createCommandPool(device, indices.compute.value());
        VkCommandBuffer commandBuffer =  createCommandBuffer(device, commandPool);
        VkQueryPool queryPool = createQueryPool(device);

        VkSemaphore imageAvailable = createSemaphore(device);
        std::vector<VkSemaphore> renderFinished(swapchain.images.size());
        for (size_t i = 0; i < renderFinished.size(); i++) {
            renderFinished[i] = createSemaphore(device);
        }
        VkFence inFlight = createFence(device);

        VkImageLayout storageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout accumLayout = VK_IMAGE_LAYOUT_UNDEFINED;


        uint32_t frameCount = 0;
        uint64_t results[2];
        const uint32_t WARMUP_FRAMES = 100;
        const uint32_t MEASURE_FRAMES = 1000;
        double totalMs = 0.0, minMs = 1e9, maxMs = 0.0;
        uint32_t measured = 0;

        const uint32_t width = swapchain.extent.width;
        const uint32_t height = swapchain.extent.height;
        std::cout << "Resolution: " << width << "x" << height << "\n";
        
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);
            if (frameCount != 0) {
                VkResult r = vkGetQueryPoolResults(device, queryPool, 0, 2, sizeof(results), results, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
                if (r != VK_SUCCESS) {
                    std::cerr << "query read failed: " << r << "\n";
                } else if (frameCount -1 >= WARMUP_FRAMES) {
                double ms = (results[1] - results[0]) * timestampPeriod / 1e6;
                totalMs += ms;
                minMs = std::min(minMs, ms);
                maxMs = std::max(maxMs, ms);
                measured++;

                if (measured == MEASURE_FRAMES) {
                    double avgMs = totalMs / measured;
                    double pathsPerSec = double(width) * height / (avgMs / 1000.0);
                    std::cout << "GPU: " << props.deviceName << "\n"
                    << "Frames: " << measured << " (1 spp each, max 8 bounces)\n"
                    << "Dispatch avg " << avgMs << " ms, min" << minMs << " ms, max" << maxMs << " ms\n"
                    << "Mpaths/s: " << pathsPerSec / 1e6 << "\n";
                    break;
                }
                }
            }
            vkResetFences(device, 1, &inFlight);
            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);
            vkResetCommandBuffer(commandBuffer, 0);
            
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            vkCmdResetQueryPool(commandBuffer, queryPool, 0,2);

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            
            barrier.image = storageImage.image;
            barrier.oldLayout = storageLayout;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);
            
            barrier.image = accumImage.image;
            barrier.oldLayout = accumLayout;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;     

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &frameCount);
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
            vkCmdDispatch(commandBuffer, (swapchain.extent.width + 7) / 8, (swapchain.extent.height + 7) / 8, 1);
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

            barrier.image = storageImage.image;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);
            
            barrier.image = swapchain.images[imageIndex];
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            storageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            accumLayout = VK_IMAGE_LAYOUT_GENERAL;
            
            
            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.extent.width = swapchain.extent.width;
            copyRegion.extent.height = swapchain.extent.height;
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(commandBuffer, storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            barrier.image = swapchain.images[imageIndex];
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            vkEndCommandBuffer(commandBuffer);

            VkSemaphore waitSemaphores[] = { imageAvailable };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
            VkSemaphore signalSemaphores[] = { renderFinished[imageIndex] };


            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;
            vkQueueSubmit(computeQueue, 1, &submitInfo, inFlight);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain.handle;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(presentQueue, &presentInfo);
            frameCount++;
        }

        vkDeviceWaitIdle(device);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);


        vkDestroyImageView(device, storageImage.view, nullptr);
        vkDestroyImage(device, storageImage.image, nullptr);
        vkFreeMemory(device, storageImage.memory, nullptr);
        vkDestroyImageView(device, accumImage.view, nullptr);
        vkDestroyImage(device, accumImage.image, nullptr);
        vkFreeMemory(device, accumImage.memory, nullptr);


        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        vkDestroySemaphore(device, imageAvailable, nullptr);
        for (VkSemaphore semaphore : renderFinished) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyFence(device, inFlight, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);

        if (enableValidationLayers) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(instance, debugMessenger, nullptr);
            }
        }

        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

