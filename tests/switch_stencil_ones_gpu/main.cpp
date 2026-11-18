#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define VK_NO_PROTOTYPES
#include "lb2_stencil_copy_shader.h"
#include "lb2_stencil_ones_shader.h"
#include <SPIRV/SpvBuilder.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#define FUNCS(X)                                                                             \
  X(CreateInstance)                                                                          \
  X(EnumeratePhysicalDevices)                                                                \
  X(GetPhysicalDeviceProperties)                                                             \
  X(GetPhysicalDeviceFeatures)                                                               \
  X(GetPhysicalDeviceQueueFamilyProperties) X(GetPhysicalDeviceMemoryProperties) X(          \
      GetPhysicalDeviceFormatProperties) X(GetPhysicalDeviceImageFormatProperties)           \
      X(CreateDevice) X(GetDeviceQueue) X(CreateCommandPool) X(                              \
          AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer)                  \
          X(QueueSubmit) X(QueueWaitIdle) X(DeviceWaitIdle) X(CreateBuffer) X(               \
              GetBufferMemoryRequirements) X(AllocateMemory) X(BindBufferMemory)             \
              X(MapMemory) X(CreateImage) X(GetImageMemoryRequirements) X(                   \
                  BindImageMemory) X(CreateImageView) X(CreateRenderPass)                    \
                  X(CreateFramebuffer) X(CreateShaderModule) X(                              \
                      CreatePipelineLayout) X(CreateGraphicsPipelines)                       \
                      X(CreateComputePipelines) X(CreateDescriptorSetLayout) X(              \
                          CreateDescriptorPool) X(AllocateDescriptorSets)                    \
                          X(UpdateDescriptorSets) X(CmdPipelineBarrier) X(                   \
                              CmdBeginRenderPass) X(CmdEndRenderPass)                        \
                              X(CmdBindPipeline) X(CmdBindVertexBuffers) X(                  \
                                  CmdSetScissor) X(CmdSetStencilWriteMask) X(CmdSetStencilReference) X(CmdDraw)        \
                                  X(CmdBindDescriptorSets) X(CmdPushConstants) X(            \
                                      CmdDispatch) X(CmdClearDepthStencilImage)              \
                                      X(CmdCopyBufferToImage) X(                             \
                                          CmdCopyImageToBuffer) X(UnmapMemory)               \
                                          X(FreeMemory) X(DestroyBuffer) X(                  \
                                              DestroyImage) X(DestroyImageView)              \
                                              X(DestroyRenderPass) X(                        \
                                                  DestroyFramebuffer) X(DestroyShaderModule) \
                                                  X(DestroyPipeline) X(                      \
                                                      DestroyPipelineLayout)                 \
                                                      X(DestroyDescriptorPool) X(            \
                                                          DestroyDescriptorSetLayout)        \
                                                          X(DestroyCommandPool) X(           \
                                                              DestroyDevice)                 \
                                                              X(DestroyInstance)
#define DECL(n) PFN_vk##n vk##n;
FUNCS(DECL)
#undef DECL
static void check(VkResult r, const char *op) {
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string(op) +
                             " VkResult=" + std::to_string(r));
}
#define VK(x) check((x), #x)
constexpr uint32_t SW = 18, SH = 18, DW = 36, DH = 36;
static uint8_t initial_stencil;
static uint8_t source(uint32_t x,uint32_t y,uint32_t s) { return uint8_t((x-1)+16*(y-1)+s*53); }
static uint8_t pattern(uint32_t x,uint32_t y,uint32_t s) { return x>=1 && x<17 && y>=1 && y<17 ? uint8_t(initial_stencil | source(x,y,s)) : initial_stencil; }
struct Buffer {
  VkBuffer b;
  VkDeviceMemory m;
  uint8_t *p;
  VkDeviceSize size;
};
struct Image {
  VkImage i;
  VkDeviceMemory m;
  VkImageView view;
};
struct Harness {
  VkInstance instance{};
  VkPhysicalDevice gpu{};
  VkDevice device{};
  VkQueue queue{};
  uint32_t family{};
  VkCommandPool pool{};
  VkCommandBuffer cmd{};
  VkPhysicalDeviceMemoryProperties mem{};
  VkFormat format{};
  uint32_t memory(uint32_t bits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (mem.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error("Required memory type unavailable");
  }
  Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer r{};
    r.size = size;
    VkBufferCreateInfo c{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    c.size = size;
    c.usage = usage;
    c.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateBuffer(device, &c, nullptr, &r.b));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, r.b, &req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    a.allocationSize = req.size;
    a.memoryTypeIndex =
        memory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK(vkAllocateMemory(device, &a, nullptr, &r.m));
    VK(vkBindBufferMemory(device, r.b, r.m, 0));
    VK(vkMapMemory(device, r.m, 0, VK_WHOLE_SIZE, 0, (void **)&r.p));
    return r;
  }
  Image image(uint32_t w, uint32_t h, VkSampleCountFlagBits samples,
              VkImageUsageFlags usage) {
    Image r{};
    VkImageCreateInfo c{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    c.imageType = VK_IMAGE_TYPE_2D;
    c.format = format;
    c.extent = {w, h, 1};
    c.mipLevels = 1;
    c.arrayLayers = 1;
    c.samples = samples;
    c.tiling = VK_IMAGE_TILING_OPTIMAL;
    c.usage = usage;
    c.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateImage(device, &c, nullptr, &r.i));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, r.i, &req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    a.allocationSize = req.size;
    a.memoryTypeIndex =
        memory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK(vkAllocateMemory(device, &a, nullptr, &r.m));
    VK(vkBindImageMemory(device, r.i, r.m, 0));
    return r;
  }
  VkImageView view(Image im, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo c{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    c.image = im.i;
    c.viewType = VK_IMAGE_VIEW_TYPE_2D;
    c.format = format;
    c.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView v;
    VK(vkCreateImageView(device, &c, nullptr, &v));
    return v;
  }
  VkShaderModule shader(const std::vector<uint32_t> &words) {
    VkShaderModuleCreateInfo c{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    c.codeSize = words.size() * 4;
    c.pCode = words.data();
    VkShaderModule m;
    VK(vkCreateShaderModule(device, &c, nullptr, &m));
    return m;
  }
  void begin() {
    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK(vkBeginCommandBuffer(cmd, &b));
  }
  void submit() {
    VK(vkEndCommandBuffer(cmd));
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;
    VK(vkQueueSubmit(queue, 1, &s, VK_NULL_HANDLE));
    VK(vkQueueWaitIdle(queue));
  }
  void barrier(Image im, VkImageLayout oldLayout, VkImageLayout newLayout,
               VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags from,
               VkPipelineStageFlags to) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = src;
    b.dstAccessMask = dst;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = im.i;
    b.subresourceRange = {
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, from, to, 0, 0, nullptr, 0, nullptr, 1, &b);
  }
  void bufferBarrier(Buffer b, VkAccessFlags src, VkAccessFlags dst,
                     VkPipelineStageFlags from, VkPipelineStageFlags to) {
    VkBufferMemoryBarrier x{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    x.srcAccessMask = src;
    x.dstAccessMask = dst;
    x.srcQueueFamilyIndex = x.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    x.buffer = b.b;
    x.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, from, to, 0, 0, nullptr, 1, &x, 0, nullptr);
  }
  bool init(VkFormat requested) {
    HMODULE dll = LoadLibraryW(L"C:\\Windows\\System32\\vulkan-1.dll");
    if (!dll)
      throw std::runtime_error("Vulkan loader unavailable");
#define LOAD(n)                                                                \
  vk##n = (PFN_vk##n)GetProcAddress(dll, "vk" #n);                             \
  if (!vk##n)                                                                  \
    throw std::runtime_error("Missing vk" #n);
    FUNCS(LOAD)
#undef LOAD
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "LB2 stencil copy GPU validation";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VK(vkCreateInstance(&ci, nullptr, &instance));
    uint32_t count = 0;
    VK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (!count)
      throw std::runtime_error("No Vulkan device");
    std::vector<VkPhysicalDevice> devices(count);
    VK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    for (auto candidate : devices) {
      uint32_t n = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
      std::vector<VkQueueFamilyProperties> qs(n);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, qs.data());
      for (uint32_t j = 0; j < n; ++j)
        if ((qs[j].queueFlags &
             (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
          gpu = candidate;
          family = j;
          break;
        }
      if (gpu)
        break;
    }
    if (!gpu)
      throw std::runtime_error("No graphics+compute queue");
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);
    printf("device=%s api=%u.%u.%u\n", props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion));
    if (props.apiVersion < VK_API_VERSION_1_2)
      throw std::runtime_error("Vulkan 1.2 required");
    for (auto f : {requested}) {
      VkImageFormatProperties p;
      auto result = vkGetPhysicalDeviceImageFormatProperties(
          gpu, f, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_SAMPLED_BIT,
          0, &p);
      if (result == VK_SUCCESS && (p.sampleCounts & VK_SAMPLE_COUNT_4_BIT)) {
        format = f;
        break;
      }
    }
    if (!format) {
      printf("SKIP format=%u: sampled 4x depth/stencil image unsupported\n",
             requested);
      vkDestroyInstance(instance, nullptr);
      FreeLibrary(dll);
      return false;
    }
    printf("format=%s (%u) samples=4 local_size=64 maxStorageBufferRange=%u\n",
           format == VK_FORMAT_D24_UNORM_S8_UINT ? "D24_UNORM_S8_UINT"
                                                 : "D32_SFLOAT_S8_UINT",
           format, props.limits.maxStorageBufferRange);
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    VK(vkCreateDevice(gpu, &di, nullptr, &device));
    vkGetDeviceQueue(device, family, 0, &queue);
    vkGetPhysicalDeviceMemoryProperties(gpu, &mem);
    VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pc.queueFamilyIndex = family;
    VK(vkCreateCommandPool(device, &pc, nullptr, &pool));
    VkCommandBufferAllocateInfo ca{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool = pool;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = 1;
    VK(vkAllocateCommandBuffers(device, &ca, &cmd));
    return true;
  }
};
static std::vector<uint32_t> graphicsShader(bool vertex) {
  spv::Builder b(0x00010000, 0, nullptr);
  b.addCapability(spv::CapabilityShader);
  b.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
  auto fn = b.makeEntryPoint("main");
  auto ep = b.addEntryPoint(vertex ? spv::ExecutionModelVertex
                                   : spv::ExecutionModelFragment,
                            fn, "main");
  if (vertex) {
    auto f = b.makeFloatType(32);
    auto v2 = b.makeVectorType(f, 2);
    auto v4 = b.makeVectorType(f, 4);
    auto in = b.createVariable(spv::NoPrecision, spv::StorageClassInput, v2,
                               "position");
    b.addDecoration(in, spv::DecorationLocation, 0);
    auto out = b.createVariable(spv::NoPrecision, spv::StorageClassOutput, v4,
                                "gl_Position");
    b.addDecoration(out, spv::DecorationBuiltIn, spv::BuiltInPosition);
    ep->addIdOperand(in);
    ep->addIdOperand(out);
    auto p = b.createLoad(in, spv::NoPrecision);
    b.createStore(b.createCompositeConstruct(
                      v4, {b.createCompositeExtract(p, f, 0),
                           b.createCompositeExtract(p, f, 1),
                           b.makeFloatConstant(0.5f), b.makeFloatConstant(1)}),
                  out);
  } else
    b.addExecutionMode(fn, spv::ExecutionModeOriginUpperLeft);
  b.makeReturn(true);
  b.leaveFunction();
  std::vector<uint32_t> words;
  b.dump(words);
  return words;
}
static std::vector<uint32_t> fragment(lb2::stencil_ones::ShaderMode mode) {
  spv::Builder b(spv::Spv_1_3,0x00080001,nullptr); b.addCapability(spv::CapabilityShader);
  b.setMemoryModel(spv::AddressingModelLogical,spv::MemoryModelGLSL450);
  auto u=b.makeUintType(32); auto st=b.makeStructType({u,u},"Arguments"); b.addDecoration(st,spv::DecorationBlock);
  b.addMemberDecoration(st,0,spv::DecorationOffset,0); b.addMemberDecoration(st,1,spv::DecorationOffset,4);
  auto push=b.createVariable(spv::NoPrecision,spv::StorageClassPushConstant,st,"args");
  auto fn=b.makeEntryPoint("main");
  auto load=[&](int i){return b.createLoad(b.createAccessChain(spv::StorageClassPushConstant,push,{b.makeIntConstant(i)}),spv::NoPrecision);};
  auto kill=Lb2BuildStencilKill(b,load(0),load(1),mode);
  spv::Builder::If branch(kill,spv::SelectionControlMaskNone,b); b.makeDiscard(); branch.makeEndIf();
  b.makeReturn(false);b.leaveFunction(); auto ep=b.addEntryPoint(spv::ExecutionModelFragment,fn,"main");
  b.addExecutionMode(fn,spv::ExecutionModeOriginUpperLeft); std::vector<uint32_t> out;b.dump(out);return out;
}
static bool run(VkFormat requested, bool split, uint8_t initial) {
  initial_stencil=initial; printf("protocol=%s initial=%u\n",split?"split":"original",initial);
  Harness h;
  if (!h.init(requested))
    return false;
  auto src = h.image(SW, SH, VK_SAMPLE_COUNT_4_BIT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT);
  auto attachment =
      h.view(src, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  auto sampled = h.view(src, VK_IMAGE_ASPECT_STENCIL_BIT);
  auto dest = h.image(DW, DH, VK_SAMPLE_COUNT_1_BIT,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  VkAttachmentDescription ad{};
  ad.format = h.format;
  ad.samples = VK_SAMPLE_COUNT_4_BIT;
  ad.loadOp = ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  ad.storeOp = ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
  ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ad.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VkAttachmentReference ref{0,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.pDepthStencilAttachment = &ref;
  VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rp.attachmentCount = 1;
  rp.pAttachments = &ad;
  rp.subpassCount = 1;
  rp.pSubpasses = &sub;
  VkRenderPass pass;
  VK(vkCreateRenderPass(h.device, &rp, nullptr, &pass));
  VkFramebufferCreateInfo fc{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fc.renderPass = pass;
  fc.attachmentCount = 1;
  fc.pAttachments = &attachment;
  fc.width = SW;
  fc.height = SH;
  fc.layers = 1;
  VkFramebuffer fb;
  VK(vkCreateFramebuffer(h.device, &fc, nullptr, &fb));
  VkPipelineLayoutCreateInfo empty{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  VkPushConstantRange gr{VK_SHADER_STAGE_FRAGMENT_BIT,0,8}; empty.pushConstantRangeCount=1; empty.pPushConstantRanges=&gr;
  VkPipelineLayout graphicsLayout;
  VK(vkCreatePipelineLayout(h.device, &empty, nullptr, &graphicsLayout));
  auto vs = h.shader(graphicsShader(true));
  VkShaderModule fs[3]={h.shader(fragment(lb2::stencil_ones::ShaderMode::kOriginal)),h.shader(fragment(lb2::stencil_ones::ShaderMode::kBulk)),h.shader(fragment(lb2::stencil_ones::ShaderMode::kResidual))};
  VkPipelineShaderStageCreateInfo stages[2]{};
  for (int i = 0; i < 2; ++i) {
    stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[i].stage =
        i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
    stages[i].module = i ? fs[0] : vs;
    stages[i].pName = "main";
  }
  VkVertexInputBindingDescription binding{0, 8, VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription attr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
  VkPipelineVertexInputStateCreateInfo vi{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &binding;
  vi.vertexAttributeDescriptionCount = 1;
  vi.pVertexAttributeDescriptions = &attr;
  VkPipelineInputAssemblyStateCreateInfo ia{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkViewport viewport{0, 0, float(SW), float(SH), 0, 1};
  VkRect2D scissor{{0, 0}, {SW, SH}};
  VkPipelineViewportStateCreateInfo vp{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = vp.scissorCount = 1;
  vp.pViewports = &viewport;
  vp.pScissors = &scissor;
  VkPipelineRasterizationStateCreateInfo rs{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1;
  VkPipelineMultisampleStateCreateInfo ms{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
  VkPipelineDepthStencilStateCreateInfo ds{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.stencilTestEnable = VK_TRUE;
  ds.front.failOp = ds.front.passOp = ds.front.depthFailOp =
      VK_STENCIL_OP_REPLACE;
  ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.front.compareMask = ds.front.writeMask = 255;
  ds.back = ds.front;
  VkPipelineColorBlendStateCreateInfo cb{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_SCISSOR,
                               VK_DYNAMIC_STATE_STENCIL_REFERENCE, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK};
  VkPipelineDynamicStateCreateInfo dy{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dy.dynamicStateCount = 3;
  dy.pDynamicStates = dynamics;
  VkGraphicsPipelineCreateInfo gp{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gp.stageCount = 2;
  gp.pStages = stages;
  gp.pVertexInputState = &vi;
  gp.pInputAssemblyState = &ia;
  gp.pViewportState = &vp;
  gp.pRasterizationState = &rs;
  gp.pMultisampleState = &ms;
  gp.pDepthStencilState = &ds;
  gp.pColorBlendState = &cb;
  gp.pDynamicState = &dy;
  gp.layout = graphicsLayout;
  gp.renderPass = pass;
  VkPipeline pipelines[3][4];
  for (unsigned mode=0;mode<3;mode++) for (uint32_t s=0;s<4;s++) {
    stages[1].module=fs[mode]; VkSampleMask mask=1u<<s;ms.pSampleMask=&mask;
    VK(vkCreateGraphicsPipelines(h.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipelines[mode][s]));
  }
  auto verifyfs=h.shader(graphicsShader(false)); stages[1].module=verifyfs;
  ms.pSampleMask=nullptr; ds.depthTestEnable=VK_TRUE; ds.depthWriteEnable=VK_FALSE;
  ds.depthCompareOp=VK_COMPARE_OP_NOT_EQUAL; ds.front.passOp=ds.back.passOp=VK_STENCIL_OP_INVERT; ds.front.depthFailOp=ds.back.depthFailOp=VK_STENCIL_OP_KEEP;
  VkPipeline depthVerify; VK(vkCreateGraphicsPipelines(h.device,VK_NULL_HANDLE,1,&gp,nullptr,&depthVerify));
  auto vertices = h.buffer(48, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  float positions[] = {-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1};
  memcpy(vertices.p, positions, sizeof positions);
  h.begin();
  h.bufferBarrier(
      vertices, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
      VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
  VkClearValue clear{}; clear.depthStencil={0.5f,initial};
  VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rb.renderPass = pass;
  rb.framebuffer = fb;
  rb.renderArea = {{0, 0}, {SW, SH}};
  rb.clearValueCount = 1;
  rb.pClearValues = &clear;
  vkCmdBeginRenderPass(h.cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
  VkDeviceSize zero = 0;
  vkCmdBindVertexBuffers(h.cmd, 0, 1, &vertices.b, &zero);
  vkCmdSetStencilReference(h.cmd,VK_STENCIL_FACE_FRONT_AND_BACK,255);
  for (uint32_t s=0;s<4;s++) for(uint32_t y=1;y<17;y++) for(uint32_t x=1;x<17;x++) {
    VkRect2D r{{int32_t(x),int32_t(y)},{1,1}};vkCmdSetScissor(h.cmd,0,1,&r);
    uint32_t args[2]={uint32_t(source(x,y,s))|0xa5c30000u,255};
    if(split) {vkCmdBindPipeline(h.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[1][s]);
      vkCmdSetStencilWriteMask(h.cmd,VK_STENCIL_FACE_FRONT_AND_BACK,255);
      vkCmdPushConstants(h.cmd,graphicsLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,8,args);vkCmdDraw(h.cmd,6,1,0,0);}
    vkCmdBindPipeline(h.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[split?2:0][s]);
    for(uint32_t bit=0;bit<8;bit++) { args[1]=1u<<bit;vkCmdSetStencilWriteMask(h.cmd,VK_STENCIL_FACE_FRONT_AND_BACK,args[1]);
      vkCmdPushConstants(h.cmd,graphicsLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,8,args);vkCmdDraw(h.cmd,6,1,0,0); }
  }
  // Invert stencil where any sample's depth differs from the original clear value.
  // Full-image CPU stencil comparison detects any inversion, including canaries.
  vkCmdBindPipeline(h.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,depthVerify);
  VkRect2D all{{0,0},{SW,SH}}; vkCmdSetScissor(h.cmd,0,1,&all);
  vkCmdSetStencilWriteMask(h.cmd,VK_STENCIL_FACE_FRONT_AND_BACK,255);
  vkCmdDraw(h.cmd,6,1,0,0);
  vkCmdEndRenderPass(h.cmd);
  h.barrier(src, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  h.submit();
  VkDescriptorSetLayout layouts[2];
  VkDescriptorSetLayoutBinding db{};
  db.binding = 1;
  db.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  db.descriptorCount = 1;
  db.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  VkDescriptorSetLayoutCreateInfo dl{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dl.bindingCount = 1;
  dl.pBindings = &db;
  VK(vkCreateDescriptorSetLayout(h.device, &dl, nullptr, &layouts[0]));
  db.binding = 0;
  db.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  VK(vkCreateDescriptorSetLayout(h.device, &dl, nullptr, &layouts[1]));
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 20};
  VkPipelineLayoutCreateInfo lc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  lc.setLayoutCount = 2;
  lc.pSetLayouts = layouts;
  lc.pushConstantRangeCount = 1;
  lc.pPushConstantRanges = &push;
  VkPipelineLayout layout;
  VK(vkCreatePipelineLayout(h.device, &lc, nullptr, &layout));
  auto module = h.shader(Lb2BuildStencilCopyShader());
  VkComputePipelineCreateInfo cp{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  cp.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              nullptr,
              0,
              VK_SHADER_STAGE_COMPUTE_BIT,
              module,
              "main",
              nullptr};
  cp.layout = layout;
  VkPipeline compute;
  VK(vkCreateComputePipelines(h.device, VK_NULL_HANDLE, 1, &cp, nullptr,
                              &compute));
  VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
  VkDescriptorPoolCreateInfo dc{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dc.maxSets = 2;
  dc.poolSizeCount = 2;
  dc.pPoolSizes = sizes;
  VkDescriptorPool descriptorPool;
  VK(vkCreateDescriptorPool(h.device, &dc, nullptr, &descriptorPool));
  VkDescriptorSetAllocateInfo da{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  da.descriptorPool = descriptorPool;
  da.descriptorSetCount = 2;
  da.pSetLayouts = layouts;
  VkDescriptorSet sets[2];
  VK(vkAllocateDescriptorSets(h.device, &da, sets));
  constexpr size_t PREFIX = 256, CAPACITY = 4096;
  auto packed =
      h.buffer(PREFIX + CAPACITY + 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  auto readStencil = h.buffer(DW * DH, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  auto readDepth = h.buffer(DW * DH * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageView = sampled;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorBufferInfo bufferInfo{packed.b, PREFIX, CAPACITY};
  VkWriteDescriptorSet writes[2]{};
  for (int i = 0; i < 2; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = sets[i];
    writes[i].dstBinding = i ? 0 : 1;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                 : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  }
  writes[0].pImageInfo = &imageInfo;
  writes[1].pBufferInfo = &bufferInfo;
  vkUpdateDescriptorSets(h.device, 2, writes, 0, nullptr);
  struct Case {
    uint32_t x, y, w, h, row;
  };
  const Case cases[] = {{0, 0, 36, 36, 9}, {1, 1, 33, 31, 9},
                        {3, 5, 1, 1, 3}, {7, 3, 17, 11, 7},
                        {2, 4, 31, 13, 8}, {35, 35, 1, 1, 1}};
  bool first = true;
  uint64_t checked = 0;
  for (auto c : cases) {
    memset(packed.p, 0xcd, size_t(packed.size));
    memset(readStencil.p, 0xee, size_t(readStencil.size));
    memset(readDepth.p, 0xee, size_t(readDepth.size));
    h.begin();
    h.bufferBarrier(packed, VK_ACCESS_HOST_WRITE_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    vkCmdBindPipeline(h.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute);
    vkCmdBindDescriptorSets(h.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 2,
                            sets, 0, nullptr);
    vkCmdPushConstants(h.cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, &c);
    vkCmdDispatch(h.cmd, (c.row * c.h + 63) / 64, 1, 1);
    h.bufferBarrier(packed, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT |
                        VK_PIPELINE_STAGE_HOST_BIT);
    h.barrier(dest,
              first ? VK_IMAGE_LAYOUT_UNDEFINED
                    : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              first ? 0 : VK_ACCESS_TRANSFER_READ_BIT,
              VK_ACCESS_TRANSFER_WRITE_BIT,
              first ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                    : VK_PIPELINE_STAGE_TRANSFER_BIT,
              VK_PIPELINE_STAGE_TRANSFER_BIT);
    first = false;
    VkClearDepthStencilValue cv{0.0f, 0x5a};
    VkImageSubresourceRange range{
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    vkCmdClearDepthStencilImage(
        h.cmd, dest.i, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &range);
    h.barrier(dest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy{};
    copy.bufferOffset = PREFIX;
    copy.bufferRowLength = c.row * 4;
    copy.imageSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1};
    copy.imageOffset = {int32_t(c.x), int32_t(c.y), 0};
    copy.imageExtent = {c.w, c.h, 1};
    vkCmdCopyBufferToImage(h.cmd, packed.b, dest.i,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    h.barrier(dest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy back{};
    back.imageSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1};
    back.imageExtent = {DW, DH, 1};
    vkCmdCopyImageToBuffer(h.cmd, dest.i, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readStencil.b, 1, &back);
    back.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkCmdCopyImageToBuffer(h.cmd, dest.i, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readDepth.b, 1, &back);
    h.bufferBarrier(readStencil, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT);
    h.bufferBarrier(readDepth, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT);
    h.submit();
    for (size_t i = 0; i < packed.size; ++i) {
      uint8_t expected = 0xcd;
      if (i >= PREFIX && i < PREFIX + c.row * c.h * 4) {
        auto n = uint32_t(i - PREFIX);
        auto y = n / (c.row * 4), x = n % (c.row * 4);
        expected = x < c.w ? pattern((c.x + x) / 2, (c.y + y) / 2,
                                     ((c.x + x) & 1) | (((c.y + y) & 1) << 1))
                           : 0;
      }
      if (packed.p[i] != expected)
        throw std::runtime_error("packed mismatch case=" + std::to_string(c.x) +
                                 "," + std::to_string(c.y) +
                                 " byte=" + std::to_string(i) +
                                 " actual=" + std::to_string(packed.p[i]) +
                                 " expected=" + std::to_string(expected));
      ++checked;
    }
    for (uint32_t y = 0; y < DH; ++y)
      for (uint32_t x = 0; x < DW; ++x) {
        bool inside = x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h;
        uint8_t expected =
            inside ? pattern(x / 2, y / 2, (x & 1) | ((y & 1) << 1)) : 0x5a;
        if (readStencil.p[y * DW + x] != expected)
          throw std::runtime_error("destination stencil mismatch");
        uint32_t depth;
        memcpy(&depth, readDepth.p + (y * DW + x) * 4, 4);
        if (h.format == VK_FORMAT_D24_UNORM_S8_UINT)
          depth &= 0xffffff;
        if (depth != 0)
          throw std::runtime_error("destination depth changed");
        checked += 5;
      }
    printf("PASS origin=(%u,%u) size=%ux%u row_words=%u "
           "packed/canaries/stencil/outside/depth\n",
           c.x, c.y, c.w, c.h, c.row);
  }
  VK(vkDeviceWaitIdle(h.device));
  // Teardown after queue completion; host mappings live through all
  // comparisons.
  vkDestroyDescriptorPool(h.device, descriptorPool, nullptr);
  for (auto &row : pipelines) for(auto p : row) vkDestroyPipeline(h.device, p, nullptr);
  vkDestroyPipeline(h.device, compute, nullptr);
  vkDestroyPipeline(h.device, depthVerify, nullptr);
  for (auto m : {vs, fs[0], fs[1], fs[2], verifyfs, module})
    vkDestroyShaderModule(h.device, m, nullptr);
  for (auto l : {graphicsLayout, layout})
    vkDestroyPipelineLayout(h.device, l, nullptr);
  for (auto l : layouts)
    vkDestroyDescriptorSetLayout(h.device, l, nullptr);
  vkDestroyFramebuffer(h.device, fb, nullptr);
  vkDestroyRenderPass(h.device, pass, nullptr);
  for (auto v : {attachment, sampled})
    vkDestroyImageView(h.device, v, nullptr);
  for (auto im : {src, dest}) {
    vkDestroyImage(h.device, im.i, nullptr);
    vkFreeMemory(h.device, im.m, nullptr);
  }
  for (auto b : {vertices, packed, readStencil, readDepth}) {
    vkUnmapMemory(h.device, b.m);
    vkDestroyBuffer(h.device, b.b, nullptr);
    vkFreeMemory(h.device, b.m, nullptr);
  }
  vkDestroyCommandPool(h.device, h.pool, nullptr);
  vkDestroyDevice(h.device, nullptr);
  vkDestroyInstance(h.instance, nullptr);
  printf("PASS GPU validation format=%u: %zu cases, %llu compared bytes\n",
         requested, std::size(cases), (unsigned long long)checked);
  return true;
}
int main(int argc, char **argv) {
  try {
    if (argc > 1) {
      auto words = Lb2BuildStencilCopyShader();
      std::ofstream artifact(argv[1], std::ios::binary | std::ios::trunc);
      artifact.write(reinterpret_cast<const char *>(words.data()),
                     words.size() * sizeof(uint32_t));
      artifact.close();
      if (!artifact)
        throw std::runtime_error("Could not write production kernel SPIR-V");
      printf("production kernel SPIR-V=%s bytes=%zu\n", argv[1],
             words.size() * sizeof(uint32_t));
    }
    if(argc>2) for(unsigned i=0;i<3;i++) {auto words=fragment(static_cast<lb2::stencil_ones::ShaderMode>(i));
      std::ofstream f(std::string(argv[2])+"/fragment_"+std::to_string(i)+".spv",std::ios::binary);f.write((const char*)words.data(),words.size()*4);}
    unsigned formats = 0;
    for (auto initial : {uint8_t(0),uint8_t(0x12)}) {
      formats += run(VK_FORMAT_D24_UNORM_S8_UINT, false,initial);
      formats += run(VK_FORMAT_D24_UNORM_S8_UINT, true,initial);
      formats += run(VK_FORMAT_D32_SFLOAT_S8_UINT, false,initial);
      formats += run(VK_FORMAT_D32_SFLOAT_S8_UINT, true,initial);
    }
    if (!formats)
      throw std::runtime_error("Neither depth/stencil format supported");
    printf("PASS protocol/format/initial runs=%u\n", formats);
    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
  }
}





