//
// Created by ghima on 16-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_VULKANFILTERR8IMAGE_H
#define REALTIMEFRAMEDISPLAY_VULKANFILTERR8IMAGE_H

#include "Util.h"

namespace fd {
    class VulkanFilterR8 {
    private:

        RenderContext *m_ctx;
        uint32_t m_width;
        uint32_t m_height;
        const char *m_compute_path{};
        bool isFirstRender = true;
        VkImage m_image_in{};
        VkImageView m_image_view_in{};
        VkDeviceMemory m_image_in_memory{};
        VkImage m_image_out{};
        VkImageView m_image_view_out{};
        VkDeviceMemory m_image_out_memory{};
        VkSampler m_image_in_sampler{};
        VkSampler m_image_out_sampler{};

        VkCommandBuffer m_commandBuffer{};
        VkSemaphore m_compute_semaphore{};

        VkPipelineLayout m_pipeline_layout{};
        VkPipeline m_pipeline{};
        VkDescriptorSetLayout m_des_layout{};
        VkDescriptorPool m_des_pool{};
        VkDescriptorSet m_des_set{};

        void create_images_and_image_views();

        void create_pipeline();

        void setup_descriptors();

    public:
        VulkanFilterR8(RenderContext *ctx, const char *filterComputePath, uint32_t width, uint32_t height);

        void compute(VkCommandBuffer commandBuffer, VkImage &r8Image);

        void cleanup();

        VkImage &get_filter_image() { return m_image_out; };

    };
}
#endif //REALTIMEFRAMEDISPLAY_VULKANFILTERR8IMAGE_H
