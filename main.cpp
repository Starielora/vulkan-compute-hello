#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

#include <iostream>
#include <array>
#include <vector>

#define VK_CHECK(f) do{ const auto result = f; if(result != VK_SUCCESS){ throw std::runtime_error(""); } } while(0)

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

int main()
{
    const auto application_info = VkApplicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "vulkan-compute-sample",
        .applicationVersion = 69420,
        .pEngineName = "lmaokek",
        .engineVersion = 69420,
        .apiVersion = VK_API_VERSION_1_3
    };

    const auto layers = std::array{ "VK_LAYER_KHRONOS_validation" };
    const auto extensions = std::array{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    const auto enabled_features = std::array{ VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
    const auto features = VkValidationFeaturesEXT{
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = nullptr,
        .enabledValidationFeatureCount = enabled_features.size(),
        .pEnabledValidationFeatures = enabled_features.data(),
        .disabledValidationFeatureCount = 0,
        .pDisabledValidationFeatures = nullptr
    };

    const auto instance_create_info = VkInstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &features,
        .flags = {},
        .pApplicationInfo = &application_info,
        .enabledLayerCount = layers.size(),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = extensions.size(),
        .ppEnabledExtensionNames = extensions.data()
    };

    auto instance = VkInstance{};
    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &instance));

    const auto debug_utils_messenger_create_info = VkDebugUtilsMessengerCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = {},
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debugCallback,
        .pUserData = nullptr
    };

    auto debug_utils_messenger = VkDebugUtilsMessengerEXT{};
    VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &debug_utils_messenger_create_info, nullptr, &debug_utils_messenger));

    auto physical_devices_count = 0u;
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, nullptr);
    auto physical_devices = std::vector<VkPhysicalDevice>(physical_devices_count);
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data());
    const auto& physical_device = physical_devices.at(0);

    const auto compute_family_index = [&]()
    {
        auto queue_property_count = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_property_count, nullptr);
        auto queue_props = std::vector<VkQueueFamilyProperties>(queue_property_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_property_count, queue_props.data());
        for (uint32_t i = 0; const auto & props : queue_props)
        {
            if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                return i;
            }
            ++i;
        }
        throw std::runtime_error("Could not find compute queue"); // should not happen because at least one queue must be compute
    }();

    const auto prio = std::array{ 1.0f };
    const auto queue_create_info = VkDeviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .queueFamilyIndex = compute_family_index,
        .queueCount = 1,
        .pQueuePriorities = prio.data()
    };

    const auto device_create_info = VkDeviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
        .pEnabledFeatures = nullptr
    };

    auto logical_device = VkDevice{};
    VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &logical_device));

    const auto print_shader = std::string(R"(
        #version 460
        #extension GL_EXT_debug_printf : require
        void main()
        { debugPrintfEXT("Hello world, thread %d\n", gl_GlobalInvocationID.x); }
    )");
    const auto compiled_shader = shaderc::Compiler().CompileGlslToSpv(print_shader, shaderc_compute_shader, "hello_world.comp");
    const auto spirv = std::vector<uint32_t>(compiled_shader.cbegin(), compiled_shader.cend());
    const auto shader_module_create_info = VkShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .codeSize = spirv.size() * sizeof(decltype(spirv)::value_type),
        .pCode = spirv.data()
    };

    auto shader_module = VkShaderModule{};
    VK_CHECK(vkCreateShaderModule(logical_device, &shader_module_create_info, nullptr, &shader_module));

    const auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    auto pipeline_layout = VkPipelineLayout{};
    VK_CHECK(vkCreatePipelineLayout(logical_device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

    const auto pipeline_create_info = VkComputePipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .stage = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = {},
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };

    auto pipeline = VkPipeline{};
    VK_CHECK(vkCreateComputePipelines(logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

    const auto command_pool_create_info = VkCommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .queueFamilyIndex = compute_family_index
    };

    auto command_pool = VkCommandPool{};
    VK_CHECK(vkCreateCommandPool(logical_device, &command_pool_create_info, nullptr, &command_pool));

    const auto allocate_info = VkCommandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    auto command_buffer = VkCommandBuffer{};
    VK_CHECK(vkAllocateCommandBuffers(logical_device, &allocate_info, &command_buffer));

    const auto begin_info = VkCommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = {},
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdDispatch(command_buffer, 8, 1, 1);

    VK_CHECK(vkEndCommandBuffer(command_buffer));

    auto queue = VkQueue{};
    vkGetDeviceQueue(logical_device, compute_family_index, 0, &queue);

    const auto submit_info = VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = {},
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkDeviceWaitIdle(logical_device));

    vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
    vkDestroyCommandPool(logical_device, command_pool, nullptr);
    vkDestroyPipeline(logical_device, pipeline, nullptr);
    vkDestroyPipelineLayout(logical_device, pipeline_layout, nullptr);
    vkDestroyShaderModule(logical_device, shader_module, nullptr);
    vkDestroyDevice(logical_device, nullptr);
    DestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
    vkDestroyInstance(instance, nullptr);
}
