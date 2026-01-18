//
// Created by ghima on 15-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_VULKANYUVTORGBA_H
#define REALTIMEFRAMEDISPLAY_VULKANYUVTORGBA_H

#include <vulkan/vulkan.h>
#include "Util.h"
#include "computes/VulkanFilterR8Image.h"
#include "computes/TemporalHistoryTwoImg.h"

namespace fd {
    class ComputeYuvRgba {
    private:
        const char *m_shader_path{};
        RenderContext *m_ctx = nullptr;
        uint32_t m_width;
        uint32_t m_height;
        VkBuffer m_y_plane_buffer{};
        VkDeviceMemory m_y_plane_buffer_memory{};
        VkBuffer m_u_plane_buffer{};
        VkDeviceMemory m_u_plane_buffer_memory{};
        VkBuffer m_v_plane_buffer{};
        VkDeviceMemory m_v_plane_buffer_memory{};
        VkImage m_y_image{};
        VkImageView m_y_image_view{};
        VkDeviceMemory m_y_image_memory{};
        VkImage m_u_image{};
        VkImageView m_u_image_view{};
        VkDeviceMemory m_u_image_memory{};
        VkImage m_v_image{};
        VkImageView m_v_image_view{};
        VkDeviceMemory m_v_image_memory{};
        VkImage m_rgba_image{};
        VkImageView m_rgba_image_view{};
        VkDeviceMemory m_rgba_image_memory{};


        VkPipeline m_pipeline{};
        VkPipelineLayout m_pipeline_layout{};
        VkDescriptorSetLayout m_des_layout{};
        VkDescriptorPool m_des_pool{};
        VkDescriptorSet m_des_set{};
        VkSampler m_sampler_y{};
        VkSampler m_sampler_u{};
        VkSampler m_sampler_v{};
        VkSampler m_sampler_rgba{};

        VkCommandBuffer m_commandBuffer{};
        VkCommandBuffer m_commandBuffer_dispatch{};
        VkCommandBuffer m_compute_command_buffer{};
        VkCommandPool m_compute_command_pool {};

        VkSemaphore m_compute_semaphore{};
        VkSemaphore m_filter_semaphore{};

        void *yData;
        void *uData;
        void *vData;
        bool firstRender = true;

        VulkanFilterR8* m_blur = nullptr;
        TemporalHistoryTwoImg* m_temp = nullptr;

        void set_up_compute_command_buffer();
        void prepare_buffers_and_images();

        void create_pipeline();

        void setup_descriptors();

        void create_samplers();

        void dispatch();

        void invoke_r8_filters();


    public:
        ComputeYuvRgba(RenderContext *ctx, const char *shaderPath, uint32_t width, uint32_t height);

        void compute(uint8_t *yPlane, uint8_t *uPlane, uint8_t *vPlane);

        VkSemaphore &get_compute_semaphore() { return m_compute_semaphore; };

        VkImage &get_rgba_image() { return m_rgba_image; }
        VkImage &get_y_image() { return m_y_image; }
        void clean_up();
    };
}
#endif //REALTIMEFRAMEDISPLAY_VULKANYUVTORGBA_H
