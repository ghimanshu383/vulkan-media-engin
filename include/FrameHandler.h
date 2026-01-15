//
// Created by ghima on 10-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_FRAMEHANDLER_H
#define REALTIMEFRAMEDISPLAY_FRAMEHANDLER_H

#include "Util.h"
#include <vulkan/vulkan.h>

namespace fd {
    class FrameHandler {
    public:
        static FrameHandler *get_instance(RenderContext *ctx, size_t width, size_t height);

    private:
        static FrameHandler *m_instance;

        FrameHandler(RenderContext *ctx, size_t width, size_t height);

        RenderContext *m_ctx;
        size_t m_width;
        size_t m_height;
        VkBuffer yPlaneBuffer{};
        VkDeviceMemory yPlaneBufferMemory{};
        VkImage yPlaneImage{};
        VkImageView yPlaneImageView{};
        VkDeviceMemory yPlaneImageMemory{};
        VkDescriptorSetLayout m_des_layout{};
        VkDescriptorPool m_des_pool{};
        std::vector<VkDescriptorSet> m_des_sets{};
        VkSampler m_sampler{};
        VkCommandBuffer m_commandBuffer{};
        bool isFirstRender = true;

        void create_buffer_and_images();

        void create_sampler();

        void create_descriptor_set_and_layout();

    public:

        void render(uint32_t *rgba);

        void render_with_compute_image(VkImage &rgbaImage, VkSemaphore& computeSemaphore);

        VkDescriptorSetLayout &get_des_layout() { return m_des_layout; }

        std::vector<VkDescriptorSet> &get_des_sets() { return m_des_sets; }

        void cleanup();

    };
}
#endif //REALTIMEFRAMEDISPLAY_FRAMEHANDLER_H
