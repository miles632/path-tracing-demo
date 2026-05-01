#pragma once

#include <vector>
#include <cstdint>

// forward declarations
struct Renderer;
struct VkDescriptorPool_T;
struct VkCommandBuffer_T;
struct VkFramebuffer_T;

typedef VkDescriptorPool_T* VkDescriptorPool;
typedef VkCommandBuffer_T* VkCommandBuffer;
typedef VkFramebuffer_T* VkFramebuffer;

class GUI {
public:
    GUI(Renderer* rd);
    void createDescPool(Renderer* rd);
    void createFramebufers(Renderer* rd, uint32_t numFrameBuffers);
    void draw(VkCommandBuffer cmdBuf);

    VkDescriptorPool descPool;
    std::vector<VkFramebuffer> framebuffers;
};