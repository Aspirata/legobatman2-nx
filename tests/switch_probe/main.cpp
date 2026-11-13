// Toolchain/device smoke test. No game code or game resources.
#include <switch.h>
#include <deko3d.h>
#include <cstdio>
#include <cstdint>

struct Graphics {
    DkDevice device = nullptr;
    DkQueue queue = nullptr;
    DkMemBlock imagesMemory = nullptr;
    DkMemBlock commandMemory = nullptr;
    DkCmdBuf commands = nullptr;
    DkSwapchain swapchain = nullptr;
    DkImage images[2];
    ~Graphics() {
        if (queue) dkQueueWaitIdle(queue);
        if (swapchain) dkSwapchainDestroy(swapchain);
        if (commands) dkCmdBufDestroy(commands);
        if (commandMemory) dkMemBlockDestroy(commandMemory);
        if (imagesMemory) dkMemBlockDestroy(imagesMemory);
        if (queue) dkQueueDestroy(queue);
        if (device) dkDeviceDestroy(device);
    }
};

int main() {
    FILE* log = std::fopen("sdmc:/switch/legobatman2-nx-probe/probe.log", "w");
    auto report = [&](const char* message) {
        if (log) { std::fprintf(log, "%s\n", message); std::fflush(log); }
    };
    report("LB2 toolchain probe v1: no guest game code");
    report(appletGetAppletType() == AppletType_Application ? "mode=application" : "mode=other");
    int result = [&]() {
        Graphics gpu;
        DkDeviceMaker deviceMaker;
        dkDeviceMakerDefaults(&deviceMaker);
        gpu.device = dkDeviceCreate(&deviceMaker);
        if (!gpu.device) { report("FAIL: device"); return 1; }
        report("device=ready");
        DkQueueMaker queueMaker;
        dkQueueMakerDefaults(&queueMaker, gpu.device);
        gpu.queue = dkQueueCreate(&queueMaker);
        if (!gpu.queue) { report("FAIL: queue"); return 2; }

        DkImageLayoutMaker imageMaker;
        dkImageLayoutMakerDefaults(&imageMaker, gpu.device);
        imageMaker.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent;
        imageMaker.format = DkImageFormat_RGBA8_Unorm;
        imageMaker.dimensions[0] = 1280;
        imageMaker.dimensions[1] = 720;
        DkImageLayout layout;
        dkImageLayoutInitialize(&layout, &imageMaker);
        const uint32_t alignment = dkImageLayoutGetAlignment(&layout);
        const uint32_t stride = (dkImageLayoutGetSize(&layout) + alignment - 1) & ~(alignment - 1);
        const uint32_t imageBytes = (2 * stride + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1);
        DkMemBlockMaker memoryMaker;
        dkMemBlockMakerDefaults(&memoryMaker, gpu.device, imageBytes);
        memoryMaker.flags |= DkMemBlockFlags_Image;
        gpu.imagesMemory = dkMemBlockCreate(&memoryMaker);
        if (!gpu.imagesMemory) { report("FAIL: image memory"); return 3; }
        for (unsigned i = 0; i != 2; ++i) dkImageInitialize(&gpu.images[i], &layout, gpu.imagesMemory, i * stride);
        const DkImage* images[] = {&gpu.images[0], &gpu.images[1]};
        DkSwapchainMaker swapMaker;
        dkSwapchainMakerDefaults(&swapMaker, gpu.device, nwindowGetDefault(), images, 2);
        gpu.swapchain = dkSwapchainCreate(&swapMaker);
        if (!gpu.swapchain) { report("FAIL: swapchain"); return 4; }
        report("swapchain=ready");

        dkMemBlockMakerDefaults(&memoryMaker, gpu.device, 65536);
        gpu.commandMemory = dkMemBlockCreate(&memoryMaker);
        if (!gpu.commandMemory) { report("FAIL: command memory"); return 5; }
        DkCmdBufMaker commandMaker;
        dkCmdBufMakerDefaults(&commandMaker, gpu.device);
        gpu.commands = dkCmdBufCreate(&commandMaker);
        if (!gpu.commands) { report("FAIL: command buffer"); return 6; }
        dkCmdBufAddMemory(gpu.commands, gpu.commandMemory, 0, 65536);

        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        PadState pad;
        padInitializeDefault(&pad);
        bool orange = false;
        uint64_t frames = 0;
        while (appletMainLoop()) {
            padUpdate(&pad);
            const uint64_t down = padGetButtonsDown(&pad);
            if (down & HidNpadButton_Plus) { report("plus=exit"); break; }
            if (down & HidNpadButton_A) { orange = !orange; report(orange ? "A=orange" : "A=blue"); }
            const int slot = dkQueueAcquireImage(gpu.queue, gpu.swapchain);
            if (slot < 0 || slot >= 2) { report("FAIL: acquire image"); return 7; }
            dkCmdBufClear(gpu.commands);
            DkImageView view;
            dkImageViewDefaults(&view, &gpu.images[slot]);
            const DkImageView* targets[] = {&view};
            dkCmdBufBindRenderTargets(gpu.commands, targets, 1, nullptr);
            DkViewport viewport = {0, 0, 1280, 720, 0, 1};
            DkScissor scissor = {0, 0, 1280, 720};
            dkCmdBufSetViewports(gpu.commands, 0, &viewport, 1);
            dkCmdBufSetScissors(gpu.commands, 0, &scissor, 1);
            const float blue[] = {0.025f, 0.12f, 0.30f, 1.0f};
            const float amber[] = {0.85f, 0.25f, 0.025f, 1.0f};
            dkCmdBufClearColor(gpu.commands, 0, DkColorMask_RGBA, orange ? amber : blue);
            dkQueueSubmitCommands(gpu.queue, dkCmdBufFinishList(gpu.commands));
            dkQueuePresentImage(gpu.queue, gpu.swapchain, slot);
            dkQueueWaitIdle(gpu.queue);
            if (++frames == 1) report("present=first");
            if (frames == 120) report("present=120");
        }
        report("graphics=shutdown");
        return 0;
    }();
    if (log) { std::fprintf(log, "exit=%d\n", result); std::fclose(log); }
    return result;
}
