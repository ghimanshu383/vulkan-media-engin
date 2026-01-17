//
// Created by ghima on 08-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_UTIL_H
#define REALTIMEFRAMEDISPLAY_UTIL_H

#include <spdlog/spdlog.h>
#include <fstream>
#include <vulkan/vulkan.h>
#include <vector>
#include "glm/glm.hpp"

#define LOG_INFO(M, ...) spdlog::info(M, ##__VA_ARGS__)
#define LOG_ERROR(M, ...) spdlog::error(M, ##__VA_ARGS__)
#define LOG_WARN(M, ...) spdlog::warn(M, ##__VA_ARGS__)

#define VK_CHECK(result, message, ...) if(result != VK_SUCCESS) {                        \
                           LOG_ERROR(message, ##__VA_ARGS__);  std::exit(EXIT_FAILURE);   \
                           }

constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;
constexpr int MAX_FRAMES = 3;

namespace fd {
    struct RenderContext {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
        VkQueue graphicsQueue;
        uint32_t graphicsQueueIndex;
        VkQueue computeQueue;
        uint32_t computeQueueIndex;
        VkCommandPool commandPool;
        uint32_t imageCount;
        VkDescriptorSetLayout desLayoutFrame;
        std::vector<VkDescriptorSet> desSetFrame{};
    };
}
struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};
struct ImageExtent {
    uint32_t width;
    uint32_t height;
};
struct VideoFrame {
    std::unique_ptr<uint8_t[]> yPlane;
    std::unique_ptr<uint8_t[]> uPlane;
    std::unique_ptr<uint8_t[]> vPlane;
    double pts_seconds;
};

struct AudioPCM {
    std::unique_ptr<int16_t[]> samples;
    int channel;
    int sampleRate;
    int interleaved;
};
struct AvIndex {
    int audioIndex;
    int videoIndex;
};

inline void clamp(int &val) {
    if (val < 0) {
        val = 0;
        return;
    }
    if (val > 255) {
        val = 255;
        return;
    }
}

inline float clamp_float_audio(float val) {
    if (val < -1.0f) {
        val = -1.0f;
    } else if (val > 1.0f) {
        val = 1.0f;
    }
    return val;
}

inline uint32_t
find_memory_index(VkPhysicalDevice physicalDevice, uint32_t requiredIndex, VkMemoryPropertyFlags requiredMemoryFlags) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (size_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((requiredIndex & (1 << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & requiredMemoryFlags) == requiredMemoryFlags) {
            return i;
        }
    }
    std::exit(EXIT_FAILURE);
}

inline void create_image_view(VkDevice &device, VkImage &image, VkImageView &imageView, VkFormat format) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.format = format;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView), "Failed to create the image view");
}

inline void read_binary_file(const char *filePath, std::vector<uint8_t> &dataBytes) {
    std::ifstream inputStream(filePath, std::ios::binary | std::ios::ate);
    if (!inputStream) {
        LOG_ERROR("Failed to load files {}", filePath);
        return;
    }
    size_t fileSize = inputStream.tellg();
    dataBytes.resize(fileSize);
    inputStream.seekg(0);

    inputStream.read(reinterpret_cast<char *>(dataBytes.data()), fileSize);
}


inline VkShaderModule create_shader_module(VkDevice device, const char *filePath) {
    std::vector<uint8_t> dataBytes{};
    read_binary_file(filePath, dataBytes);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = static_cast<uint32_t >(dataBytes.size());
    createInfo.pCode = reinterpret_cast<uint32_t *>(dataBytes.data());

    VkShaderModule shaderModule{};
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create the shader module");
    return shaderModule;
}


inline VkCommandBuffer start_command_buffer(fd::RenderContext *ctx) {
    VkCommandBuffer commandBuffer{};
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = ctx->commandPool;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vkAllocateCommandBuffers(ctx->logicalDevice, &allocateInfo, &commandBuffer);
    return commandBuffer;
}

inline VkFence get_fence(fd::RenderContext *ctx) {
    VkFenceCreateInfo createInfo{};
    VkFence fence{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateFence(ctx->logicalDevice, &createInfo, nullptr, &fence);
    return fence;
}

inline void
submit_queue(fd::RenderContext *ctx, VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore = VK_NULL_HANDLE) {
    vkEndCommandBuffer(commandBuffer);
    VkFence fence = get_fence(ctx);
    VkSubmitInfo submitInfo{};
    std::vector<VkPipelineStageFlags> waitFlags{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = waitSemaphore == VK_NULL_HANDLE ? 0 : 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = waitSemaphore == VK_NULL_HANDLE ? nullptr : waitFlags.data();

    if (waitSemaphore == VK_NULL_HANDLE) {
        vkResetFences(ctx->logicalDevice, 1, &fence);
        vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, fence);
        vkWaitForFences(ctx->logicalDevice, 1, &fence, VK_TRUE, UINT32_MAX);
        vkDestroyFence(ctx->logicalDevice, fence, nullptr);
    } else {
        vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, nullptr);
    }

}

inline void
create_buffer(fd::RenderContext *ctx, VkBuffer &buffer, VkBufferUsageFlags usageFlags, VkDeviceMemory &bufferMemory,
              VkMemoryPropertyFlags propertyFlags, VkDeviceSize size) {

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(ctx->logicalDevice, &createInfo, nullptr, &buffer);

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(ctx->logicalDevice, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_index(ctx->physicalDevice, requirements.memoryTypeBits, propertyFlags);
    vkAllocateMemory(ctx->logicalDevice, &allocateInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(ctx->logicalDevice, buffer, bufferMemory, 0);
}

inline void
create_image(fd::RenderContext *ctx, VkImage &image, uint32_t width, uint32_t height, VkDeviceMemory &imageMemory,
             VkFormat format,
             VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags) {
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.format = format;
    createInfo.usage = usageFlags;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.arrayLayers = 1;
    createInfo.mipLevels = 1;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.extent = {width, height, 1};

    VkMemoryRequirements requirements{};
    vkCreateImage(ctx->logicalDevice, &createInfo, nullptr, &image);
    vkGetImageMemoryRequirements(ctx->logicalDevice, image, &requirements);
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_index(ctx->physicalDevice, requirements.memoryTypeBits,
                                                     memoryPropertyFlags);
    vkAllocateMemory(ctx->logicalDevice, &allocateInfo, nullptr, &imageMemory);

    vkBindImageMemory(ctx->logicalDevice, image, imageMemory, 0);
}

inline void
buffer_to_buffer(fd::RenderContext *ctx, VkCommandBuffer commandBuffer, VkBuffer &srcBuffer, VkBuffer &dstBuffer,
                 VkDeviceSize size) {
    VkBufferCopy region{};
    region.size = size;
    region.dstOffset = 0;
    region.srcOffset = 0;

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin the command buffer");
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &region);
    submit_queue(ctx, commandBuffer);
}

inline void
buffer_to_image(fd::RenderContext *ctx, VkCommandBuffer commandBuffer, VkBuffer &srcBuffer, VkImage dstImage,
                uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags, VkImageLayout dstLayout) {

    VkBufferImageCopy bufferImageCopy{};
    bufferImageCopy.imageExtent = {width, height, 1};
    bufferImageCopy.bufferOffset = 0;
    bufferImageCopy.imageOffset = {0, 0};
    bufferImageCopy.bufferImageHeight = 0;
    bufferImageCopy.bufferRowLength = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;
    bufferImageCopy.imageSubresource.aspectMask = aspectFlags;
    bufferImageCopy.imageSubresource.mipLevel = 0;

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin the command buffer");

    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstLayout, 1, &bufferImageCopy);
    submit_queue(ctx, commandBuffer);
}

inline void record_buffer_to_image(VkCommandBuffer commandBuffer, VkBuffer &srcBuffer, VkImage dstImage,
                                   uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags,
                                   VkImageLayout dstLayout) {
    VkBufferImageCopy bufferImageCopy{};
    bufferImageCopy.imageExtent = {width, height, 1};
    bufferImageCopy.bufferOffset = 0;
    bufferImageCopy.imageOffset = {0, 0};
    bufferImageCopy.bufferImageHeight = 0;
    bufferImageCopy.bufferRowLength = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;
    bufferImageCopy.imageSubresource.aspectMask = aspectFlags;
    bufferImageCopy.imageSubresource.mipLevel = 0;
    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstLayout, 1, &bufferImageCopy);
}

inline void
transition_image_layout(fd::RenderContext *ctx, VkCommandBuffer commandBuffer, VkImage image,
                        VkImageAspectFlags aspectFlags, VkImageLayout oldLayout,
                        VkImageLayout newLayout, VkAccessFlags srcAccess, VkPipelineStageFlags srcStage,
                        VkAccessFlags dstAccess, VkPipelineStageFlags dstStage,
                        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin the command buffer");

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
    submit_queue(ctx, commandBuffer);

}

inline void
image_to_image(fd::RenderContext *ctx, VkCommandBuffer commandBuffer, VkImage &srcImage, VkImage &dstImage,
               uint32_t width,
               uint32_t height, VkSemaphore waitSemaphore = VK_NULL_HANDLE) {
    VkImageCopy region{};
    region.srcOffset = {0, 0};
    region.dstOffset = {0, 0};
    region.extent = {width, height, 1};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.mipLevel = 0;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.mipLevel = 0;

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin the command buffer");
    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    submit_queue(ctx, commandBuffer, waitSemaphore);
}

inline void record_image_to_image(VkCommandBuffer commandBuffer, VkImage &srcImage, VkImage &dstImage,
                                  uint32_t width,
                                  uint32_t height) {
    VkImageCopy region{};
    region.srcOffset = {0, 0};
    region.dstOffset = {0, 0};
    region.extent = {width, height, 1};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.mipLevel = 0;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.mipLevel = 0;

    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

inline void record_transition_image(VkCommandBuffer commandBuffer, VkImage image,
                                    VkImageAspectFlags aspectFlags, VkImageLayout oldLayout,
                                    VkImageLayout newLayout, VkAccessFlags srcAccess, VkPipelineStageFlags srcStage,
                                    VkAccessFlags dstAccess, VkPipelineStageFlags dstStage,
                                    uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

inline void create_sampler(VkDevice logicalDevice, VkSampler &sampler) {
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &sampler),
             "failed to create the sampler");

}

#endif //REALTIMEFRAMEDISPLAY_UTIL_H
