//
// Created by ghima on 18-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_TEMPORALHISTORYTWOIMG_H
#define REALTIMEFRAMEDISPLAY_TEMPORALHISTORYTWOIMG_H

#include "Util.h"

namespace fd {
    class TemporalHistoryTwoImg {
    private:
        RenderContext *m_ctx;
        const char *m_shader_path;
        uint32_t m_width;
        uint32_t m_height;
        static uint32_t m_frame_count;
        std::vector<VkImage> m_history{3, VK_NULL_HANDLE};
        VkImage m_img_one{};
        VkImageView m_img_one_view{};
        VkDeviceMemory m_img_one_memory{};
        VkImage m_img_two{};
        VkImageView m_img_two_view{};
        VkDeviceMemory m_img_two_memory{};
        VkImage m_img_three{};
        VkImageView  m_img_three_view {};
        VkDeviceMemory m_img_three_memory{};
        VkImage m_img_out{};
        VkImageView m_img_out_view{};
        VkDeviceMemory m_img_out_memory{};

        uint32_t m_motion_vector_buffer_size;
        VkBuffer m_motion_vectors_buffer{};
        VkDeviceMemory m_motion_vectors_buffer_memory {};

        VkPipeline m_pipeline{};
        VkPipelineLayout m_pipeline_layout{};
        VkDescriptorSetLayout m_des_layout{};
        VkDescriptorSet m_des_set{};
        VkDescriptorPool m_des_pool{};

        VkSampler m_sampler_one{};
        VkSampler m_sampler_two{};
        VkSampler m_sampler_out{};
        VkSampler m_sampler_three{};

        void setup_images_and_history();

        void setup_descriptors();

        void create_pipeline();

        bool isFirstRender = true;
    public:
        TemporalHistoryTwoImg(RenderContext *ctx, const char *shaderPath, uint32_t width, uint32_t height);

        void compute(VkCommandBuffer commandBuffer, VkImage &r8Image);
        void clean_up();
    };
}
#endif //REALTIMEFRAMEDISPLAY_TEMPORALHISTORYTWOIMG_H

