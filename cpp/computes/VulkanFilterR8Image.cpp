//
// Created by ghima on 16-01-2026.
//
#include <array>
#include "computes/VulkanFilterR8Image.h"

namespace fd {

    VulkanFilterR8::VulkanFilterR8(fd::RenderContext *ctx, const char *computeFilter, uint32_t width, uint32_t height)
            : m_ctx{ctx}, m_width{width},
              m_height{height}, m_compute_path{computeFilter} {
        m_commandBuffer = start_command_buffer(m_ctx);
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(m_ctx->logicalDevice, &createInfo, nullptr, &m_compute_semaphore),
                 "Failed to create the semaphore");
        create_sampler(m_ctx->logicalDevice, m_image_out_sampler);
        create_sampler(m_ctx->logicalDevice, m_image_in_sampler);
        create_images_and_image_views();
        setup_descriptors();
        create_pipeline();
    }

    void VulkanFilterR8::create_images_and_image_views() {
        create_image(m_ctx, m_image_in, m_width, m_height, m_image_in_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_image_in, m_image_view_in, VK_FORMAT_R8_UNORM);
        transition_image_layout(m_ctx, m_commandBuffer, m_image_in, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);
        create_image(m_ctx, m_image_out, m_width, m_height, m_image_out_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_image_out, m_image_view_out, VK_FORMAT_R8_UNORM);
        transition_image_layout(m_ctx, m_commandBuffer, m_image_out, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void VulkanFilterR8::setup_descriptors() {
        VkDescriptorSetLayoutBinding inBinding{};
        inBinding.binding = 0;
        inBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inBinding.descriptorCount = 1;
        inBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inBinding.pImmutableSamplers = nullptr;
        VkDescriptorSetLayoutBinding outBinding{};
        outBinding.binding = 1;
        outBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outBinding.descriptorCount = 1;
        outBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings{inBinding, outBinding};
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_des_layout),
                 "failed to create the descriptor set layout for blur");
        VkDescriptorPoolSize sizeOne{};
        sizeOne.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizeOne.descriptorCount = 1;
        VkDescriptorPoolSize sizeTwo{};
        sizeTwo.descriptorCount = 1;
        sizeTwo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        std::array<VkDescriptorPoolSize, 2> sizes{sizeOne, sizeTwo};
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = sizes.size();
        poolCreateInfo.pPoolSizes = sizes.data();
        poolCreateInfo.maxSets = 1;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &poolCreateInfo, nullptr, &m_des_pool),
                 "Failed to create the descriptor pool for blur");
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_des_pool;
        allocateInfo.pSetLayouts = &m_des_layout;
        allocateInfo.descriptorSetCount = 1;
        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, &m_des_set);

        VkDescriptorImageInfo inInfo{};
        inInfo.sampler = m_image_in_sampler;
        inInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inInfo.imageView = m_image_view_in;
        VkWriteDescriptorSet writeIn{};
        writeIn.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeIn.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeIn.descriptorCount = 1;
        writeIn.dstBinding = 0;
        writeIn.pImageInfo = &inInfo;
        writeIn.dstArrayElement = 0;
        writeIn.dstSet = m_des_set;

        VkDescriptorImageInfo outInfo{};
        outInfo.sampler = m_image_out_sampler;
        outInfo.imageView = m_image_view_out;
        outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet outWrite{};
        outWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outWrite.dstBinding = 1;
        outWrite.dstArrayElement = 0;
        outWrite.descriptorCount = 1;
        outWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outWrite.pImageInfo = &outInfo;
        outWrite.dstSet = m_des_set;

        std::array<VkWriteDescriptorSet, 2> writes{writeIn, outWrite};
        vkUpdateDescriptorSets(m_ctx->logicalDevice, writes.size(), writes.data(), 0, nullptr);

    }

    void VulkanFilterR8::create_pipeline() {
        VkShaderModule computeModule = create_shader_module(m_ctx->logicalDevice, m_compute_path);
        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.pName = "main";
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = computeModule;

        VkPushConstantRange extentRange{};
        extentRange.size = sizeof(ImageExtent);
        extentRange.offset = 0;
        extentRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_des_layout;
        layoutCreateInfo.pushConstantRangeCount = 1;
        layoutCreateInfo.pPushConstantRanges = &extentRange;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_pipeline_layout),
                 "failed to create the pipeline layout for filter");
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = m_pipeline_layout;
        computePipelineCreateInfo.basePipelineIndex = 0;
        computePipelineCreateInfo.stage = computeStage;

        VK_CHECK(vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr,
                                          &m_pipeline), "Failed to create the pipeline");
        vkDestroyShaderModule(m_ctx->logicalDevice, computeModule, nullptr);
    }

    void VulkanFilterR8::compute(VkCommandBuffer commandBuffer,  VkImage &r8Image) {
        if (!isFirstRender) {

        }
        record_image_to_image(commandBuffer, r8Image, m_image_in, m_width, m_height);
        record_transition_image(commandBuffer, m_image_in, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        record_transition_image(commandBuffer, m_image_out, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout, 0, 1, &m_des_set, 0,
                                nullptr);
        ImageExtent extent{m_width, m_height};
        vkCmdPushConstants(commandBuffer, m_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ImageExtent),
                           &extent);
        vkCmdDispatch(commandBuffer, (m_width + 7) / 8, (m_height + 7) / 8, 1);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(m_ctx->computeQueue, 1, &submitInfo, nullptr);
        if (isFirstRender) isFirstRender = false;
    }

    void VulkanFilterR8::cleanup() {
        vkDestroyImageView(m_ctx->logicalDevice, m_image_view_in, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_image_view_out, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_image_in, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_image_out, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_image_out_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_image_in_memory, nullptr);

        vkDestroySampler(m_ctx->logicalDevice, m_image_out_sampler, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_image_in_sampler, nullptr);

        vkDestroySemaphore(m_ctx->logicalDevice, m_compute_semaphore, nullptr);
        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_des_layout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_des_pool, nullptr);

    }
}