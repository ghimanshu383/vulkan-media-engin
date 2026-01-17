//
// Created by ghima on 15-01-2026.
//
#include <array>
#include "computes/VulkanYuvToRgba.h"

namespace fd {
    ComputeYuvRgba::ComputeYuvRgba(RenderContext *ctx, const char *shaderPath, uint32_t width, uint32_t height) : m_ctx{
            ctx}, m_shader_path{shaderPath}, m_width{width},
                                                                                                                  m_height{
                                                                                                                          height} {
        m_commandBuffer = start_command_buffer(m_ctx);
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(m_ctx->logicalDevice, &createInfo, nullptr, &m_compute_semaphore);
        vkCreateSemaphore(m_ctx->logicalDevice, &createInfo, nullptr, &m_filter_semaphore);
        set_up_compute_command_buffer();
        prepare_buffers_and_images();
        create_samplers();
        setup_descriptors();
        create_pipeline();

        m_blur = new VulkanFilterR8(m_ctx,
                                    R"(D:\cProjects\realTimeFrameDisplay\shaders\gaussianBlurCompute.comp.spv)",
                                    m_width, m_height);
        vkMapMemory(m_ctx->logicalDevice, m_y_plane_buffer_memory, 0, m_width * m_height, 0, &yData);
        vkMapMemory(m_ctx->logicalDevice, m_u_plane_buffer_memory, 0, (m_width >> 1) * (m_height >> 1), 0, &uData);
        vkMapMemory(m_ctx->logicalDevice, m_v_plane_buffer_memory, 0, (m_width >> 1) * (m_height >> 1), 0, &vData);
    }

    void ComputeYuvRgba::prepare_buffers_and_images() {
        uint32_t chromaW = m_width >> 1;
        uint32_t chromaH = m_height >> 1;
        VkDeviceSize size = m_width * m_height;
        VkDeviceSize chromaSize = (m_width >> 1) * (m_height >> 1);
        create_buffer(m_ctx, m_y_plane_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_y_plane_buffer_memory,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size);
        create_buffer(m_ctx, m_u_plane_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_u_plane_buffer_memory,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, chromaSize);
        create_buffer(m_ctx, m_v_plane_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_v_plane_buffer_memory,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, chromaSize);

        // Creating the images and views.
        create_image(m_ctx, m_y_image, m_width, m_height, m_y_image_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_y_image, m_y_image_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_u_image, chromaW, chromaH, m_u_image_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_u_image, m_u_image_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_v_image, chromaW, chromaH, m_v_image_memory, VK_FORMAT_R8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_v_image, m_v_image_view, VK_FORMAT_R8_UNORM);

        create_image(m_ctx, m_rgba_image, m_width, m_height, m_rgba_image_memory, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_image_view(m_ctx->logicalDevice, m_rgba_image, m_rgba_image_view, VK_FORMAT_R8G8B8A8_UNORM);

        transition_image_layout(m_ctx, m_commandBuffer, m_y_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(m_ctx, m_commandBuffer, m_u_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(m_ctx, m_commandBuffer, m_v_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition_image_layout(m_ctx, m_commandBuffer, m_rgba_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void ComputeYuvRgba::setup_descriptors() {
        VkDescriptorSetLayoutBinding yuvBinding{};
        yuvBinding.binding = 0;
        yuvBinding.descriptorCount = 3;
        yuvBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        yuvBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        yuvBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding rgbaBinding{};
        rgbaBinding.binding = 1;
        rgbaBinding.descriptorCount = 1;
        rgbaBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        rgbaBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        rgbaBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings{yuvBinding, rgbaBinding};
        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = bindings.size();
        createInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &createInfo, nullptr, &m_des_layout),
                 "failed to create the descriptor set for compute rgba");

        // Creating a descriptor Pool.
        VkDescriptorPoolSize inputSize{};
        inputSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inputSize.descriptorCount = 3;
        VkDescriptorPoolSize outputSize{};
        outputSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputSize.descriptorCount = 1;
        std::array<VkDescriptorPoolSize, 2> sizes{inputSize, outputSize};
        VkDescriptorPoolCreateInfo desPoolCreateInfo{};
        desPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desPoolCreateInfo.poolSizeCount = sizes.size();
        desPoolCreateInfo.pPoolSizes = sizes.data();
        desPoolCreateInfo.maxSets = 1;


        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &desPoolCreateInfo, nullptr, &m_des_pool),
                 "Failed to create the descriptor set pool");

        // Creating the descriptor sets and write info.
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.pSetLayouts = &m_des_layout;
        allocateInfo.descriptorPool = m_des_pool;
        allocateInfo.descriptorSetCount = 1;

        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, &m_des_set);

        VkDescriptorImageInfo yImageInfo{};
        yImageInfo.sampler = m_sampler_y;
        yImageInfo.imageView = m_y_image_view;
        yImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo uImageInfo{};
        uImageInfo.sampler = m_sampler_u;
        uImageInfo.imageView = m_u_image_view;
        uImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo vImageInfo{};
        vImageInfo.sampler = m_sampler_v;
        vImageInfo.imageView = m_v_image_view;
        vImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkDescriptorImageInfo, 3> imageInfos{yImageInfo, uImageInfo, vImageInfo};
        VkWriteDescriptorSet yuvWrite{};
        yuvWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        yuvWrite.descriptorCount = imageInfos.size();
        yuvWrite.dstBinding = 0;
        yuvWrite.dstArrayElement = 0;
        yuvWrite.dstSet = m_des_set;
        yuvWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        yuvWrite.pImageInfo = imageInfos.data();


        VkDescriptorImageInfo rgbaInfo{};
        rgbaInfo.sampler = m_sampler_rgba;
        rgbaInfo.imageView = m_rgba_image_view;
        rgbaInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet rgbaWrite{};
        rgbaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        rgbaWrite.pImageInfo = &rgbaInfo;
        rgbaWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        rgbaWrite.descriptorCount = 1;
        rgbaWrite.dstArrayElement = 0;
        rgbaWrite.dstBinding = 1;
        rgbaWrite.dstSet = m_des_set;

        std::array<VkWriteDescriptorSet, 2> writeInfo{yuvWrite, rgbaWrite};
        vkUpdateDescriptorSets(m_ctx->logicalDevice, writeInfo.size(), writeInfo.data(), 0, nullptr);
    }

    void ComputeYuvRgba::create_pipeline() {
        VkShaderModule computeModule = create_shader_module(m_ctx->logicalDevice, m_shader_path);
        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.pName = "main";
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = computeModule;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_des_layout;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_pipeline_layout),
                 "failed to create the pipeline layout for rgba");
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = m_pipeline_layout;
        computePipelineCreateInfo.basePipelineIndex = 0;
        computePipelineCreateInfo.stage = computeStage;

        VK_CHECK(vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr,
                                          &m_pipeline), "Failed to create the pipeline");
        vkDestroyShaderModule(m_ctx->logicalDevice, computeModule, nullptr);
    }

    void ComputeYuvRgba::create_samplers() {
        create_sampler(m_ctx->logicalDevice, m_sampler_y);
        create_sampler(m_ctx->logicalDevice, m_sampler_u);
        create_sampler(m_ctx->logicalDevice, m_sampler_v);
        create_sampler(m_ctx->logicalDevice, m_sampler_rgba);
    }

    void ComputeYuvRgba::compute(uint8_t *yPlane, uint8_t *uPlane, uint8_t *vPlane) {
        vkResetCommandBuffer(m_compute_command_buffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(m_compute_command_buffer, &beginInfo), "Failed to begin the command buffer");

        if (!firstRender) {
            record_transition_image(m_compute_command_buffer, m_y_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT);
            record_transition_image(m_compute_command_buffer, m_u_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT);
            record_transition_image(m_compute_command_buffer, m_v_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT);
            record_transition_image(m_compute_command_buffer, m_rgba_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_ctx->graphicsQueueIndex,
                                    m_ctx->computeQueueIndex);
        }
        memcpy(yData, yPlane, m_width * m_height);
        memcpy(uData, uPlane, (m_width >> 1) * (m_height >> 1));
        memcpy(vData, vPlane, (m_width >> 1) * (m_height >> 1));

        record_buffer_to_image(m_compute_command_buffer, m_y_plane_buffer, m_y_image, m_width, m_height,
                               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        record_buffer_to_image(m_compute_command_buffer, m_u_plane_buffer, m_u_image, (m_width >> 1), (m_height >> 1),
                               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        record_buffer_to_image(m_compute_command_buffer, m_v_plane_buffer, m_v_image, (m_width >> 1), (m_height >> 1),
                               VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        record_transition_image(m_compute_command_buffer, m_u_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        record_transition_image(m_compute_command_buffer, m_v_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // record all filters for the yplane here.
        invoke_r8_filters();

        record_transition_image(m_compute_command_buffer, m_y_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        vkEndCommandBuffer(m_compute_command_buffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_compute_command_buffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_filter_semaphore;

        vkQueueSubmit(m_ctx->computeQueue, 1, &submitInfo, nullptr);
        dispatch();
        if (firstRender) {
            firstRender = false;
        }
    }

    void ComputeYuvRgba::invoke_r8_filters() {
        m_blur->compute(m_compute_command_buffer, m_y_image);
    }

    void ComputeYuvRgba::dispatch() {
//        vkWaitForFences(m_ctx->logicalDevice, 1, &m_compute_fence, VK_TRUE, UINT32_MAX);
//        vkResetFences(m_ctx->logicalDevice, 1, &m_compute_fence);

        vkResetCommandBuffer(m_commandBuffer_dispatch, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        vkBeginCommandBuffer(m_commandBuffer_dispatch, &beginInfo);
        vkCmdBindPipeline(m_commandBuffer_dispatch, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(m_commandBuffer_dispatch, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout, 0, 1,
                                &m_des_set, 0,
                                nullptr);
        vkCmdDispatch(m_commandBuffer_dispatch, (m_width + 7) / 8, (m_height + 7) / 8, 1);
        record_transition_image(m_commandBuffer_dispatch, m_rgba_image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                m_ctx->computeQueueIndex, m_ctx->graphicsQueueIndex);
        vkEndCommandBuffer(m_commandBuffer_dispatch);

        VkPipelineStageFlags waitFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffer_dispatch;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_filter_semaphore;
        submitInfo.pWaitDstStageMask = &waitFlags;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_compute_semaphore;

        vkQueueSubmit(m_ctx->computeQueue, 1, &submitInfo, nullptr);

    }

    void ComputeYuvRgba::set_up_compute_command_buffer() {
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = m_ctx->graphicsQueueIndex;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(m_ctx->logicalDevice, &commandPoolCreateInfo, nullptr, &m_compute_command_pool);

        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandBufferCount = 1;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandPool = m_compute_command_pool;
        VK_CHECK(vkAllocateCommandBuffers(m_ctx->logicalDevice, &allocateInfo, &m_compute_command_buffer),
                 "Failed to allocate the compute command buffers");
        VK_CHECK(vkAllocateCommandBuffers(m_ctx->logicalDevice, &allocateInfo, &m_commandBuffer_dispatch),
                 "Failed to allocate the compute command buffers");
    }

    void ComputeYuvRgba::clean_up() {
        vkDestroyBuffer(m_ctx->logicalDevice, m_y_plane_buffer, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_v_plane_buffer, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_u_plane_buffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_y_plane_buffer_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_v_plane_buffer_memory, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_u_plane_buffer_memory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_y_image_view, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_y_image, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_y_image_memory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_u_image_view, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_u_image, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_u_image_memory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_v_image_view, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_v_image, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_v_image_memory, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_rgba_image_view, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_rgba_image, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_rgba_image_memory, nullptr);

        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_des_layout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_des_pool, nullptr);

        vkDestroySampler(m_ctx->logicalDevice, m_sampler_y, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_u, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_v, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler_rgba, nullptr);
        vkDestroySemaphore(m_ctx->logicalDevice, m_filter_semaphore, nullptr);
        vkDestroySemaphore(m_ctx->logicalDevice, m_compute_semaphore, nullptr);
        vkDestroyCommandPool(m_ctx->logicalDevice, m_compute_command_pool, nullptr);
        m_blur->cleanup();
    }
}