//
// Created by ghima on 07-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_VULKANGRAPHICS_H
#define REALTIMEFRAMEDISPLAY_VULKANGRAPHICS_H

#include <vulkan/vulkan.h>
#include <vector>
#include <condition_variable>
#include <optional>
#include "FrameHandler.h"
#include "FrameGeneratorTwo.h"

namespace fd {
    class VulkanGraphics {
    private:
        GLFWwindow *m_window = nullptr;
        struct RenderContext *m_ctx;

        RenderContext *get_context() { return m_ctx; }

        void init();
        FrameGeneratorTwo* m_fmGenerator = nullptr;
        std::condition_variable m_cv_graphics;
        std::mutex _mutex;

#pragma region INSTANCE_AND_VALIDATION
        VkInstance m_instance{};

        void create_instance();

        void get_window_required_instance_extensions(std::vector<const char *> &instanceExtensions);

        VkDebugUtilsMessengerCreateInfoEXT create_debug_messenger();

#pragma endregion
#pragma region DEVICES
        struct Devices {
            VkPhysicalDevice physicalDevice{};
            VkDevice logicalDevice{};
        } m_device{};

        struct QueueFamilyIndex {
            std::optional<uint32_t> graphicsIndex{};
            std::optional<uint32_t> presentationIndex{};

            bool is_valid() {
                return (graphicsIndex.has_value() && presentationIndex.has_value());
            }
        } m_queue_family_index;

        VkSurfaceKHR m_surface{};
        VkQueue m_graphics_queue{};
        VkQueue m_presentation_queue{};

        void get_physical_device_and_create_logical_device();

        void get_physical_devices(std::vector<VkPhysicalDevice> &devices);

        bool is_device_suitable(VkPhysicalDevice &device);

        void create_logical_device(VkPhysicalDevice &device);

#pragma endregion
#pragma region SWAPCHAIN
        VkSwapchainKHR m_swap_chain{};
        std::vector<VkImage> m_images{};
        std::vector<VkImageView> m_image_views{};
        VkSurfaceCapabilitiesKHR m_surface_capabilities{};
        VkSurfaceFormatKHR m_format{};
        VkPresentModeKHR m_present_mode{};
        uint32_t m_image_count;

        VkSurfaceFormatKHR select_format();

        VkPresentModeKHR select_present_mode();

        void create_swapchain();

#pragma endregion
#pragma region PIPELINE
        VkPipeline m_graphics_pipeline{};
        VkPipelineLayout m_graphics_layout{};
        VkRenderPass m_render_pass{};
        std::vector<VkFramebuffer> m_frame_buffers{};

        void create_render_pass();

        void create_frame_buffers();

        void create_pipeline();

#pragma endregion
#pragma region RENDER
        uint32_t m_curr_image{};
        VkCommandBuffer m_command_buffer{};
        VkCommandPool m_command_pool{};
        VkFence m_render_fence{};
        VkSemaphore m_get_image_semaphore{};
        VkSemaphore m_render_image_semaphore{};
        VkBuffer quadVertBuffer{};
        VkBuffer quadVertBufferStaging{};
        VkDeviceMemory vertBufferMemory{};
        VkDeviceMemory vertBufferMemoryStaging{};
        VkBuffer quadIndexBuffer{};
        VkBuffer quadIndexBufferStaging{};
        VkDeviceMemory indexBufferMemory{};
        VkDeviceMemory indexBufferMemoryStaging{};

        void create_command_pool_and_allocate_buffer();

        void create_semaphore_and_fences();

        void prepare_quad_display();

        void begin_frame();

        void draw();

        void end_frame();

        void yuv_to_rgba(uint32_t width, uint32_t height, const uint8_t* yPlane, const uint8_t* vPlane, const uint8_t* uPlane, uint32_t* rgbaOut);

#pragma endregion
    public:
        explicit VulkanGraphics(GLFWwindow *window);

        void render();

        ~VulkanGraphics();
    };
}
#endif //REALTIMEFRAMEDISPLAY_VULKANGRAPHICS_H
