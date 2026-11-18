#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define VK_NO_PROTOTYPES
#include "lb2_transfer_same_origin_shader.h"
#include <SPIRV/SpvBuilder.h>


#include <algorithm>
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
          AllocateCommandBuffers) X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer)                  \
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
                                  CmdSetScissor) X(CmdSetStencilReference) X(CmdDraw)        \
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
constexpr uint32_t SW = 20, SH = 12, DW = 40, DH = 24;
static uint8_t pattern(uint32_t x, uint32_t y, uint32_t s) {
  return uint8_t(x * 37 + y * 71 + s * 53 + 19);
}
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
    VK(vkResetCommandBuffer(cmd, 0));
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
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
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
    app.pApplicationName = "LB2 direct color GPU validation";
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
    format = requested;
    VkImageFormatProperties ip{};
    VK(vkGetPhysicalDeviceImageFormatProperties(gpu, format, VK_IMAGE_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ip));
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
static std::vector<uint32_t> build() {
 spv::Builder b(spv::Spv_1_3,0x00080001,nullptr);b.addCapability(spv::CapabilityShader);b.setMemoryModel(spv::AddressingModelLogical,spv::MemoryModelGLSL450);
 auto u=b.makeUintType(32),u3=b.makeVectorType(u,3);auto array=b.makeRuntimeArray(u);b.addDecoration(array,spv::DecorationArrayStride,4);
 auto st=b.makeStructType({array},"Words");b.addDecoration(st,spv::DecorationBlock);b.addMemberDecoration(st,0,spv::DecorationOffset,0);
 auto data=b.createVariable(spv::NoPrecision,spv::StorageClassStorageBuffer,st,"data");b.addDecoration(data,spv::DecorationDescriptorSet,0);b.addDecoration(data,spv::DecorationBinding,0);
 auto global=b.createVariable(spv::NoPrecision,spv::StorageClassInput,u3,"global");b.addDecoration(global,spv::DecorationBuiltIn,spv::BuiltInGlobalInvocationId);
 auto main=b.makeEntryPoint("main");auto id=b.createCompositeExtract(b.createLoad(global,spv::NoPrecision),u,0);auto index=b.createBinOp(spv::OpIMul,u,id,b.makeUintConstant(5));
 auto ptr=[&](unsigned i){auto idx=b.createBinOp(spv::OpIAdd,u,index,b.makeUintConstant(i));return b.createAccessChain(spv::StorageClassStorageBuffer,data,{b.makeUintConstant(0),idx});};
 auto result=Lb2BuildTransferSameOriginCoordinates(b,b.createLoad(ptr(0),spv::NoPrecision),b.createLoad(ptr(1),spv::NoPrecision),b.createLoad(ptr(2),spv::NoPrecision));
 b.createStore(result.x,ptr(3));b.createStore(result.y,ptr(4));b.makeReturn(false);b.leaveFunction();
 auto ep=b.addEntryPoint(spv::ExecutionModelGLCompute,main,"main");ep->addIdOperand(global);b.addExecutionMode(main,spv::ExecutionModeLocalSize,64,1,1);std::vector<uint32_t> code;b.dump(code);return code;
}
int main(int argc,char** argv) {try {
 if(argc!=2)throw std::runtime_error("usage: transfer-same-origin-gpu kernel.spv");auto code=build();std::ofstream f(argv[1],std::ios::binary);f.write((const char*)code.data(),code.size()*4);f.close();
 Harness h;h.init(VK_FORMAT_R8G8B8A8_UNORM);constexpr uint32_t N=8192;auto buf=h.buffer(N*5*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
 VkDescriptorSetLayoutBinding binding{};binding.binding=0;binding.descriptorCount=1;binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
 VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&binding;VkDescriptorSetLayout sl;VK(vkCreateDescriptorSetLayout(h.device,&li,nullptr,&sl));
 VkPipelineLayoutCreateInfo lc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};lc.setLayoutCount=1;lc.pSetLayouts=&sl;VkPipelineLayout layout;VK(vkCreatePipelineLayout(h.device,&lc,nullptr,&layout));
 VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=1;pi.poolSizeCount=1;pi.pPoolSizes=&ps;VkDescriptorPool pool;VK(vkCreateDescriptorPool(h.device,&pi,nullptr,&pool));
 VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=pool;ai.descriptorSetCount=1;ai.pSetLayouts=&sl;VkDescriptorSet ds;VK(vkAllocateDescriptorSets(h.device,&ai,&ds));
 VkDescriptorBufferInfo bi{buf.b,0,buf.size};VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};wr.dstSet=ds;wr.dstBinding=0;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;wr.pBufferInfo=&bi;vkUpdateDescriptorSets(h.device,1,&wr,0,nullptr);
 auto module=h.shader(code);VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,module,"main",nullptr};ci.layout=layout;VkPipeline pipeline;VK(vkCreateComputePipelines(h.device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline));
 const uint32_t pitches[]={1,2,3,5,11,13,16,32,64,127,511,1023};uint64_t checked=0;auto words=(uint32_t*)buf.p;
 for(uint32_t pitch:pitches) {uint32_t rng=0x834c1267;
  for(uint32_t i=0;i<N;i++) {rng=rng*1664525u+1013904223u;uint32_t tile;
   if(i<256)tile=(i/32)%pitch;else if(i<512)tile=2047-(i/32)%(2048%pitch?2048%pitch:pitch);else tile=rng%2048;
   uint32_t tx=tile%pitch,ty=tile/pitch;const uint32_t edges[]={0,1,18,19,20,21,38,39};uint32_t lx=edges[(i/4)%8];uint32_t ly=(i/32)%8;
   words[i*5]=tx*40+lx;words[i*5+1]=ty*8+ly;words[i*5+2]=i%4;words[i*5+3]=words[i*5+4]=0xdeadbeef; }
  h.begin();h.bufferBarrier(buf,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  vkCmdBindPipeline(h.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(h.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&ds,0,nullptr);vkCmdDispatch(h.cmd,N/64,1,1);
  h.bufferBarrier(buf,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT);h.submit();
  for(uint32_t i=0;i<N;i++){uint32_t x=words[i*5],y=words[i*5+1],sample=words[i*5+2];uint32_t dx=x/40,dy=y/8,tile=(dy*pitch+dx)&2047,stx=tile%pitch,sty=tile/pitch;
   uint32_t local=(x%40)*2+(sample&1);local=local<40?local+40:local-40;uint32_t sx=stx*80+local,sy=sty*8+y%8;
   if(words[i*5+3]!=sx||words[i*5+4]!=sy){printf("FAIL pitch=%u x=%u y=%u sample=%u got=%u,%u expected=%u,%u\n",pitch,x,y,sample,words[i*5+3],words[i*5+4],sx,sy);return 1;}}
  checked+=N;printf("PASS pitch=%u coordinate_pairs=%u\n",pitch,N);
 }
 printf("PASS coordinate_pairs=%llu scalar_results=%llu pitches=%zu\n",(unsigned long long)checked,(unsigned long long)(checked*2),std::size(pitches));return 0;
 }catch(const std::exception& e){fprintf(stderr,"ERROR %s\n",e.what());return 1;}}
