//
// Created by ghima on 07-01-2026.
//
#define GLFW_INCLUDE_VULKAN

#include <glfw/glfw3.h>
#include <set>
#include <array>
#include <iostream>
#include "VulkanGraphics.h"
#include "Util.h"
#include "FrameGeneratorTwo.h"

__declspec(dllimport) void print_simple_message_two(const char *val);

namespace fd {
    VulkanGraphics::VulkanGraphics(GLFWwindow *window) : m_window{window} {
        print_simple_message_two("Hello world");
        init();
    }

    VulkanGraphics::~VulkanGraphics() {
        vkDeviceWaitIdle(m_device.logicalDevice);
        FrameHandler::get_instance(m_ctx, 0, 0)->cleanup();
        m_computeYuvRgba->clean_up();
        delete m_fmGenerator;
        delete m_ctx;
        delete m_computeYuvRgba;
        vkDestroyBuffer(m_device.logicalDevice, quadVertBuffer, nullptr);
        vkFreeMemory(m_device.logicalDevice, vertBufferMemory, nullptr);
        vkDestroyBuffer(m_device.logicalDevice, quadIndexBuffer, nullptr);
        vkFreeMemory(m_device.logicalDevice, indexBufferMemory, nullptr);
        vkDestroySemaphore(m_device.logicalDevice, m_render_image_semaphore, nullptr);
        vkDestroySemaphore(m_device.logicalDevice, m_get_image_semaphore, nullptr);
        vkDestroyFence(m_device.logicalDevice, m_render_fence, nullptr);
        vkDestroyCommandPool(m_device.logicalDevice, m_command_pool, nullptr);
        vkDestroyPipeline(m_device.logicalDevice, m_graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device.logicalDevice, m_graphics_layout, nullptr);

        for (int i = 0; i < m_image_count; i++) {
            vkDestroyFramebuffer(m_device.logicalDevice, m_frame_buffers[i], nullptr);
            vkDestroyImageView(m_device.logicalDevice, m_image_views[i], nullptr);
        }
        vkDestroyRenderPass(m_device.logicalDevice, m_render_pass, nullptr);
        vkDestroySwapchainKHR(m_device.logicalDevice, m_swap_chain, nullptr);
        vkDestroyDevice(m_device.logicalDevice, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

    }

    void VulkanGraphics::init() {
        create_instance();
        get_physical_device_and_create_logical_device();
        create_swapchain();
        create_render_pass();
        create_frame_buffers();
        create_command_pool_and_allocate_buffer();
        create_semaphore_and_fences();
        m_ctx = new RenderContext{};
        m_ctx->physicalDevice = m_device.physicalDevice;
        m_ctx->logicalDevice = m_device.logicalDevice;
        m_ctx->imageCount = m_image_count;
        m_ctx->commandPool = m_command_pool;
        m_ctx->graphicsQueue = m_graphics_queue;
        m_ctx->computeQueue = m_compute_queue;
        m_ctx->graphicsQueueIndex = m_queue_family_index.graphicsIndex.value();
        m_ctx->computeQueueIndex = m_queue_family_index.computeIndex.value();
        prepare_quad_display();
        m_fmGenerator = new FrameGeneratorTwo();
        m_fmGenerator->process("D:\\vid.mp4");
        {
            std::unique_lock<std::mutex> lock{m_fmGenerator->get_vid_mutex()};
            m_fmGenerator->get_vid_cv().wait(lock, [this]() -> bool { return m_fmGenerator->is_generator_ready(); });
            FrameHandler::get_instance(m_ctx, m_fmGenerator->get_vid_frame_width(),
                                       m_fmGenerator->get_vid_frame_height());
            m_computeYuvRgba = new ComputeYuvRgba(m_ctx,
                                                  R"(D:\cProjects\realTimeFrameDisplay\shaders\yuvRgba.comp.spv)",
                                                  m_fmGenerator->get_vid_frame_width(),
                                                  m_fmGenerator->get_vid_frame_height());
        }

        create_pipeline();
    }

    void VulkanGraphics::create_instance() {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Real Time frame";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "Real Time frame Engine";
        applicationInfo.apiVersion = VK_API_VERSION_1_1;

        std::vector<const char *> windowExtensions{};
        get_window_required_instance_extensions(windowExtensions);
        windowExtensions.push_back("VK_EXT_debug_utils");
        std::vector<const char *> requiredLayers = {"VK_LAYER_KHRONOS_validation"};
        VkDebugUtilsMessengerCreateInfoEXT messenger = create_debug_messenger();

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledExtensionCount = windowExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = windowExtensions.data();
        instanceCreateInfo.enabledLayerCount = requiredLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
        instanceCreateInfo.pNext = &messenger;

        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance), "Failed to create the instance");
        LOG_INFO("Vulkan Instance Created Successfully");
    }

    void VulkanGraphics::get_window_required_instance_extensions(std::vector<const char *> &instanceExtensions) {
        std::uint32_t count = 0;
        const char **ext = glfwGetRequiredInstanceExtensions(&count);
        for (int i = 0; i < count; i++) {
            instanceExtensions.push_back(ext[i]);
        }
    }

    static VKAPI_ATTR VkBool32

    VKAPI_CALL ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callback,
                                  void *userData) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_ERROR("VALIDATION_ERROR : {}", callback->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_WARN("VALIDATION_WARNING : {}", callback->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            LOG_INFO("VALIDATION_INFO : {}", callback->pMessage);
        }

        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT VulkanGraphics::create_debug_messenger() {
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfoExt{};
        messengerCreateInfoExt.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerCreateInfoExt.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        messengerCreateInfoExt.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        messengerCreateInfoExt.pfnUserCallback = ValidationCallback;
        return messengerCreateInfoExt;
    }

#pragma region DEVICES

    void VulkanGraphics::get_physical_device_and_create_logical_device() {
        glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
        std::vector<VkPhysicalDevice> devices{};
        get_physical_devices(devices);

        bool deviceFound = false;
        for (VkPhysicalDevice &device: devices) {
            if (is_device_suitable(device)) {
                m_device.physicalDevice = device;
                deviceFound = true;
                break;
            }
        }
        if (!deviceFound) {
            LOG_ERROR("No Suitable Device Found");
            std::exit(EXIT_FAILURE);
        }
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_device.physicalDevice, &properties);
        LOG_INFO("Device Found {}, {}", properties.deviceName, properties.deviceID);
        create_logical_device(m_device.physicalDevice);

        LOG_INFO("Device Configuration Successful");
    }

    void VulkanGraphics::get_physical_devices(std::vector<VkPhysicalDevice> &devices) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        devices.resize(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    }

    bool VulkanGraphics::is_device_suitable(VkPhysicalDevice &device) {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilyProperties.data());
        std::vector<VkQueueFamilyProperties>::iterator iter = std::find_if(queueFamilyProperties.begin(),
                                                                           queueFamilyProperties.end(),
                                                                           [](VkQueueFamilyProperties pr) -> bool {
                                                                               return (pr.queueFlags &
                                                                                       (VK_QUEUE_GRAPHICS_BIT |
                                                                                        VK_QUEUE_TRANSFER_BIT));
                                                                           });
        if (iter == queueFamilyProperties.end()) {
            LOG_ERROR("No valid queue found for graphics display in device");
            std::exit(EXIT_FAILURE);
        }
        m_queue_family_index.graphicsIndex = iter - queueFamilyProperties.begin();
        std::vector<VkQueueFamilyProperties>::iterator iterComp = std::find_if(queueFamilyProperties.begin(),
                                                                               queueFamilyProperties.end(),
                                                                               [](VkQueueFamilyProperties pr) -> bool {
                                                                                   return pr.queueFlags &
                                                                                          VK_QUEUE_COMPUTE_BIT;
                                                                               });
        if (iterComp == queueFamilyProperties.end()) {
            LOG_INFO("No valid compute queue found");
        } else {
            m_queue_family_index.computeIndex = iter - queueFamilyProperties.begin();
        }

        LOG_INFO("Graphics Queue {} Compute Queue {}", m_queue_family_index.graphicsIndex.value(),
                 m_queue_family_index.computeIndex.value());
        for (int i = 0; i < queueFamilyProperties.size(); i++) {
            VkBool32 hasPresentationQueue = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &hasPresentationQueue);
            if (hasPresentationQueue) {
                m_queue_family_index.presentationIndex = i;
                break;
            }
        }
        return m_queue_family_index.is_valid();
    }

    void VulkanGraphics::create_logical_device(VkPhysicalDevice &device) {

        float priority = 1.0f;
        std::set<uint32_t> indexes{m_queue_family_index.graphicsIndex.value(),
                                   m_queue_family_index.presentationIndex.value(),
                                   m_queue_family_index.computeIndex.value()};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
        for (std::uint32_t i: indexes) {
            VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
            deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            deviceQueueCreateInfo.queueFamilyIndex = i;
            deviceQueueCreateInfo.queueCount = 1;
            deviceQueueCreateInfo.pQueuePriorities = &priority;
            queueCreateInfos.push_back(deviceQueueCreateInfo);
        }


        std::vector<const char *> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.enabledExtensionCount = requiredExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

        VK_CHECK(vkCreateDevice(device, &deviceCreateInfo, nullptr, &m_device.logicalDevice),
                 "Failed to create the logical device");
        vkGetDeviceQueue(m_device.logicalDevice, m_queue_family_index.graphicsIndex.value(), 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device.logicalDevice, m_queue_family_index.presentationIndex.value(), 0,
                         &m_presentation_queue);
        vkGetDeviceQueue(m_device.logicalDevice, m_queue_family_index.computeIndex.value(), 0, &m_compute_queue);
    }

#pragma endregion
#pragma region SWAPCHAIN

    VkSurfaceFormatKHR VulkanGraphics::select_format() {
        std::vector<VkSurfaceFormatKHR> formats{};
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physicalDevice, m_surface, &count, nullptr);
        formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physicalDevice, m_surface, &count, formats.data());
        for (int i = 0; i < count; i++) {
            if (formats[i].format == VK_FORMAT_UNDEFINED) {
                return {VK_FORMAT_R8G8B8A8_SNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            }
        }
        return formats[0];
    }

    VkPresentModeKHR VulkanGraphics::select_present_mode() {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.physicalDevice, m_surface, &count, nullptr);
        std::vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.physicalDevice, m_surface, &count, modes.data());
        for (int i = 0; i < count; i++) {
            if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                return modes[i];
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    void VulkanGraphics::create_swapchain() {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.physicalDevice, m_surface,
                                                  &m_surface_capabilities);
        m_format = select_format();
        m_present_mode = select_present_mode();
        m_image_count = m_surface_capabilities.minImageCount + 1;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.imageFormat = m_format.format;
        createInfo.imageColorSpace = m_format.colorSpace;
        createInfo.presentMode = m_present_mode;
        createInfo.minImageCount = m_image_count;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.surface = m_surface;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageArrayLayers = 1;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = nullptr;
        createInfo.imageExtent = {WIN_WIDTH, WIN_HEIGHT};
        createInfo.preTransform = m_surface_capabilities.currentTransform;

        std::vector<uint32_t> queueFamilyIndices{m_queue_family_index.graphicsIndex.value(),
                                                 m_queue_family_index.presentationIndex.value()};

        if (m_queue_family_index.presentationIndex.value() != m_queue_family_index.graphicsIndex.value()) {
            std::array<uint32_t, 2> indices{m_queue_family_index.graphicsIndex.value(),
                                            m_queue_family_index.presentationIndex.value()};
            createInfo.queueFamilyIndexCount = indices.size();
            createInfo.pQueueFamilyIndices = indices.data();
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VK_CHECK(vkCreateSwapchainKHR(m_device.logicalDevice, &createInfo, nullptr, &m_swap_chain),
                 "Failed to get the swapchain images");
        // Creating the image view from the images in the swapchain.
        m_images.resize(m_image_count);
        vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swap_chain, &m_image_count, m_images.data());
        m_image_views.resize(m_image_count);
        for (int i = 0; i < m_image_count; i++) {
            create_image_view(m_device.logicalDevice, m_images[i], m_image_views[i], m_format.format);
        }
        LOG_INFO("Swapchain Configured Successfully");
    }

#pragma endregion

#pragma region PIPELINE

    void VulkanGraphics::create_frame_buffers() {
        m_frame_buffers.resize(m_image_count);
        for (int i = 0; i < m_image_count; i++) {
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = m_render_pass;
            createInfo.width = WIN_WIDTH;
            createInfo.height = WIN_HEIGHT;
            createInfo.renderPass = m_render_pass;
            createInfo.attachmentCount = 1;
            createInfo.layers = 1;
            createInfo.pAttachments = &m_image_views[i];

            VK_CHECK(vkCreateFramebuffer(m_device.logicalDevice, &createInfo, nullptr, &m_frame_buffers[i]),
                     "Failed to create the frame buffers");
        }
    }

    void VulkanGraphics::create_render_pass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_format.format;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference colorReference{};
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorReference.attachment = 0;

        VkSubpassDescription subpassOne{};
        subpassOne.colorAttachmentCount = 1;
        subpassOne.pColorAttachments = &colorReference;

        VkSubpassDependency dependencyOne{};
        dependencyOne.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencyOne.dstSubpass = 0;
        dependencyOne.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencyOne.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencyOne.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOne.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencyOne.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &colorAttachment;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassOne;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &dependencyOne;

        VK_CHECK(vkCreateRenderPass(m_device.logicalDevice, &renderPassCreateInfo, nullptr, &m_render_pass),
                 "Failed to create the render pass");

    }

    void VulkanGraphics::create_pipeline() {
        VkShaderModule vertexShaderModule = create_shader_module(m_device.logicalDevice,
                                                                 R"(D:\cProjects\realTimeFrameDisplay\shaders\default.vert.spv)");
        VkShaderModule fragShaderModule = create_shader_module(m_device.logicalDevice,
                                                               R"(D:\cProjects\realTimeFrameDisplay\shaders\default.frag.spv)");

        VkPipelineShaderStageCreateInfo vertexStage{};
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.module = vertexShaderModule;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShaderModule;
        fragStage.pName = "main";

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertexStage, fragStage};

        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
        inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;


        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
        rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;;
        rasterizationStateCreateInfo.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
        pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_ctx->desLayoutFrame;

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescription.stride = sizeof(Vertex);

        VkVertexInputAttributeDescription pos{};
        pos.binding = 0;
        pos.location = 0;
        pos.format = VK_FORMAT_R32G32B32_SFLOAT;
        pos.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription uv{};
        uv.binding = 0;
        uv.location = 1;
        uv.format = VK_FORMAT_R32G32_SFLOAT;
        uv.offset = offsetof(Vertex, uv);

        std::vector<VkVertexInputAttributeDescription> attributes{pos, uv};
        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributes.size();
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributes.data();

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        VkViewport viewport{0, 0, WIN_WIDTH, WIN_HEIGHT, 0, 1};
        VkRect2D scissors{0, 0, WIN_WIDTH, WIN_HEIGHT};
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &viewport;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &scissors;

        VkPipelineColorBlendAttachmentState blendOne{};
        blendOne.blendEnable = VK_FALSE;
        blendOne.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateCreateInfo.attachmentCount = 1;
        colorBlendStateCreateInfo.pAttachments = &blendOne;


        VK_CHECK(vkCreatePipelineLayout(m_device.logicalDevice, &layoutCreateInfo, nullptr, &m_graphics_layout),
                 "Failed to create the graphics pipeline layout");
        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.renderPass = m_render_pass;
        createInfo.subpass = 0;
        createInfo.layout = m_graphics_layout;
        createInfo.stageCount = shaderStages.size();
        createInfo.pStages = shaderStages.data();
        createInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        createInfo.pRasterizationState = &rasterizationStateCreateInfo;
        createInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
        createInfo.pDynamicState = &dynamicStateCreateInfo;
        createInfo.pVertexInputState = &vertexInputStateCreateInfo;
        createInfo.pViewportState = &viewportStateCreateInfo;
        createInfo.pColorBlendState = &colorBlendStateCreateInfo;

        VK_CHECK(vkCreateGraphicsPipelines(m_device.logicalDevice, nullptr, 1, &createInfo, nullptr,
                                           &m_graphics_pipeline), "Failed to create the graphics pipeline");

        vkDestroyShaderModule(m_device.logicalDevice, vertexShaderModule, nullptr);
        vkDestroyShaderModule(m_device.logicalDevice, fragShaderModule, nullptr);
    }

#pragma endregion
#pragma region RENDER

    void VulkanGraphics::create_command_pool_and_allocate_buffer() {
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = m_queue_family_index.graphicsIndex.value();
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_device.logicalDevice, &commandPoolCreateInfo, nullptr, &m_command_pool),
                 "Failed to create the command pool");
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_command_pool;
        allocateInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_device.logicalDevice, &allocateInfo, &m_command_buffer),
                 "failed to allocate the command buffer");

    }

    void VulkanGraphics::create_semaphore_and_fences() {
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_get_image_semaphore),
                 "failed to create the get image semaphore");
        VK_CHECK(vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_render_image_semaphore),
                 "failed to create the render image semaphore");

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(m_device.logicalDevice, &fenceCreateInfo, nullptr, &m_render_fence),
                 "failed to create the render fence");

    }

    void VulkanGraphics::begin_frame() {
        vkWaitForFences(m_device.logicalDevice, 1, &m_render_fence, VK_TRUE, UINT64_MAX);
        vkResetCommandBuffer(m_command_buffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &beginInfo), "Failed to begin the command buffer");

        VkViewport viewport{0, 0, WIN_WIDTH, WIN_HEIGHT, 0, 1};
        VkRect2D scissors{0, 0, WIN_WIDTH, WIN_HEIGHT};
        vkAcquireNextImageKHR(m_device.logicalDevice, m_swap_chain, UINT64_MAX, m_get_image_semaphore, nullptr,
                              &m_curr_image);
        vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
        vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(m_command_buffer, 0, 1, &scissors);

        // Starting the render pass.
        VkClearValue clearValue{};
        clearValue.color = {0.2, 0.2, 0.2, 1};
        VkOffset2D offset{};
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = m_render_pass;
        renderPassBeginInfo.renderArea = {offset, {static_cast<uint32_t>(viewport.width),
                                                   static_cast<uint32_t>(viewport.height)}};
        renderPassBeginInfo.framebuffer = m_frame_buffers[m_curr_image];
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(m_command_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanGraphics::draw() {
        VkDeviceSize offset{};
        vkCmdBindVertexBuffers(m_command_buffer, 0, 1, &quadVertBuffer, &offset);
        vkCmdBindIndexBuffer(m_command_buffer, quadIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_layout, 0, 1,
                                &m_ctx->desSetFrame[m_curr_image], 0,
                                nullptr);

        {
            m_fmGenerator->get_vid_mutex().lock();
            VideoFrame videoFrame = std::move(m_fmGenerator->get_vide_frame_queue().front());
            m_fmGenerator->notify_video_frame_processed();
            m_fmGenerator->get_vid_mutex().unlock();
            std::unique_ptr<uint8_t[]> yPlane = std::move(videoFrame.yPlane);
            std::unique_ptr<uint8_t[]> vPlane = std::move(videoFrame.vPlane);
            std::unique_ptr<uint8_t[]> uPlane = std::move(videoFrame.uPlane);
            m_computeYuvRgba->compute(yPlane.get(), uPlane.get(), vPlane.get());
            //Scalar RGBA conversion.
   //         uint32_t *rgba = new uint32_t[m_fmGenerator->get_vid_frame_width() * m_fmGenerator->get_vid_frame_height()];
//            yuv_to_rgba(m_fmGenerator->get_vid_frame_width(), m_fmGenerator->get_vid_frame_height(), yPlane.get(),
//                        vPlane.get(), uPlane.get(), rgba);
            FrameHandler::get_instance(m_ctx, 0, 0)->render_with_compute_image(m_computeYuvRgba->get_rgba_image(),
                                                                               m_computeYuvRgba->get_compute_semaphore());
            //FrameHandler::get_instance(m_ctx, 0, 0)->render(rgba);
            double pts = videoFrame.pts_seconds;
            double timePassed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - m_fmGenerator->frame_start).count();

            if (timePassed < pts) {
                std::this_thread::sleep_for(std::chrono::duration<double>(pts - timePassed));
            } else if (pts < timePassed) {
                LOG_INFO("Bad Frame");
            }
          //  delete[] rgba;

        }

        vkCmdDrawIndexed(m_command_buffer, 6, 1, 0, 0, 0);
    }

    void VulkanGraphics::end_frame() {
        vkCmdEndRenderPass(m_command_buffer);
        vkEndCommandBuffer(m_command_buffer);
        std::vector<VkPipelineStageFlags> waitFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
        std::array<VkSemaphore, 2> semaphores{m_get_image_semaphore, FrameHandler::get_instance(m_ctx, 0, 0)->get_frame_handler_semaphore()};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_command_buffer;
        submitInfo.waitSemaphoreCount = semaphores.size();
        submitInfo.pWaitSemaphores = semaphores.data();
        submitInfo.pWaitDstStageMask = waitFlags.data();
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_render_image_semaphore;
        vkResetFences(m_device.logicalDevice, 1, &m_render_fence);
        vkQueueSubmit(m_graphics_queue, 1, &submitInfo, m_render_fence);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_render_image_semaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swap_chain;
        presentInfo.pImageIndices = &m_curr_image;

        vkQueuePresentKHR(m_presentation_queue, &presentInfo);
    }

    void VulkanGraphics::prepare_quad_display() {
        std::vector<Vertex> vert{
                {{-1, -1, 0}, {0, 0}},
                {{-1, 1,  0}, {0, 1}},
                {{1,  1,  0}, {1, 1}},
                {{1,  -1, 0}, {1, 0}}
        };
        std::vector<std::uint32_t> indices{
                0, 1, 2,
                0, 2, 3
        };
        create_buffer(m_ctx, quadVertBufferStaging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, vertBufferMemoryStaging,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      sizeof(Vertex) * vert.size());
        create_buffer(m_ctx, quadIndexBufferStaging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, indexBufferMemoryStaging,
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                      sizeof(uint32_t) * indices.size());
        create_buffer(m_ctx, quadVertBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      vertBufferMemory,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      sizeof(Vertex) * vert.size());
        create_buffer(m_ctx, quadIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      indexBufferMemory,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      sizeof(uint32_t) * indices.size());


        void *data;
        vkMapMemory(m_device.logicalDevice, vertBufferMemoryStaging, 0, sizeof(Vertex) * vert.size(), 0, &data);
        memcpy(data, vert.data(), sizeof(Vertex) * vert.size());
        vkUnmapMemory(m_device.logicalDevice, vertBufferMemoryStaging);
        vkMapMemory(m_device.logicalDevice, indexBufferMemoryStaging, 0, sizeof(uint32_t) * indices.size(), 0, &data);
        memcpy(data, indices.data(), sizeof(uint32_t) * indices.size());
        vkUnmapMemory(m_device.logicalDevice, indexBufferMemoryStaging);

        buffer_to_buffer(m_ctx, m_command_buffer, quadVertBufferStaging, quadVertBuffer, sizeof(Vertex) * vert.size());
        buffer_to_buffer(m_ctx, m_command_buffer, quadIndexBufferStaging, quadIndexBuffer,
                         sizeof(uint32_t) * indices.size());

        vkDestroyBuffer(m_device.logicalDevice, quadVertBufferStaging, nullptr);
        vkFreeMemory(m_device.logicalDevice, vertBufferMemoryStaging, nullptr);
        vkDestroyBuffer(m_device.logicalDevice, quadIndexBufferStaging, nullptr);
        vkFreeMemory(m_device.logicalDevice, indexBufferMemoryStaging, nullptr);
    }

    void VulkanGraphics::render() {
        {
            std::unique_lock<std::mutex> lock{m_fmGenerator->get_vid_mutex()};
            m_fmGenerator->get_vid_cv().wait(lock,
                                             [this]() -> bool { return !m_fmGenerator->get_vide_frame_queue().empty(); });
        }
        begin_frame();
        draw();
        end_frame();
    }

    void VulkanGraphics::yuv_to_rgba(uint32_t width, uint32_t height, const uint8_t *yPlane, const uint8_t *vPlane,
                                     const uint8_t *uPlane,
                                     uint32_t *rgbaOut) {

        uint32_t chromaW = width >> 1;
        for (int y = 0; y < height; y++) {
            int chromaY = y >> 1;
            for (int x = 0; x < width; x++) {
                int chromaX = x >> 1;
                uint8_t yPix = yPlane[y * width + x];
                uint8_t vPix = vPlane[chromaY * chromaW + chromaX];
                uint8_t uPix = uPlane[chromaY * chromaW + chromaX];

                int C = yPix - 16;
                int D = uPix - 128;
                int E = vPix - 128;

                int R = (298 * C + 409 * E + 128) >> 8;
                int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
                int B = (298 * C + 516 * D + 128) >> 8;

                clamp(R);
                clamp(G);
                clamp(B);

                rgbaOut[y * width + x] = (255 << 24) | (B << 16) | (G << 8) | (R << 0);
            }
        }
    }

#pragma endregion
}