#include "gui.h"

#include <iostream>

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_vulkan.h"
#include "../external/imgui/backends/imgui_impl_glfw.h"

#include "../renderer.h"
#include "../globals.h"
#include "../../../1.4.321.1/x86_64/include/vulkan/vulkan_core.h"
#include "../include/globals.h"
#include "../include/renderer.h"
#include "../include/renderer.h"


GUI::GUI(Renderer* rd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    rd->guiHandle = this;
    createDescPool(&*rd);
    //std::cout << &this << std::endl;
    rd->initGui();
    createFramebufers(&*rd, rd->swapChainImages.size());
}

void GUI::createDescPool(Renderer* rd) {

    // descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(rd->device, &pool_info, nullptr, &descPool);
}

void GUI::createFramebufers(Renderer* rd, uint32_t numFrameBuffers) {
    framebuffers.resize(numFrameBuffers);
    for (uint32_t i = 0; i < numFrameBuffers; i++) {
        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = rd->imguiRenderPass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &rd->swapChainImageViews[i];
        fb_info.width = rd->swapChainExtent.width;
        fb_info.height = rd->swapChainExtent.height;
        fb_info.layers = 1;

        if (vkCreateFramebuffer(rd->device, &fb_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed creating framebuffers for the GUI");
        }
    }
}


void GUI::draw(Renderer* rd, VkCommandBuffer cmdBuf) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Test");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    if (ImGui::Combo("Scene", reinterpret_cast<int*>(&rd->sceneIndex), rd->scenes, IM_COUNTOF(rd->scenes))) {
        rd->pendingSceneRecreate = true;
    }

    //ImGui::ShowDemoWindow();
    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
}
