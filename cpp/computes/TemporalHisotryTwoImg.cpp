//
// Created by ghima on 18-01-2026.
//
#include <array>
#include "computes/TemporalHistoryTwoImg.h"

namespace fd {
    TemporalHistoryTwoImg::TemporalHistoryTwoImg(fd::RenderContext *ctx, const char *shaderPath, uint32_t width,
                                                 uint32_t height) : m_ctx{ctx}, m_width{width}, m_height{height},
                                                                    m_shader_path{shaderPath} {
        m_motion_vector_buffer_size = ((m_width + 7) / 8) * ((m_height + 7) / 8);
        setup_images_and_history();
        setup_descriptors();
        create_pipeline();
    }

    uint32_t TemporalHistoryTwoImg::m_frame_count = 0;

    void TemporalHistoryTwoImg::setup_images_and_history() {
        VkCommandBuffer commandBuffer = start_command_buffer(m_ctx);
        create_image(m_ctx, m_img_one, m_width, m_height, m_img_one_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_img_one, m_img_one_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_img_two, m_width, m_height, m_img_two_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_img_two, m_img_two_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_img_three, m_width, m_height, m_img_three_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_img_three, m_img_three_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_img_out, m_width, m_height, m_img_out_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_img_out, m_img_out_view, VK_FORMAT_R8_UNORM);

        transition_image_layout(m_ctx, commandBuffer, m_img_one, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        transition_image_layout(m_ctx, commandBuffer, m_img_two, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(m_ctx, commandBuffer, m_img_three, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(m_ctx, commandBuffer, m_img_out, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        create_sampler(m_ctx->logicalDevice, m_sampler_one);
        create_sampler(m_ctx->logicalDevice, m_sampler_two);
        create_sampler(m_ctx->logicalDevice, m_sampler_three);
        create_sampler(m_ctx->logicalDevice, m_sampler_out);

        m_history[0] = m_img_one;
        m_history[1] = m_img_two;
        m_history[2] = m_img_three;

        // Creating the motion vectors
        create_buffer(m_ctx, m_motion_vectors_buffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      m_motion_vectors_buffer_memory,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      sizeof(MotionVector) * m_motion_vector_buffer_size);

    }

    void TemporalHistoryTwoImg::setup_descriptors() {
        VkDescriptorSetLayoutBinding inBinding{};
        inBinding.binding = 0;
        inBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inBinding.descriptorCount = 3;
        inBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding outBinding{};
        outBinding.binding = 1;
        outBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outBinding.descriptorCount = 1;
        outBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding motionVectorBinding{};
        motionVectorBinding.binding = 2;
        motionVectorBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        motionVectorBinding.descriptorCount = 1;
        motionVectorBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        motionVectorBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings{inBinding, outBinding, motionVectorBinding};
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_des_layout),
                 "Failed to create the des layout for temporal history two");
        VkDescriptorPoolSize sizeIn{};
        sizeIn.descriptorCount = 3;
        sizeIn.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        VkDescriptorPoolSize sizeOut{};
        sizeOut.descriptorCount = 1;
        sizeOut.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        VkDescriptorPoolSize sizeMotionVector{};
        sizeMotionVector.descriptorCount = 1;
        sizeMotionVector.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        std::array<VkDescriptorPoolSize, 3> sizes{sizeIn, sizeOut, sizeMotionVector};
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.maxSets = 1;
        poolCreateInfo.poolSizeCount = sizes.size();
        poolCreateInfo.pPoolSizes = sizes.data();

        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &poolCreateInfo, nullptr, &m_des_pool),
                 "failed to create the des pool for temporal history");
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.descriptorPool = m_des_pool;
        allocateInfo.pSetLayouts = &m_des_layout;

        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, &m_des_set);

        VkDescriptorImageInfo infoInOne{};
        infoInOne.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infoInOne.imageView = m_img_one_view;
        infoInOne.sampler = m_sampler_one;
        VkDescriptorImageInfo infoInTwo{};
        infoInTwo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infoInTwo.imageView = m_img_two_view;
        infoInTwo.sampler = m_sampler_two;
        VkDescriptorImageInfo infoInThree{};
        infoInThree.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infoInThree.imageView = m_img_three_view;
        infoInThree.sampler = m_sampler_three;
        std::array<VkDescriptorImageInfo, 3> inInfos{infoInOne, infoInTwo, infoInThree};
        VkDescriptorImageInfo infoOut{};
        infoOut.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        infoOut.imageView = m_img_out_view;
        infoOut.sampler = m_sampler_out;

        VkWriteDescriptorSet writeIn{};
        writeIn.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeIn.descriptorCount = inInfos.size();
        writeIn.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeIn.dstArrayElement = 0;
        writeIn.dstBinding = 0;
        writeIn.dstSet = m_des_set;
        writeIn.pImageInfo = inInfos.data();

        VkWriteDescriptorSet writeOut{};
        writeOut.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeOut.descriptorCount = 1;
        writeOut.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeOut.dstArrayElement = 0;
        writeOut.dstBinding = 1;
        writeOut.dstSet = m_des_set;
        writeOut.pImageInfo = &infoOut;

        VkDescriptorBufferInfo motionVectorBufferInfo{};
        motionVectorBufferInfo.offset = 0;
        motionVectorBufferInfo.buffer = m_motion_vectors_buffer;
        motionVectorBufferInfo.range = sizeof(MotionVector) * m_motion_vector_buffer_size;
        VkWriteDescriptorSet motionVectorWriteInfo{};
        motionVectorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        motionVectorWriteInfo.descriptorCount = 1;
        motionVectorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        motionVectorWriteInfo.dstBinding = 2;
        motionVectorWriteInfo.dstArrayElement = 0;
        motionVectorWriteInfo.dstSet = m_des_set;
        motionVectorWriteInfo.pBufferInfo = &motionVectorBufferInfo;

        std::array<VkWriteDescriptorSet, 3> writes{writeIn, writeOut, motionVectorWriteInfo};
        vkUpdateDescriptorSets(m_ctx->logicalDevice, writes.size(), writes.data(), 0, nullptr);
    }

    void TemporalHistoryTwoImg::create_pipeline() {
        VkShaderModule computeModule = create_shader_module(m_ctx->logicalDevice, m_shader_path);
        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.pName = "main";
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = computeModule;

        VkPushConstantRange extentRange{};
        extentRange.size = sizeof(TemporalInfo);
        extentRange.offset = 0;
        extentRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_des_layout;
        layoutCreateInfo.pushConstantRangeCount = 1;
        layoutCreateInfo.pPushConstantRanges = &extentRange;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_pipeline_layout),
                 "failed to create the pipeline layout for temporal history");
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = m_pipeline_layout;
        computePipelineCreateInfo.basePipelineIndex = 0;
        computePipelineCreateInfo.stage = computeStage;

        VK_CHECK(vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr,
                                          &m_pipeline), "Failed to create the pipeline for temporal history");
        vkDestroyShaderModule(m_ctx->logicalDevice, computeModule, nullptr);
    }

    void TemporalHistoryTwoImg::compute(VkCommandBuffer commandBuffer, VkImage &r8Image) {
        record_transition_image(commandBuffer, r8Image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        if (m_frame_count == 0) {
            // copying both of the history images with same data.
            record_image_to_image(commandBuffer, r8Image, m_history[0], m_width, m_height);
            record_image_to_image(commandBuffer, r8Image, m_history[1], m_width, m_height);
            record_image_to_image(commandBuffer, r8Image, m_history[2], m_width, m_height);
            record_transition_image(commandBuffer, m_history[0], VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            record_transition_image(commandBuffer, m_history[1], VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            record_transition_image(commandBuffer, m_history[2], VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        } else {
            VkImage currImage = m_history[m_frame_count % 3];
            record_transition_image(commandBuffer, currImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            record_image_to_image(commandBuffer, r8Image, currImage, m_width, m_height);
            record_transition_image(commandBuffer, currImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout, 0, 1, &m_des_set, 0,
                                nullptr);
        TemporalInfo info{m_width, m_height, m_frame_count};
        vkCmdPushConstants(commandBuffer, m_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TemporalInfo),
                           &info);
        vkCmdDispatch(commandBuffer, (m_width + 7) / 8, (m_height + 7) / 8, 1);
        record_transition_image(commandBuffer, r8Image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        record_transition_image(commandBuffer, m_img_out, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        record_transition_image(commandBuffer, m_history[(m_frame_count + 2) % 3], VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        record_image_to_image(commandBuffer, m_img_out, m_history[(m_frame_count + 2) % 3], m_width, m_height);
        record_image_to_image(commandBuffer, m_img_out, r8Image, m_width, m_height);
        record_transition_image(commandBuffer, m_img_out, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        record_transition_image(commandBuffer, m_history[(m_frame_count + 2) % 3], VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        m_frame_count++;

    }

    void TemporalHistoryTwoImg::clean_up() {

        vkDestroyBuffer(m_ctx->logicalDevice, m_motion_vectors_buffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_motion_vectors_buffer_memory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_img_one_view, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_img_two_view, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_img_three_view, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_img_out_view, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_img_out, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_img_one, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_img_three, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_img_two, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_img_out_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_img_one_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_img_two_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_img_three_memory, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_one, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_two, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_three, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_out, nullptr);
        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_pipeline_layout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_des_pool, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_des_layout, nullptr);
    }

}