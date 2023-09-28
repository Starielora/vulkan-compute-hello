#include <vulkan/vulkan_raii.hpp>
#include <shaderc/shaderc.hpp>

#include <iostream>
#include <string>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

int main()
{
    const auto app_info = vk::ApplicationInfo("Vulkan compute hello", 0, nullptr, 0, VK_API_VERSION_1_3);
    const auto layers = std::array{ "VK_LAYER_KHRONOS_validation" };
    const auto extensions = std::array{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    const auto enabled_features = std::array{ vk::ValidationFeatureEnableEXT::eDebugPrintf };
    const auto features = vk::ValidationFeaturesEXT(enabled_features);
    const auto context = vk::raii::Context{};
    const auto instance = vk::raii::Instance(context, { {}, &app_info, layers, extensions, &features });
    const auto severity_flags = vk::DebugUtilsMessageSeverityFlagsEXT(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose);
    const auto message_type_flags = vk::DebugUtilsMessageTypeFlagsEXT(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    const auto debug_messenger = instance.createDebugUtilsMessengerEXT({ {}, severity_flags, message_type_flags, &debugCallback });
    const auto physical_devices = instance.enumeratePhysicalDevices();
    const auto& physical_device = physical_devices.at(0);

    const auto compute_family_index = [&]()
    {
        const auto queue_props = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; const auto& props : queue_props)
        {
            if (props.queueFlags & vk::QueueFlagBits::eCompute)
            {
                return i;
            }
            ++i;
        }
        throw std::runtime_error("Could not find compute queue"); // should not happen because at least one queue must be compute
    }();

    const auto compute_queue_create_info = vk::DeviceQueueCreateInfo({}, compute_family_index, 1.0f);

    const auto logical_device = physical_device.createDevice({ {}, compute_queue_create_info });
    const auto print_shader = std::string(R"(
        #version 460
        #extension GL_EXT_debug_printf : require
        void main()
        { debugPrintfEXT("Hello world, thread %d\n", gl_GlobalInvocationID.x); }
    )");
    const auto compiled_shader = shaderc::Compiler().CompileGlslToSpv(print_shader, shaderc_compute_shader, "hello_world.comp");
    const auto spirv = std::vector<uint32_t>(compiled_shader.cbegin(), compiled_shader.cend());
    const auto shader_module = logical_device.createShaderModule({ {}, spirv });
    const auto pipeline_layout = logical_device.createPipelineLayout({});
    const auto pipeline_create_info = vk::ComputePipelineCreateInfo({}, {{}, vk::ShaderStageFlagBits::eCompute, *shader_module, "main"}, *pipeline_layout);
    const auto pipeline = logical_device.createComputePipeline(logical_device.createPipelineCache({}), pipeline_create_info);
    const auto command_pool = logical_device.createCommandPool({ {}, compute_family_index });
    const auto allocate_info = vk::CommandBufferAllocateInfo(*command_pool, vk::CommandBufferLevel::ePrimary, 1);
    const auto command_buffers = logical_device.allocateCommandBuffers(allocate_info);
    const auto& command_buffer = command_buffers[0];
    command_buffer.begin(vk::CommandBufferBeginInfo{});
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    command_buffer.dispatch(8, 1, 1);
    command_buffer.end();
    logical_device.getQueue(compute_family_index, 0).submit(vk::SubmitInfo({}, {}, *command_buffer));
    logical_device.waitIdle();
}
