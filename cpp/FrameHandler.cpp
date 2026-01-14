//
// Created by ghima on 10-01-2026.
//
#include "FrameHandler.h"
#include "Util.h"

namespace fd {
    FrameHandler *FrameHandler::m_instance = nullptr;

    FrameHandler *FrameHandler::get_instance(fd::RenderContext *ctx, size_t width, size_t height) {
        if (m_instance == nullptr) {
            m_instance = new FrameHandler(ctx, width, height);
        }
        return m_instance;
    }

    FrameHandler::FrameHandler(RenderContext *ctx, size_t width, size_t height) :
            m_ctx{ctx},
            m_width{width},
            m_height{height} {
        m_commandBuffer = start_command_buffer(m_ctx);
        create_buffer_and_images();
        create_sampler();
        create_descriptor_set_and_layout();
        ctx->desLayoutFrame = m_des_layout;
        for (VkDescriptorSet &des: m_des_sets) {
            ctx->desSetFrame.push_back(des);
        }
    };

    void FrameHandler::create_buffer_and_images() {
        create_buffer(m_ctx, yPlaneBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, yPlaneBufferMemory,
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                      m_width * m_height * sizeof (uint32_t));
        create_image(m_ctx, yPlaneImage, m_width, m_height, yPlaneImageMemory, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, yPlaneImage, yPlaneImageView, VK_FORMAT_R8G8B8A8_UNORM);
        transition_image_layout(m_ctx, m_commandBuffer, yPlaneImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    void FrameHandler::create_sampler() {
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VK_CHECK(vkCreateSampler(m_ctx->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler),
                 "failed to create the sampler");
    }

    void FrameHandler::create_descriptor_set_and_layout() {
        VkDescriptorSetLayoutBinding imageBinding{};
        imageBinding.binding = 0;
        imageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageBinding.descriptorCount = 1;
        imageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo desLayoutCreateInfo{};
        desLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desLayoutCreateInfo.bindingCount = 1;
        desLayoutCreateInfo.pBindings = &imageBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &desLayoutCreateInfo, nullptr, &m_des_layout),
                 "Failed to create the descriptor set layout");
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = m_ctx->imageCount;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.maxSets = m_ctx->imageCount;
        poolCreateInfo.pPoolSizes = &poolSize;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &poolCreateInfo, nullptr, &m_des_pool),
                 "Failed to create the descriptor Pool");

        std::vector<VkDescriptorSetLayout> layouts(3, m_des_layout);
        m_des_sets.resize(m_ctx->imageCount);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_des_pool;
        allocateInfo.descriptorSetCount = m_ctx->imageCount;
        allocateInfo.pSetLayouts = layouts.data();
        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, m_des_sets.data());

        for (int i = 0; i < m_ctx->imageCount; i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = yPlaneImageView;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.sampler = m_sampler;

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.dstSet = m_des_sets[i];
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(m_ctx->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
        }
    }

    void FrameHandler::render(uint32_t* rgba) {
        if (!isFirstRender) {
            transition_image_layout(m_ctx, m_commandBuffer, yPlaneImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        }
        void *data;
        vkMapMemory(m_ctx->logicalDevice, yPlaneBufferMemory, 0, m_width * m_height, 0, &data);
        memcpy(data, rgba, m_width * m_height * sizeof (uint32_t));
        vkUnmapMemory(m_ctx->logicalDevice, yPlaneBufferMemory);

        buffer_to_image(m_ctx, m_commandBuffer, yPlaneBuffer, yPlaneImage, m_width, m_height, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        transition_image_layout(m_ctx, m_commandBuffer, yPlaneImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        if (isFirstRender) {
            isFirstRender = false; }
    }

    void FrameHandler::cleanup() {
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_des_pool, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_des_layout, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, yPlaneBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, yPlaneBufferMemory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, yPlaneImageView, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, yPlaneImage, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, yPlaneImageMemory, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler, nullptr);
    }
}